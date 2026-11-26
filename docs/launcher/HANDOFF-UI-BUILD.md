# Handoff to the launcher session: build the JKPro launcher UI (2026-11-25)

Owner decision: the launcher session BUILDS the launcher UI (code, not only design). The project lead keeps the engine, the title integration and the packages.

## Status of the engine
- PES 2013 runs on the PS4 through **JKPro v0.1/v0.2** = the JkFlow 1.00 runtime and title source (WSL `~/orbis-win-host-bootstrap/jkpro-title`, a copy of `blur-launcher-r4`) with **no UI yet**.
- `launcher/jkflow/jkpro_prepare.inc` replaces JkFlow's `blur_launcher_screen`: it seeds the Wine prefix, copies `jkpro-files/d3d9.dll` into the game folder, imports the player's `.reg` (`jk_regimport`), writes `settings.dat` (`jk_pes13set`: 1920x1080, full screen, High, XInput), composes profile + env into JkFlow's `BlDecision` and starts the game. Owner: "smooth".
- Build: `launcher/jkflow/apply_v01.sh`, then `build-launcher.sh` in the WSL tree (about 2 min), then `launcher/jkflow/make_pkg_jkpro.sh`.
- The title restarts itself after the game ends (`launch.cycle > 0`). v0.2 then goes to the PS4 home; with a UI, the UI must come up again (never auto-start).

## What you build
The JKPro launcher UI in C under `launcher/src` (jk_* style; host tests via `launcher/tests/run.sh`; golden PNGs), per `design/FEATURES.md` rev 2:
- Home: PLAY · PATCH · SETTINGS · SAVE GAMES;
- PATCH steps with MB progress;
- Settings (resolution, quality, …) → `JkPes13Settings`;
- Save games: import / backup / export;
- Help, Logs & Support, About, error codes.

Reuse the JK4SPEED renderer, screens and keyboard (`jk_ui`, `jk_art`, `jk_screens`, `jk_frontend`), but drop the NFS parts: mods engine, X360, EA save CRC, NFS pace.

## The seam (keep it exactly; the lead integrates it into the JkFlow title)
```c
#include "jk_pes13set.h"
typedef enum { JKPRO_START = 1, JKPRO_LEAVE } JkProAction;
typedef struct { JkProAction action; JkPes13Settings settings; int logs; int pe_renderer; } JkProDecision;
typedef struct {
    const char *game_dir;     /* e.g. /data/JKPRO/PES13 ("" when none found) */
    const char *prefix_dir;   /* <JKPro>/System/prefix (seeded by the title before the UI) */
    const char *system_dir;   /* <JKPro>/System (settings.ini, state) */
    const char *files_dir;    /* /app0/jkpro-files (d3d9.dll) */
    const char *saves_drop;   /* <JKPro>/Saves/PES13/Import */
    const char *usb_dir;      /* USB root or NULL */
    /* + the JkServices callbacks: present / pad / now_ns / sleep_us / log / wall_time */
} JkProServices;
int jkpro_ui_run(const JkProServices *s, JkProDecision *out);   /* 0 ok */
```

PATCH and SAVES do their file work inside your module with the existing tested helpers:

| Helper | What it does |
|---|---|
| `jk_regimport` | player `.reg` → prefix `system.reg`, `installdir = Z:<game_dir>\` |
| `jk_pes13set_write` | writes `settings.dat` into the prefix's Documents |
| `jk_fs` | installs `d3d9.dll` |
| `jk_games` | detection and verdict |
| `jk_gamecfg` | `settings.ini` |

Prefix paths:
- `settings.dat`: `drive_c/users/BLUR/Documents/KONAMI/Pro Evolution Soccer 2013/settings.dat`
- saves: `drive_c/users/BLUR/Documents/KONAMI/Pro Evolution Soccer 2013/save/` (`OPTION.bin`, `EDIT.bin`, …)

Commit inside `launcher/` on branch `jkpro-1.0` with a targeted `git add`. Tell the lead when a first host-tested `jkpro_ui_run` exists.
