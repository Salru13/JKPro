# Owner decisions (2026-11-25)

| # | Question | Decision |
|---|---|---|
| 1 | No patches | The goal is a "native" launcher with the least headache for the player: **no patch files and no Patch step if the tests allow it**. That is proven on the bench, not assumed. If a game turns out to need files, JKPro places them automatically and reversibly, and the player still never patches. (Owner note: people online reportedly run PES13 through a modified JkFlow, but they patch. A web search on 2026-11-25 found no source for this; the lead verifies it.) |
| 2 | Mods in 1.00 | A patched copy made on the PC, uploaded as its own game folder. A Mods folder comes **later**. |
| 3 | Do patched copies share saves? | The owner asked for the option with the least headache, based on community evidence. **Decision: each game folder keeps its own saves, plus a "Copy saves from another game folder" action.** Community: a patch's option file belongs to that patch. Switching patches usually means starting a new Master League (SuperPatch 3.0; a PES 2021 option-file thread). The PES 6 community tool "Patch Switcher" keeps a separate option file per patch for the same reason. Shared saves would break careers silently. |
| 4 | Saves from a PC | **Yes, and it must be easy.** The player places the save files in an Import folder, presses "Load saves", and JKPro backs up the current saves first so they can be restored. |
| 5 | Save backups | The last 10 matches plus one per day for 30 days. |
| 6 | ✕ on a game | Starts the game directly. △ opens the game page. |
| 7 | Display mode | Designer's call (not asked): shown locked as "Full screen (fixed on PS4)". |
| 8 | Language | English only. |
| 9 | Renamed folder | Ask once: "Is PES13-new the same game as PES13?" Yes = settings and saves follow. |

Community sources:
- https://soccergaming.com/forums/threads/superpatch-3-0-update-out-now.128209/
- https://evoweb.uk/threads/patch-switcher.39276/
- https://www.pesmodding.com/2021/05/pes-2021-dlc-60-pc-option-file-full.html
- https://soccergaming.com/forums/threads/pes13-option-file.174103/

---

# Rev 2: questions to the project lead (2026-11-25)

The decisions in `docs/launcher/LEAD-DECISIONS-2026-11-25.md` are adopted in `FEATURES.md` and `SETTINGS-MODEL.md` (rev 2). Owner decisions 1–3 above are superseded by them: PATCH is required, mods are copy-paste, PES 6 is parked.

| # | Question | Design proposal |
|---|---|---|
| Q1 | Where does PATCH look for the player's `.reg`? | 1) the top level of the game folder, 2) `/data/JKPro/` root, 3) USB `JKPro/`. The first file with the right key wins, and the file name is shown in step 4 |
| Q2 | The save drop zone `Saves/<folder>/` top level is also the game's Documents root. Is that OK? Or would a separate `Saves/<folder>/Import/` be clearer? | Keep yours (one folder per game for the player). The launcher ignores `KONAMI/` and only picks up `OPTION.bin`, `EDIT.bin`, `save/` placed beside it |
| Q3 | Defaults for resolution and quality while the kernel-panic case is open? | Show the short list; the default is the safest one the case allows. Hide "safe settings" until it is known |
| Q4 | Auto-backup of saves before every PLAY (owner decision 5): is it in `jk_saves` scope for 1.00? | Yes (the owner called saves the most important part) |
| Q5 | Numeric error codes (Blur style) or plain kinds (JK4SPEED)? | Numeric codes shown small, plus a plain sentence (`FEATURES.md` §7) |
| Q6 | Vsync bit (0x0E bit0) with the fixed 60: on or off? | Your measurement decides; it is not shown in the UI |

## Lead answers (2026-11-25), applied in FEATURES / SETTINGS-MODEL / mockups

| # | Answer |
|---|---|
| Q1 | The .reg search: any `*.reg` holding `[..\KONAMI\PES2013]`. It looks first at the top level of the game folder, then in `/data/JKPro`, then on USB in `JKPro/`. The file used is shown |
| Q2 | A separate drop zone, `Saves/<folder>/Import/` |
| Q3 | Defaults 1280x720 and Medium, marked provisional. 1920x1080 becomes the default when the kernel-panic case closes |
| Q4 | Auto-backup before every PLAY is in 1.00, keeping the last 10. The 30-day daily rule is later |
| Q5 | Numeric codes E1xx–E6xx, each with a plain sentence and what to do |
| Q6 | Vsync always on (bit0 = 1), not a setting. Frame rate is locked at 60 |
| Note | PES 2013 uses its own XInput pad mode (in settings.dat). No Xidi |
