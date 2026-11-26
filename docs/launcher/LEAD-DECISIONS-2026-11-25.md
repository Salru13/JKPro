# JKPro launcher: lead decisions and facts for the design session (2026-11-25)

From the project lead (owner-approved direction). The design session owns `design/`; this file says what the launcher must do so the design can match reality, in function and in look.

## Scope
- **PES 2013 only for now.** PES 6 is parked (owner 2026-11-25). Keep the game catalog model so more games fit later, but design every screen for PES 2013 first.
- The game folder is `/data/JKPro/<folder>` (any letter case; the owner's is `/data/JKPRO/PES13`). Detection is by content: `pes2013.exe` + `settings.exe` + `img/dt01.img`; patched/kitserver folders show as "modded", never refused.

## Main flow (like JK4SPEED: PLAY / PATCH / SETTINGS)
1. **PATCH** (required once, again after updates). PES 2013 does not run correctly without it. It:
   - installs our runtime files into the game folder: `d3d9.dll` (our DXVK client), `dxvk.conf`;
   - **imports the player's registry file** (the `.reg` that PC guides ship, e.g. `PES_2013_Registry_Fix.reg`) into the game's Wine prefix. Without the `code` value the game says "Pro Evolution Soccer 2013 has not been installed". If no `.reg` is found: show which value is missing and where to put the file;
   - writes `settings.dat` (see Settings) into the game's Documents in the prefix;
   - shows progress in MB/steps and a clear done/failed state (JK4SPEED lessons: no frozen "0 of N").
2. **SETTINGS** (per game folder, stored in `System/games/<folder>/settings.ini`, written to the game as `settings.dat` at PATCH and PLAY):
   - Resolution: list from the game definition; **which entries are allowed depends on the kernel-panic investigation** (see below). Design for a short list (e.g. 1920x1080, 1280x720).
   - Display: full screen (fixed on PS4 unless proven otherwise).
   - Picture quality: Low / Medium / High.
   - Frame rate: 60 (fixed; the game speed follows the frame rate, 30 would make it slow).
   - Controller: XInput (the PS4 pad works in XInput mode; confirmed in menus 2026-11-25). Button remap screen like JK4SPEED CONTROLS, later.
   - Show FPS, Logs on/off.
   The settings.dat format is decoded (`research/pes13-settings-format.md`): resolution, full screen, quality, vsync, XInput are all writable by the launcher; no settings.exe on the console.
3. **SAVE GAMES**: import the player's PES 2013 saves from a PC or **another PES 2013 version** (copy-paste): `OPTION.bin` (option file: teams, kits, names), `EDIT.bin`, and the `save\` folder contents, dropped in `/data/JKPro/Saves/<folder>/` or USB `JKPro/saves/`. Launcher moves them into the game's `Documents\KONAMI\Pro Evolution Soccer 2013\save\`, keeps a backup, never overwrites without one. Export back to USB.
4. **MODS / patches by copy-paste**: the player copies a PES patch (e.g. PESEdit, kitserver data) into the game folder; the launcher only re-detects ("modded") and keeps settings. No mod engine for PES.
5. **PLAY**: composes and starts; loader while Wine starts; "The game didn't start" + support bundle on failure (JK4SPEED pattern).
6. **HELP / LOGS & SUPPORT / ABOUT**: as JK4SPEED; error codes per game.

## Look
- Our own art only: no Konami/PES logos, players or screenshots in the launcher or package. A football/stadium mood in our own drawing style is fine. Follow the JKPro name, the JK4SPEED/JkFlow software renderer limits (1920x1080 BGRX frame, pad only).

## Open, owned by the lead
- **Kernel panic** on the Slim at some PES 2013 starts (4–5 so far, logs in `docs/status/CASE-kernel-panic.md`). Until it is fixed, resolution/quality choices are provisional.
- Native renderer vs PE: decided by measurement.
