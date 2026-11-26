# JKPro launcher: features and screens (rev 2, PES 2013)

Status: design proposal aligned with `docs/launcher/LEAD-DECISIONS-2026-11-25.md` (lead, owner-approved). **M** = must-have for 1.00, **L** = later. The lead owns the decisions.
Mockups: `mockups/jkpro-mockups.html`. Earlier analysis: `ANALYSIS.md` (rev 1; the sections it supersedes are marked there).

**Scope: PES 2013 only.** PES 6 is parked. The game catalog (`launcher/games/<id>.game`) stays, so a later game is a new definition file and no screen changes.

## Folder layout on the PS4 (matches `jk_gamecfg.h`)

```
/data/JKPro/                      any letter case (owner's: /data/JKPRO)
  PES13/                          player's game folder. PATCH adds d3d9.dll + dxvk.conf (backed up first)
  PES13-PESEdit/                  optional second folder (another version, or a patched copy): its own tile and profile
  Saves/<folder>/                 the game's own Documents (KONAMI tree: live saves + settings.dat)
  Saves/<folder>/Import/          drop zone: OPTION.bin / EDIT.bin / save\ placed here are imported
  Backups/<folder>/<stamp>/       save backups (before every Play, before every import)
  Logs/   Logs/support/
  System/games/<folder>/settings.ini, controls.ini, state.txt
  System/prefix/<folder>/         Wine prefix (registry with the player's .reg)
  System/cache/<folder>/          dxvk.conf, shader cache
```

---

## 1. Home and game folders

| Feature | | Notes |
|---|---|---|
| **Home = a shelf of game tiles** (owner, 2026-11-25). **1.0 is PES 2013 only**: the shelf shows the PES 2013 folder's tile and nothing else. Other folders under /data/JKPro are not shown and never touched. PES 6, PES 2010 and others come in later releases, each with its own definition and PATCH; the shelf is built so it grows without a redesign | M | |
| **✕ on a tile = play.** If the game needs PATCH, ✕ runs PATCH first and then offers Play | M | |
| **△ on a tile = the game menu**: PLAY · PATCH · SETTINGS · SAVE GAMES, for that game only. ○ goes back | M | |
| No PES 2013 folder: an explanation in place of the tile ("PES 2013 not found", where to copy it). ✕ shows E201 | M | |
| More games on the shelf (← → between tiles) | L | Later releases (owner) |
| The last played game is focused at open | M | |
| □ opens Help | M | |
| The state line on the card: `Ready` · `Needs PATCH` · `PATCH again (files changed)` · `Check game files` (one red state for a missing file or an unsupported pes2013.exe; the dialog names exactly what is wrong, E202/E205). A patched or kitserver folder is detected but **never labelled** on screen (owner, 2026-11-25: no "Modded"); it is only logged | M | From `JkGameVerdict` + the patch record |
| Detection by content (`pes2013.exe` + `settings.exe` + `img/dt01.img`), any letter case. The exe hash never blocks (kitserver/PESEdit change it) | M | Already in `jk_games.c` |
| An unknown folder under /data/JKPro: listed once in Help as "not a PES 2013 folder" and never written to | M | |
| Several folders that differ only in letter case: refused with a message | M | NFS lesson |
| More games (PES 6 and later) as new definitions | L | |

## 2. PATCH (required once, and again after updates or a copy-paste mod)

PATCH is one screen with a step list and real progress (MB and step N of 6). Each step shows ✓ / working / ✗.

| Step | What it does | |
|---|---|---|
| 1 Check game folder | exe, markers, version; an exe JKPro cannot start stops here: "This pes2013.exe version is not supported by JKPro." | M |
| 2 Prepare Windows layer | First time only: seeds `System/prefix/<folder>` with MB progress | M |
| 3 Install JKPro files | `d3d9.dll`, `dxvk.conf` into the game folder. A file that is not ours is backed up first (`System/backup/...`), temp → verify → rename | M |
| 4 Import registry file | Finds the player's `.reg` (e.g. `PES_2013_Registry_Fix.reg`) and imports it into the prefix. Checks that `code` exists | M |
| 5 Write settings | `settings.dat` from the folder's settings.ini (decoded format) | M |
| 6 Check and record | Verifies steps 3–5, writes the patch record | M |

| Behaviour | | |
|---|---|---|
| Done: "PES 2013 is ready. ✕ Play" | M | One press from PATCH to PLAY |
| Failed: shows the step, the cause, "Nothing was changed." (or what was undone), and the next step. □ opens Help at the right page | M | |
| **.reg search (lead Q1):** any `*.reg` that holds the `[..\KONAMI\PES2013]` key. It looks first at the top level of the game folder, then in `/data/JKPro/`, then on USB in `JKPro/`. Step 4 shows which file was used | M | |
| **The .reg is missing or has no `code`:** "PES 2013 needs the registry file from your PC. Put the .reg file in `/data/JKPro/PES13/` and press PATCH again. Missing: code." | M | |
| Interrupted PATCH (power or crash) finishes the next time it runs | M | JK4SPEED journal |
| Restore: △ twice on PATCH removes our files and puts back the backed-up ones | M | |
| PATCH again is detected automatically: our files are missing or changed (a copy-paste mod overwrote `d3d9.dll`), the package was updated, or settings.dat was replaced | M | The card shows "PATCH again", and PLAY leads to PATCH |

