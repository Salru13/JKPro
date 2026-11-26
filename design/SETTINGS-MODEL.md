# JKPro: per-game settings model (rev 2, PES 2013)

Status: aligned with the lead decisions (2026-11-25), `research/pes13-settings-format.md` and `launcher/src/jk_gamecfg.h`. The lead owns the decisions.

## Principles (unchanged; already implemented in `jk_gamecfg`)
1. **One game folder = one profile.** `PES13` and `PES13-PESEdit` each have their own settings, saves and Wine prefix.
2. `settings.ini` holds **only what the player changed**. Defaults and allowed values come from `pes2013.game`. An empty file = the tested defaults.
3. The game's own `settings.dat` is **generated** from the profile at PATCH and at every PLAY, so the INI is the single truth. There is no `settings.exe` on the console.
4. Atomic writes plus one `.bak`. Unknown keys are kept. A value not allowed → default + W501.

## Where it lives

```
/data/JKPro/System/games/<folder>/settings.ini   player changes
/data/JKPro/System/games/<folder>/state.txt      verdict, version, patch record, last played
/data/JKPro/System/prefix/<folder>/              Wine prefix: system.reg incl. the player's .reg import
/data/JKPro/System/cache/<folder>/dxvk.conf      composed at PLAY
/data/JKPro/Saves/<folder>/KONAMI/Pro Evolution Soccer 2013/settings.dat   generated
/data/JKPro/Saves/<folder>/KONAMI/Pro Evolution Soccer 2013/save/          live saves
/data/JKPro/Saves/<folder>/Import/                                         player's drop zone (OPTION.bin, EDIT.bin, save/)
/app0/.../games/pes2013.game                     defaults, allowed values (read-only)
```
(`Saves/<folder>` is the game's Documents, per `jk_gamecfg.h`.)

## `settings.ini` keys → `settings.dat`

| ini key | Values (allowed) | Default | settings.dat | UI |
|---|---|---|---|---|
| `resolution` | `1920x1080` only | 1920x1080 | 0x10 width, 0x14 height, 0x18 aspect = 1 | locked (owner) |
| `display` | `fullscreen` | fullscreen | 0x20 = 1 | locked |
| `quality` | `high` only | high | 0x1C = 2 | locked (owner) |
| `frame_rate` | `60` | 60 | vsync always on: 0x0E bit0 = 1 | locked |
| `controller` | `xinput` | xinput | 0x0E bit3 = 1 (the game's own pad mode; no Xidi for PES 2013) | locked |
| `show_fps` | `0`, `1` | 0 | env `DXVK_PS4_FPS_HUD` | selectable |
| `logs` | `0`, `1` | 0 | launcher logging | selectable |

The other settings.dat fields keep the values settings.exe writes by default: flags 0xC3 base, UDP 5739, audio buffer 4096, input blocks. The generator is `bench/tools/pes13_settings.py`; the C port in the launcher is the lead's call.

## Example
```ini
[settings]
show_fps   = 1
```
All other keys = defaults (1920x1080, High, fullscreen, 60, XInput).

## The game definition (`pes2013.game`): additions the design needs
These are proposals; the lead decides the format.
- `[defaults]` / `[allowed]` for the keys above. `allowed` with **one** value = the UI shows the row locked.
- `[locked_note]` frame_rate = "The game's speed follows the frame rate"; display = "Fixed on PS4".
- `[saves]` drop names `OPTION.bin`, `EDIT.bin`, folder `save`; drop folder `Import`; Documents subpath `KONAMI/Pro Evolution Soccer 2013`.
- `[registry]` key `..\KONAMI\PES2013` (any *.reg holding it; searched in the game folder top level → /data/JKPro → USB JKPro/) and the required value name `code` (PATCH step 4, E210/E211).

## Isolation
- A separate prefix per folder (registry, Documents). Renaming a folder → JKPro asks once "Is PES13-new the same as PES13?" and moves the profile on yes (owner decision 9).
