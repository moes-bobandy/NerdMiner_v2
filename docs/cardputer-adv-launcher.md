# M5Stack Cardputer Adv — Launcher app-only firmware

This fork targets **Cardputer Adv** (Stamp-S3A, ST7789 240×135, TCA8418 keyboard, SD on HSPI CS12/MOSI14/CLK40/MISO39).

Porkchop **dual-screen** (INT nav + EXT ILI9341 mining) is a separate env: `M5-Cardputer-Adv-dual`. Same Launcher rules, different bin. See [cardputer-adv-dual-screen.md](cardputer-adv-dual-screen.md).

Install **[bmorcelli Launcher](https://github.com/bmorcelli/Launcher)** first (`m5stack-cardputer-adv` env / matching web flasher target). Then install NerdMiner as an **app-only** `.bin` from SD or OTA. That keeps Launcher in place.

## Which file to install

| File | What it is | Use with Launcher? |
| --- | --- | --- |
| `firmware/launcher/NerdMiner_v2_M5-Cardputer-Adv.bin` | ESP **app image**, first byte **`0xE9`** | **Yes** — SD / OTA Install |
| `firmware/launcher/NerdMiner_v2_M5-Cardputer-Adv-dual.bin` | Dual-screen app image (`M5-Cardputer-Adv-dual`) | **Yes** — SD / OTA Install |
| `.pio/build/M5-Cardputer-Adv/firmware.bin` | Same app image (PlatformIO output) | **Yes** |
| `.pio/build/M5-Cardputer-Adv-dual/firmware.bin` | Dual-screen PlatformIO app image | **Yes** |
| `firmware/<version>/M5-Cardputer-Adv_firmware.bin` | Same app image (post-build copy) | **Yes** |
| `firmware/<version>/M5-Cardputer-Adv-dual_firmware.bin` | Dual-screen post-build app image | **Yes** |
| `firmware/<version>/M5-Cardputer-Adv_factory.bin` | Merged flash (bootloader + partitions + app @ 0x0) | **No** — overwrites Launcher |
| `firmware/<version>/M5-Cardputer-Adv-dual_factory.bin` | Dual-screen factory merge | **No** — overwrites Launcher |

On ESP32-S3 the **bootloader also starts with `0xE9`** and sits at flash offset `0x0`, so a merged factory file can look like an app image if you only check the first byte. Distinguishing checks:

- App-only `firmware.bin`: `0xE9` at offset `0`, **no** second image at `0x10000`
- Factory merge: `0xE9` at `0` (bootloader) **and** `0xE9` at `0x10000` (the real app)

`scripts/export_launcher_bin.py` requires `0xE9` at offset 0 and **rejects** files that also have `0xE9` at `0x10000`. Launcher writes a true app image into an OTA partition and leaves Launcher itself installed.

Check the magic locally:

```bash
python3 -c "p='firmware/launcher/NerdMiner_v2_M5-Cardputer-Adv.bin'; print(hex(open(p,'rb').read(1)[0]))"
# expect: 0xe9
```

## Build (PlatformIO)

Same style as the rest of this repo (`platformio.ini` env `M5-Cardputer-Adv`):

```bash
pio run -e M5-Cardputer-Adv
pio run -e M5-Cardputer-Adv-dual   # porkchop EXT ILI9341; see dual-screen doc
```

After a successful compile, `post_build_merge.py` copies the app image to:

- `firmware/<git-describe>/M5-Cardputer-Adv_firmware.bin`
- `firmware/launcher/NerdMiner_v2_M5-Cardputer-Adv.bin`

You can also export (or re-export) the Launcher path without rebuilding:

```bash
python3 scripts/export_launcher_bin.py
python3 scripts/export_launcher_bin.py --env M5-Cardputer-Adv-dual
```

The script refuses any file that does not start with `0xE9`.

### CI / artifacts

`.github/workflows/release.yml` runs `pio run` (all `default_envs`, including `M5-Cardputer-Adv`) and uploads the `firmware/` tree. Look for:

- `firmware/launcher/NerdMiner_v2_M5-Cardputer-Adv.bin`
- `firmware/<version>/M5-Cardputer-Adv_firmware.bin`

## Install on Cardputer Adv via Launcher

1. Flash **Launcher** for Cardputer Adv (USB / web flasher). Do this once.
2. Format a microSD card **FAT32**. Copy `NerdMiner_v2_M5-Cardputer-Adv.bin` onto it (root or any folder Launcher can browse).
3. Insert the SD card, boot into Launcher.
4. Open **SD**, select the `.bin`, choose **Install**.
5. Reboot into NerdMiner when Launcher offers it.

Optional: Launcher **OTA** can install the same app-only URL if you host the `0xE9` file (GitHub release asset, etc.). Still do **not** publish the factory merge as the Launcher payload.

First-boot Wi‑Fi / pool setup is the usual NerdMiner AP (`NerdMinerAP` / `MineYourCoins`), or an SD `config.json` (see main README). On Adv you can also **hold Enter, C, or W** while NerdMiner starts to force the config portal.

## Keyboard UI (TCA8418)

Stock upstream Adv support only wired **G0** as a one-button device. This fork drives the **TCA8418** (I2C `0x34`, SDA=8, SCL=9, INT=11) so the keyboard can navigate the miner UI.

| Key | Action |
| --- | --- |
| Enter, Space, `n`, `.`, `/`, `;` | Next screen |
| `p`, `,` | Previous screen |
| Short tap Backspace (`KEY_BACKSPACE` / HID `0x2A`) | Previous screen |
| `1`–`4` | Jump to cyclic screen 0–3 |
| `r` | Rotate display |
| `b`, Tab, Fn+`` ` `` | Toggle backlight |
| Hold Backspace (`KEY_BACKSPACE` / HID `0x2A`) 5 seconds | Reset config and reboot |
| Hold Enter / `c` / `w` at boot | Open Wi‑Fi config portal |
| G0 (BOOT) | Same as a one-button device (click = next, long-press = reset) |

G0 still works if the keyboard chip is missing; Serial will log `TCA8418 not found at 0x34`.
