# Cardputer Adv dual-screen — Dirt routing contract v1

INT **ST7789 240×135** is navigation. EXT **ILI9341 320×240** is mining.
This is **not** a 1:1 blit of the built-in 240×135 frames onto the porkchop panel.

Stock PlatformIO env `M5-Cardputer-Adv` is unchanged (keyboard + Launcher path from PR #1). Dual is a **separate** env and a **separate** PR.

## Routing

| Surface | API | Hardware | Content |
| --- | --- | --- | --- |
| INT nav | `nerd_nav()` | ST7789 via stock TFT_eSPI **Setup215** / `tDisplayV1Driver` | Keyboard + G0 cyclic nav, loading, setup, Wi‑Fi portal. Backlight and rotate stay here. |
| EXT mining | `nerd_mining()` | ILI9341 320×240 on HSPI | Hashrate / stats / clock / network / price at **native** 320×240, themed to match NerdMiner (cream `0xDEDB`, orange hashrate, dark panels). |
| Fallback | `nerd_mining()` → `nerd_nav()` | INT only | EXT init fail or `-DNERDMINER_DUAL_FORCE_INT=1`. Full V1 cyclic screens on INT, same as stock. |

When EXT is up, INT draws a compact **NAV HUD** (current view + key hints) instead of the 240×135 mining bitmaps.

## Pins (locked)

**INT LCD** (separate SPI, Setup215 — do not retarget TFT_eSPI):

| Signal | GPIO |
| --- | --- |
| MOSI | 35 |
| SCLK | 36 |
| CS | 37 |
| DC | 34 |
| RST | 33 |
| BL | 38 |

**EXT ILI9341 porkchop** (HSPI, write-mostly):

| Signal | GPIO |
| --- | --- |
| CS | 5 |
| DC | 6 |
| RST | 3 |
| SCK | 40 |
| MOSI | 14 |

**SD** shares that HSPI bus:

| Signal | GPIO |
| --- | --- |
| CS | 12 |
| MOSI | 14 |
| CLK | 40 |
| MISO | 39 |

Rules:

- Quiesce **EXT only** before any SD transaction (`nerd_quiesce_ext()` → GPIO5 idle **HIGH**). Do not pause the INT SPI.
- After `SDCard::terminate()`, HSPI stays up so mining can keep using EXT.
- Cap **LoRa / Hydra** Units use G5 / G3 / G6. They are **mutually exclusive** with the porkchop. Building dual with `HAS_LORA` / `HYDRA` is a compile error.

**TCA8418** (unchanged v1.2): I2C `0x34`, SDA=8, SCL=9, INT=11. `;` next; `p` / `,` prev; long `KEY_BACKSPACE` `0x2A` reset.

## Build

```bash
pio run -e M5-Cardputer-Adv
pio run -e M5-Cardputer-Adv-dual
```

| Env | Flag | Use |
| --- | --- | --- |
| `M5-Cardputer-Adv` | (none) | Stock Adv, no EXT driver |
| `M5-Cardputer-Adv-dual` | `-DNERDMINER_DUAL_SCREEN=1` | INT nav + EXT mining |

Optional flags on the dual env:

| Flag | Effect |
| --- | --- |
| `-DNERDMINER_DUAL_ASSUME_EXT=1` | Default. Porkchop pin list has no EXT MISO, so ID probe is best-effort. |
| `-DNERDMINER_DUAL_FORCE_INT=1` | Force INT-only fallback (no EXT traffic). |

## Launcher (app-only `0xE9`)

Same recipe as [cardputer-adv-launcher.md](cardputer-adv-launcher.md). Dual has its **own** app image:

| File | Launcher? |
| --- | --- |
| `firmware/launcher/NerdMiner_v2_M5-Cardputer-Adv-dual.bin` | **Yes** — SD / OTA Install |
| `.pio/build/M5-Cardputer-Adv-dual/firmware.bin` | **Yes** |
| `firmware/<version>/M5-Cardputer-Adv-dual_firmware.bin` | **Yes** |
| `firmware/<version>/M5-Cardputer-Adv-dual_factory.bin` | **No** — wipes Launcher |

```bash
pio run -e M5-Cardputer-Adv-dual
python3 scripts/export_launcher_bin.py --env M5-Cardputer-Adv-dual
```

`scripts/export_launcher_bin.py` still rejects non-`0xE9` files and ESP32-S3 factory merges (`0xE9` at `0` **and** `0x10000`).

## Field risks

- **No porkchop, dual firmware:** GPIO 5/3/6 still toggle. With `ASSUME_EXT` (default) INT shows the nav HUD and mining is drawn to a missing panel — flash **stock** `M5-Cardputer-Adv` if you have no EXT glass.
- **Write-only panel:** ID read on MISO39 is unreliable; that is why `ASSUME_EXT` is on. Use `FORCE_INT` if the bus must stay quiet.
- **SD + EXT:** If EXT CS is left low, SD enumerates fail. Boot always idles GPIO5 HIGH; `loadConfigFile` / `initSDcard` quiesce EXT first.
- **LoRa / Hydra on the Grove/hat pins:** Do not stack with the porkchop.
- **Color order:** Cheap ILI9341 modules may swap R/B. Driver uses BGR MADCTL; swap in `ili9341Ext.cpp` if a panel looks inverted.
- **Keyboard / Launcher:** Dual env still compiles TCA8418 v1.2 and the app-only export. Stock env is the safe path if you only want PR #1 behavior.