## 3. SETTINGS (per game folder)

Values come from `pes2013.game` `[defaults]`/`[allowed]`; `settings.ini` holds changes only. The settings are written to `settings.dat` at PATCH and at every PLAY.

| Row | Values | | Notes |
|---|---|---|---|
| Resolution | **1920×1080, fixed** (owner, 2026-11-25: "lock to the highest, 1080 and 60 FPS with High") | M | Shown as an info row, not a choice |
| Display | Full screen | M | Shown locked: "Full screen (fixed on PS4)" |
| Picture quality | **High, fixed** (owner-verified smooth at 1080p, 58.9 FPS) | M | Shown as an info row |
| Frame rate | 60 | M | Shown locked, with the note "The game's speed follows the frame rate". Vsync is always on (settings.dat flags bit0 = 1) and is not a setting |
| Controller | XInput (PS4 pad) | M | Shown locked. This is PES 2013's own pad mode, written in settings.dat. **No Xidi for PES 2013** |
| Show FPS | Off / On | M | |
| Kitserver (owner, plan C) | On / Without kits / Off; the row shows only when `<game>/kitserver/config.txt` exists. Only the comment state of the `dll =` lines under `[kload]` changes ("; " in front); BOM, CRLF and every other line stay byte-identical. The original is kept once in `System/backup/kitserver/config.txt.original`; On restores the dll lines' state from it. Saved as `kitserver=on\|nokits\|off`, applied on save and again before PLAY. The row shows the state read from config.txt | M | `jkpro_kitcfg.c` |
| Kitserver face fix (owner, 2026-11-26, W7) | Off (default) / On; the row shows only when `<game>/kitserver/fserv.dll` exists and shows the file's real state (On, Off, or "Not for this fserv.dll" for another version). On patches the player's own fserv.dll in place (lead's `jkpro_fservfix`, the known original only, one backup `fserv.dll.jkpro-orig` next to it); Off restores it. Saved as `fservfix=on\|off`, applied on save and again before PLAY; an error is logged and the game still starts. PATCH never touches Kitserver files | M | Help page 1 points to it for the fast team-scrolling crash |
| Logs | Off / On (+ Clear logs) | M | Under "Logs & Support" as in JK4SPEED |
| Reset to defaults | removed: nothing to reset but Show FPS | – | |
| Button remap (CONTROLS) | as JK4SPEED | L | |

Locked rows are shown as plain text, so the player sees the truth and nothing pretends to be adjustable.

## 4. SAVE GAMES

| Feature | | |
|---|---|---|
| Shows what the folder has: Option file (`OPTION.bin`), Edit data (`EDIT.bin`), saves (Master League, Become a Legend...) with dates | M | |
| **Import by copy-paste:** the player drops `OPTION.bin` / `EDIT.bin` / the `save\` contents in `/data/JKPro/Saves/<folder>/Import/` or USB `JKPro/saves/`. JKPro notices them ("3 files ready to import") and **Load saves** backs up the current saves, then moves the new ones into `Documents\KONAMI\Pro Evolution Soccer 2013\save\` | M | From a PC or another PES 2013 version |
| Nothing is overwritten without a backup. Backups are taken before every import, before every restore, and before PLAY **only when the saves changed** since the newest backup (sha256), so repeated PLAY never churns the history. The **newest 10** are kept | M | Owner, 2026-11-25 |
| Each backup folder (`System/backup/saves/<folder>/<date_time>/`) holds a `backup.txt`: reason, time, files with sizes and sha256, readable over FTP | M | |
| SAVE GAMES states the real paths, in the console's letter case: where to put files (Import + USB), the names (EDIT.bin = option file: teams, names, kits; OPTION.bin = game settings), the live saves, the backups. After an import it lists the files loaded, says "Import is empty again" (the dropped files are deleted once copied and checked: the console FTP cannot remove non-empty folders, W1) and names the backup taken; leftover empty done-* folders are removed at open, non-empty ones are named. A .bin other than EDIT.bin/OPTION.bin gets a "rename yours to one of these" hint, and an OPTION.bin over 2 MB gets "this looks like an option file: name it EDIT.bin" (hints only; files are still copied with their names) | M | Owner UX |
| Also keep one backup a day for 30 days | L | |
| After import, the files in `Import/` move to `Import/done-<stamp>/`, so the same import never runs twice | M | |
| **Restore a backup**: a list of every kept backup, newest first: date and time, the reason (before match / before import / before restore), file names with sizes. Pick any one; the confirm names what comes back and what it replaces; the current saves are backed up first ("before restore"). The backup being restored is never pruned | M | Owner, 2026-11-25 |
| Export to USB `JKPro/export/<folder>-<stamp>/`, never overwriting | M | |
| Copy saves from another PES 2013 folder (e.g. PES13 → PES13-PESEdit) | M | Each folder has its own saves by default |
| After a crash: if a save is 0 bytes or missing, offer the backup from before the match | M | |
| Saves are never in the support bundle | M | |

## 5. Mods and patches (copy-paste)

| Feature | | |
|---|---|---|
| The player copies a PES patch (PESEdit, kitserver data...) into the game folder. JKPro re-detects it (logged, no label on screen) and keeps the settings | M | No mod engine |
| If the copy overwrote JKPro's files or `settings.dat`: "PATCH again" (one press, settings kept) | M | |
| Help page "Using a PES patch": copy it into the folder, press PATCH again, saves are kept | M | |

## 6. PLAY

| Feature | | |
|---|---|---|
| Save backup → write settings.dat → compose → start. A loader with "Starting PES 2013..." until the first frame | M | |
| Start marker. The next open after a failed start shows **"PES 2013 didn't start"**: ✕ Try again · □ Save support bundle | M | JK4SPEED |
| "Start with safe settings" (1280×720, Low) | L | Only once the panic case shows which settings are safe |
| Exit back to JKPro cleanly | M | |

## 7. HELP · LOGS & SUPPORT · ABOUT

| Feature | | |
|---|---|---|
| Help pages: 1 Put the game in place (FTP, folder name, the .reg file) · 2 PATCH · 3 Saves and import · 4 Using a PES patch · 5 Troubleshooting | M | Real paths with the player's own folder name |
| Logs & Support: Logs on/off, Clear logs, Save support bundle (no saves, no .reg, user name masked) | M | |
| About: version, Official/Unofficial build, credits (incl. futbol13 research note if the lead wants it) | M | |
| Error codes (below), shown small in the corner of every error | M | |

### Error codes (numeric, JkFlow style, decided by the lead in Q5; one scheme for all games; the game id goes in the log line)
Each code has a plain sentence and what to do, plus "Nothing was changed." when true.

| Code | Meaning |
|---|---|
| E101 | Didn't start before its first frame |
| E102 | Ended abnormally |
| E201 | `pes2013.exe` not found / not a PES 2013 folder |
| E202 | Required game files missing |
| E203 | Duplicate folder spellings |
| E205 | Unsupported version: this pes2013.exe is not supported by JKPro |
| E210 | Registry file (.reg) not found |
| E211 | The .reg has no `code` value |
| E301 | PATCH failed (step N) |
| E302 | Restore failed |
| E401 | Save backup failed (PLAY is blocked until the player picks "Play without backup") |
| E402 | Import failed, the current saves are kept |
| E404 | A save looks damaged after exit |
| E501 | settings.dat not written |
| W501 | A setting not allowed, default used |
| E601 | Disk full |
| E610 | A crash emptied the Wine registry (system.reg / user.reg / userdef.reg); JKPro put back the last good copy (a note, PLAY works) |
| E611 | The registry is damaged and there is no good copy: PLAY is blocked; reopen JKPro (the Windows layer is set up again), then PATCH |
| E603 | Support bundle failed |

## Look (lead decision 4)
- JKPro's own art only: no Konami/PES logos, players or screenshots. The mood is a stadium at night. The one motif is pitch lines (centre circle, box) drawn as plain strokes.
- Built for the JK4SPEED software renderer: a 1920×1080 BGRX frame, flat fills, at most a static gradient in the cached background, no blur, no live transparency. Focus = a solid amber block.
- Readable at 3 m: body text ≥ 30 px, menu items ≥ 60 px, inside the 90 % safe area. At most ✕, ○ and one more face button per screen, always in the hint bar.

## Data packs (built 2026-11-25; the engine redirect is the lead's)

Official PES 2013 Data Packs (`dt*.img`), imported "the same way as the option file".

- **One inbox:** the same `Saves/<folder>/Import/` (and USB `JKPro/saves/`). Load sorts by name:
  - `OPTION.bin`, `EDIT.bin` and save files go to the save folder (copied, with a backup first);
  - `dt*.img` go to the data packs (**moved**, not copied; they are large and on the same filesystem).
- **Where it fits:** its own row in the △ game menu, **DATA PACKS**, between SAVE GAMES and the end. It has a different life from saves: no backups, no restore, a size budget. SAVE GAMES' banner still says "1 data pack waiting: dt0f.img · 312 MB → DATA PACKS", so the one inbox stays one place.
- **DATA PACKS screen:**
  - "Install" moves the waiting packs into `DataPacks/<folder>/`.
  - A list of the installed packs: file name, version when it can be identified (the lead's name/size/header rules), size.
  - Remove moves a pack back to `Import/removed-<date_time>/` and never deletes it.
  - The paths, as on SAVE GAMES: "Put files in …/Import/", "Installed packs are in /data/JKPRO/DataPacks/PES13/".
  - A note that the game reads them at its next start.
- **Seam:** `JkProServices.datapacks_dir` (appended). The title redirects the game's `ProgramData\KONAMI\Pro Evolution Soccer 2013\download\` there.
