# Cardputer Adv dual-screen — Dirt routing contract v1 / v2 / v2.1b / v2.2 / v2.2b / v2.3

INT **ST7789 240×135** is navigation. EXT **ILI9341 320×240** is mining.
This is **not** a 1:1 blit of the built-in 240×135 frames onto the porkchop panel.

Stock PlatformIO env `M5-Cardputer-Adv` is unchanged (keyboard + Launcher path from PR #1). Dual is a **separate** env and a **separate** PR.

## Routing

| Surface | API | Hardware | Content |
| --- | --- | --- | --- |
| INT nav | `nerd_nav()` | ST7789 via stock TFT_eSPI **Setup215** / `tDisplayV1Driver` | Stock V1 cyclic screens (MinerScreen logo / BLOCK TEMPLATES / BEST DIFFICULTY / 32BITS SHARES / VALID BLOCKS / KH/s / uptime). Keyboard + G0 cycle them. Loading, setup, portal, backlight, rotate stay here. |
| EXT mining | `nerd_mining()` | ILI9341 320×240 on HSPI | Same stock V1 visual scheme, scaled to 320×240, with a denser mining-goods band. Not a 1:1 blit and not a clone of INT. |
| Fallback | `nerd_mining()` → `nerd_nav()` | INT only | EXT init fail or `-DNERDMINER_DUAL_FORCE_INT=1`. Full V1 cyclic screens on INT, same as stock. |

When EXT is up, INT and EXT share the **stock V1 visual scheme** (logo, chrome, DigitalNumbers / `0xDEDB` like single-screen NerdMiner) but **different info** — not identical clones:

- **INT (240×135):** nav/status only. Dense stock tDisplayV1 screens. Dirty-only redraw (index / view / `s_intDirty`). No 1 Hz full V1 goods paint. `mElapsed` forced to 0. No custom / yellowish HUD (`C_ORANGE` banned). No oversized “big words + big number” chrome.
- **EXT (320×240):** mining goods. Same stock V1 frame (composed via `tDisplayV1ComposeCyclic` + DigitalNumbers) scaled to the porkchop, plus a denser mining-goods band in `0xDEDB`. Live goods stay here.

No stripped NETWORK-only debug chrome. No per-tick INT `fillSprite` except the stock screen's own `pushSprite`.

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

**TCA8418** (v1.2 + HWbot arrows): I2C `0x34`, SDA=8, SCL=9, INT=11. Bare `;` / `.` next; **Fn+`.`** = Down → next; **Fn+`;`** = Up → prev; `p` / `,` prev; long `KEY_BACKSPACE` `0x2A` reset. No extra GPIO.

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
| `-DEXT_TFT_MADCTL=0xE8` | Override boot MADCTL for A/B. Default is `MY|MV|BGR|MH` = `0xAC`. Rollbacks: `0x28`, `0xE8`, `0xA8`, `0x68`. If L/R returns after `0xAC`, try `0x38` (`MV|ML|BGR`). |

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

## EXT redraw / shared-bus notes

`runMonitor()` refreshes mining screens at ~1 Hz (`mining.cpp` when `mElapsed >= 1000`).

**Root cause (Dirt film-slide):** every cyclic EXT screen used to start with `ili9341ExtFillScreen(C_BG)` (`C_BG = 0x1082`, near-black). That is a full 320×240 `RAMWR` in scan order, then the same widgets are painted back. Field look: a continuous top-down black band that blanks and restores **stable** graphics — not random sparkle.

INT V1 does not do this: it composes in a sprite and `pushSprite`s once.

`nerd_quiesce_ext()` only idles EXT CS (GPIO5 **HIGH**) and now waits until `nerd_ext_end_frame()`. ILI9341 GRAM is retained. SD is boot-time (`initSDcard` / `loadConfigFile`) then `terminate()`.

Locked redraw rules:

- **Ban** full clear on steady cyclic refresh
- Full `fillScreen` **only** when the cyclic screen index changes
- Live stats = dirty-field updates (no `fillContentBand`, no mid-frame blank)
- `nerd_quiesce_ext` only between complete frames

Also: pixel bursts use `SPI.writeBytes`; EXT `begin(..., ss=-1)`; SD CS held HIGH during EXT transactions.

## Field retest (EXT wipe)

Flash the **new** dual Launcher app-only bin (`0xE9`), not the factory merge:

```bash
pio run -e M5-Cardputer-Adv-dual
python3 scripts/export_launcher_bin.py --env M5-Cardputer-Adv-dual
```

Install `firmware/launcher/NerdMiner_v2_M5-Cardputer-Adv-dual.bin` via SD / OTA Install.

On the dirt unit (porkchop EXT wired, dual bin, SD present):

1. Boot. INT should show the NAV HUD. EXT should show the MINING 320×240 layout (hashrate / shares / …).
2. Leave MINING on EXT for at least 60 seconds. Hashrate / shares / uptime should update **in place**. There must be **no** repeating top-down black (or near-black) wipe.
3. Press `;` to CLOCK, then NETWORK, then PRICE. A single fast flash on the switch is OK. After the new view is up, it must stay stable between 1 Hz updates.
4. Press `p` or `,` to walk back. Same: no periodic wipe.
5. INT must keep the selected cyclic view in sync (stock miner/clock/network/price chrome, not a dual-HUD list). `r` rotate and `b` backlight stay on INT. Hold `KEY_BACKSPACE` still resets.
6. Confirm boot still talks to SD (config load or “No config file” — no hang, no EXT-stuck-low SD fail).

## Field retest (contract v2 / v2.1 / v2.2 / v2.3)

Same Launcher app-only flash as above. Wipe (v1) must stay **PASS**. Then:

1. **EXT orientation (v2.3):** MINING/CLOCK/NETWORK/PRICE on the porkchop must read left-to-right and right-side-up, not mirrored, reversed, or upside-down. MADCTL is boot-only `0xAC` (`MY|MV|BGR|MH`). Field FAIL on tip `e54ac1c` with boot `0x28` (`MV|BGR`) — image upside-down only (not L/R mirrored). Earlier FAIL on tip `976c96a` with boot `0xE8` (`MX|MY|MV|BGR`) — still mirrored/reversed. Prior field photo on `0xA8` was also L/R mirrored. A/B rollbacks: `-DEXT_TFT_MADCTL=0x28` / `0xE8` / `0xA8` / `0x68`. If L/R returns after `0xAC`, try `-DEXT_TFT_MADCTL=0x38` (`MV|ML|BGR`).
2. **Down key:** **Fn+`.`** moves MINING → CLOCK → NETWORK → PRICE. Bare `;` / `.` still next (v1.2). **Fn+`;`** prev. `p` / `,` / Backspace / G0 unchanged.
3. **INT lag / dirty-only:** After nav, INT updates within about one monitor tick (~100 ms). No 1 Hz full V1 goods paint. No per-tick `fillSprite`.
4. **Stock chrome (v2.1b / v2.2b):** INT looks like stock single-screen NerdMiner (logo, clock, BLOCK TEMPLATES / BEST DIFFICULTY / 32BITS SHARES / VALID BLOCKS, KH/s, uptime, gauge, DigitalNumbers / `0xDEDB`). No `C_ORANGE` / custom yellow HUD. EXT composes the same V1 screens (live `mElapsed`) and scale-blits them, plus a `0xDEDB` goods band. Not identical clones.

If a wipe remains, note whether it is every second (redraw) or only around SD/boot (bus). Serial `>>> EXT miner|clock|global|price` marks each 1 Hz paint.

## Field risks

- **No porkchop, dual firmware:** GPIO 5/3/6 still toggle. With `ASSUME_EXT` (default) INT shows the nav HUD and mining is drawn to a missing panel — flash **stock** `M5-Cardputer-Adv` if you have no EXT glass.
- **Write-only panel:** ID read on MISO39 is unreliable; that is why `ASSUME_EXT` is on. Use `FORCE_INT` if the bus must stay quiet.
- **SD + EXT:** If EXT CS is left low, SD enumerates fail. Boot always idles GPIO5 HIGH; `loadConfigFile` / `initSDcard` quiesce EXT first.
- **LoRa / Hydra on the Grove/hat pins:** Do not stack with the porkchop.
- **Color order:** Cheap ILI9341 modules may swap R/B. Driver uses BGR MADCTL; swap in `ili9341Ext.cpp` if a panel looks inverted.
- **EXT orientation (v2.3):** MADCTL is written **once at boot**: `MY|MV|BGR|MH` = `0xAC`. Field FAIL on `0x28` (upside-down only, not L/R mirrored). Earlier `0xE8` still mirrored/reversed; `0xA8` was L/R mirrored. BGR kept. Do not rewrite mid-run. Override `-DEXT_TFT_MADCTL=` for A/B; rollbacks are `0x28`, `0xE8`, `0xA8`, `0x68`. If L/R returns, try `0x38` (`MV|ML|BGR`).
- **EXT switch (v2.3+):** On cyclic index change only: one `FillScreen` + one stock V1 compose/scale-blit, then goods-band labels once. Steady 1 Hz ticks are dirty-field goods only — no second full compose or multi-pass clear.
- **Keyboard / Launcher:** Dual env still compiles TCA8418 v1.2 and the app-only export. Stock env is the safe path if you only want PR #1 behavior.
