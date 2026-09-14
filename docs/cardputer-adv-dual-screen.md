# Cardputer Adv dual-screen — Dirt routing contract v1 / v2 / v2.1b / v2.2 / v2.2b / v2.5 / v2.6

INT **ST7789 240×135** is navigation. EXT **ILI9341 320×240** is mining.
This is **not** a 1:1 blit of the built-in 240×135 frames onto the porkchop panel.

Stock PlatformIO env `M5-Cardputer-Adv` is unchanged (keyboard + Launcher path from PR #1). Dual is a **separate** env and a **separate** PR.

## Routing

| Surface | API | Hardware | Content |
| --- | --- | --- | --- |
| INT nav | `nerd_nav()` | ST7789 via stock TFT_eSPI **Setup215** / `tDisplayV1Driver` | Stock V1 cyclic screens (MinerScreen logo / BLOCK TEMPLATES / BEST DIFFICULTY / 32BITS SHARES / VALID BLOCKS / KH/s / uptime). Keyboard + G0 cycle them. Loading, setup, portal, backlight, rotate stay here. |
| EXT mining | `nerd_mining()` | ILI9341 320×240 on HSPI | Same stock V1 visual scheme, scaled aspect-preserving (320×180, centered). Not a 1:1 blit, not a clone of INT, and **no goods-band overlay**. |
| Fallback | `nerd_mining()` → `nerd_nav()` | INT only | EXT init fail or `-DNERDMINER_DUAL_FORCE_INT=1`. Full V1 cyclic screens on INT, same as stock. |

When EXT is up, INT and EXT share the **stock V1 visual scheme** (logo, chrome, DigitalNumbers / `0xDEDB` like single-screen NerdMiner) but **different info** — not identical clones:

- **INT (240×135):** nav/status only, with live ticks. Dense stock tDisplayV1 screens painted on **live 1 Hz `mElapsed`** (plus dirty/view for nav). Do **not** force `mElapsed==0` — that froze DigitalNumbers on zeros and poisoned KH/s (`0/0` NaN in the averager). No extra debug overlay (`WIFI 0KH 12s` / Serial HUD banned). No custom / yellowish HUD (`C_ORANGE` banned). Loading version stays inside 135 px (not leftover T-Display `y=147`).
- **EXT (320×240):** mining. Same stock V1 frame (composed via `tDisplayV1ComposeCyclic` + DigitalNumbers) scaled aspect-preserving to 320×180 and **centered** on the porkchop **every 1 Hz tick** (`blitLiveStock` — not index-only, never re-compose with `mElapsed==0`). **No goods-band overlay** under the art — field v2.5 painted extra labels/uptime below the chrome (looked like debug numbers off-layout). Stock V1 chrome only.

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
| `-DEXT_TFT_MADCTL=0xE8` | Override boot MADCTL for A/B. Default is `MV|BGR` = `0x28` (no MX, no MY, no MH, no ML). Field FAIL: `MX|MY|MV|BGR` = `0xE8` still mirrored. Do **not** set MX to fix L/R — that re-broke Y. Rollbacks: `0xE8`, `0xA8`, `0x68`, `0xAC`, `0x38`. |
| `-DEXT_TFT_SW_FLIP_Y=0` | Disable software vertical flip of the EXT write path (default **1**). |
| `-DEXT_TFT_SW_FLIP_X=0` | Disable software horizontal flip (mirror each row) of the EXT write path (default **1**). |

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

**Portal must not be skipped.** Dual EXT init, SD HSPI quiesce, and the mining loop must not bypass `init_WifiManager` / `NerdMinerAP`. Boot-held Enter / C / W (drain-while-held, do not `flushFifo` a still-held key) and G0 during the loading splash force the portal. After Save/Connect (contract **v1.2**): persist, clear sticky `g_wantsConfig`, arm STA-first (**NVS + RTC + SPIFFS `/sta_first`**). **Do not `ESP.restart()`** — v1.1 always restarted, leftover Enter/C/W/G0 re-latched in `cardputerKeyboardBegin()`, and SPIFFS `/sta_first` lost if `SPIFFS.begin(true)` formatted (dirt bounce to WAITING CONFIG). Keyboard begin peeks STA-first **before** `drainFifo`. Show Connecting (`initScreen` / `NM_Connecting`) only. If `WL_CONNECTED`, proceed to mining. If STA fails after a **real** connect timeout, then the config portal may open again. Sticky force-portal is **wipe-only** (`/force_portal`). Connect-fail must **not** arm `/force_portal`. SD `config.json` remains first-boot fallback when the portal is **not** forced and STA-first is **not** armed. Mining tasks start only after Wi‑Fi setup. No INT menu list in this firmware. Portal stays INT-only (`nerd_nav()` / `drawSetupScreen`).

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
7. **Portal:** Hold Enter (or C / W / G0 during splash) on a dual Launcher bin — INT must show setup and `NerdMinerAP` / `MineYourCoins` must come up. Do **not** skip into mining. Hold Backspace 5s after a saved config: Wi‑Fi must clear and the portal must return. SD `config.json` still applies on a normal first boot without a force-portal hold.
8. **Portal Save/Connect (v1.2):** Save from a forced portal must **not** bounce back to the config QR / WAITING CONFIG. Connecting (`initScreen`) only. Good password → mining (no restart). Bad password → config QR only after the STA connect timeout (~40s), not instantly. Leftover Enter/C/W/G0 is ignored while STA-first is armed (NVS/RTC, not SPIFFS-only). Wipe `/force_portal` still opens the portal immediately. Boot-hold Enter still opens the portal when STA-first is **not** armed. Do **not** arm `/force_portal` on connect fail.

## Field retest (contract v2 / v2.1 / v2.2)

Same Launcher app-only flash as above. Wipe (v1) must stay **PASS**. Then:

1. **EXT orientation (v2.6):** MINING/CLOCK/NETWORK/PRICE on the porkchop must read left-to-right **and** right-side-up. MADCTL stays boot-only `0x28` (`MV|BGR` — no MX, no MY, no MH, no ML). Software **X+Y** in `ili9341Ext` (`extFlipX` + `extFlipY` + reverse columns and rows on push) fixes 0x28 upside-down (v2.5) **and** the remaining L/R mirror. Do **not** flip X/Y with MADCTL bits — `0xAC` was backwards+upside-down; `0x38` was upside-down + RTL + frozen. MX is banned (re-broke Y).
2. **Live motion (v2.6):** Must leave **frozen zeros**. EXT `blitLiveStock` every 1 Hz with live `mElapsed` (PR #12 `blitStockOnce` froze art on boot zeros; restore-compose with `0` poisoned KH/s). Stock V1 DigitalNumbers are the live values — **no** extra goods-band / `WIFI 0KH` overlay. INT paints live `mElapsed` every monitor tick (not `mElapsed==0`). `tDisplayV1PaintLivePulse` is a no-op (no off-chrome HUD).
3. **Down key:** **Fn+`.`** moves MINING → CLOCK → NETWORK → PRICE. Bare `;` / `.` still next (v1.2). **Fn+`;`** prev. `p` / `,` / Backspace / G0 unchanged.
4. **INT lag / live chrome:** After nav, INT updates within about one monitor tick (~100 ms). 1 Hz live `mElapsed` paint so KH/s / uptime / connecting are not stuck at 0. No per-tick extra `fillSprite` outside stock V1 `pushSprite`.
5. **Stock chrome (v2.1b / v2.2b / v2.6):** INT looks like stock single-screen NerdMiner (logo, clock, BLOCK TEMPLATES / BEST DIFFICULTY / 32BITS SHARES / VALID BLOCKS, KH/s, uptime, gauge, DigitalNumbers / `0xDEDB`). No `C_ORANGE` / custom yellow HUD. EXT composes the same V1 screens (live `mElapsed`) and scale-blits them, **centered, no goods-band overlay**. Not identical clones. No stray numbers below 320×240 or below INT chrome.

If a wipe remains, note whether it is every second (redraw) or only around SD/boot (bus). Serial `>>> EXT miner|clock|global|price` marks each 1 Hz paint.

## Field risks

- **No porkchop, dual firmware:** GPIO 5/3/6 still toggle. With `ASSUME_EXT` (default) INT shows the nav HUD and mining is drawn to a missing panel — flash **stock** `M5-Cardputer-Adv` if you have no EXT glass.
- **Write-only panel:** ID read on MISO39 is unreliable; that is why `ASSUME_EXT` is on. Use `FORCE_INT` if the bus must stay quiet.
- **SD + EXT:** If EXT CS is left low, SD enumerates fail. Boot always idles GPIO5 HIGH; `loadConfigFile` / `initSDcard` quiesce EXT first.
- **LoRa / Hydra on the Grove/hat pins:** Do not stack with the porkchop.
- **Color order:** Cheap ILI9341 modules may swap R/B. Driver uses BGR MADCTL; swap in `ili9341Ext.cpp` if a panel looks inverted.
- **EXT orientation (v2.6):** MADCTL is written **once at boot**: `MV|BGR` = `0x28` (no MX, no MY, no MH, no ML). Field: `e54ac1c` / `0x28` was L/R-good + sharp, only upside-down. v2.5 SW Y-flip made it upright but **L/R mirrored**. `0xAC` FAIL (backwards + upside-down). `0x38` FAIL (upside-down + RTL + frozen). Software X+Y (`EXT_TFT_SW_FLIP_X` + `EXT_TFT_SW_FLIP_Y`, default 1) on the write path — mirror each row and reverse rows. Do not rewrite MADCTL mid-run. Do not set MX. Override `-DEXT_TFT_MADCTL=` for A/B.
- **Keyboard / Launcher:** Dual env still compiles TCA8418 v1.2 and the app-only export. Stock env is the safe path if you only want PR #1 behavior.
