# JKPro launcher: analysis (patches, mods, saves)

> **Rev 2 note (2026-11-25):** the lead's decisions (`docs/launcher/LEAD-DECISIONS-2026-11-25.md`) supersede parts of this analysis:
> - **§2 "no patches":** the bench showed PES 2013 needs a **required PATCH**. It installs `d3d9.dll` and `dxvk.conf`, imports the player's `.reg` (the `code` value), and writes `settings.dat`. The design keeps the spirit as one screen, one press, real progress, and an automatic "PATCH again".
> - **§3 mods:** copy-paste into the game folder, shown as "Modded". There is no mod engine.
> - **PES 6** is parked.
>
> §1 (pain points), §4 (saves) and §5 (simplicity rules) still apply. `FEATURES.md` rev 2 is the current spec.

Status: proposal from the design session. The owner decides; the project lead verifies every technical claim on the bench.
Sources: the Blur launcher JkFlow 1.00 (`Ps4 BLUR - PC project/.claude/worktrees/blur-1.00-r4/launcher/`) and the NFS MW launcher JK4SPEED 1.00 (`NFS-MostWanted-PS4/launcher/`), both read-only.

The owner's goals, in their own words: a launcher that is *superior*, "maybe no patches at all", games should take mods where they exist, and **save files are very important**. Usability, robustness and simplicity are the key.

---

## 1. What players found hard in Blur and NFS MW

| # | Pain | Where | What JKPro does instead |
|---|------|-------|------------------------|
| 1 | Players did not know where files go, zipped or unzipped, or where the saves are | Blur findings 17, 17b, 18, 37 | One folder (`/data/JKPro`), a fixed layout, and a Help page that shows the real full paths with the player's own folder names |
| 2 | A Patch step before the first Play, with 9 steps (Blur) or a 5-minute run with the bar stuck at "0 of 60" (NFS) | Blur `bl_patcher`, NFS r3 | **No Patch screen.** JKPro never needs to write into the game folder to start it (section 2) |
| 3 | The online patch: clock check, pinned zips that break when upstream updates, confusing errors with no network | Blur 22, 31 | No online steps. Nothing is downloaded. PES online is out of scope |
| 4 | Wine prefix setup hidden inside Patch | NFS | First-start setup is its own short screen with a real progress bar, once per console |
| 5 | Questions at every Play ("untested exe", "no account") | Blur 7, 8 | Ask once, remember, never ask again. Play is one button press |
| 6 | Start crashes that only a support bundle could explain | Blur E101, NFS startup crash | Start marker plus a "didn't start" screen that names the likely cause, offers "Start with safe settings", and saves the bundle in one press |
| 7 | A missing mod archive was read as "turn the mod off", and every Apply then failed | NFS r3 P0-1 | A mod's state is only what the player chose. A missing file shows as "missing", never as "off" |
| 8 | A debug config and a dev marker shipped to the owner | NFS, Blur memory | The package has no bench switches. Bench is a separate title ID (lead's call) |
| 9 | The PS4 keyboard could not be used | Blur 12, NFS | JKPro needs no text input in v1. Rename is "later" and reuses `jk_keyboard` |
| 10 | Folders were scattered between `/data/BLUR` and `/data/JkFlow` | Blur 4, 5 | Everything under `/data/JKPro`, nothing anywhere else |
| 11 | FTP: file names with spaces refused, 0755 folders refused STOR, the path is case-sensitive | NFS | JKPro creates its folders 0777, matches names in any letter case, and Help warns about spaces |
| 12 | The save lived inside the Wine prefix where the player could not find it; the rolling backup was written but never called | Blur `bl_saves` | Saves live in a plain, visible folder. Backup is automatic and runs every time (section 4) |
| 13 | The UI ran at 1–5 FPS | Blur 2 | Reuse the cached static layers from Blur |
| 14 | The owner rule: every stop message says "Nothing was changed." when true, plus the next step | Blur 36 | A hard rule for every JKPro message |

---

## 2. "No patches at all": is it OK?

**Owner decision (2026-11-25):** a "native" launcher with the least headache, and no patch files if the tests allow it. The bench proves it. See `OPEN-QUESTIONS.md`.

**Short answer: yes, as a goal for the player.** The player never sees a Patch screen and never runs a patch step. Whether the game folder can stay 100 % untouched is a bench question for the project lead.

What "patch" meant in the old launchers:
- **Blur:** copies JkFlow's own DLLs (`d3d9.dll`, the DXVK client, `dxvk.conf`) *into the game folder*, plus the online (AMAX) files.
- **NFS:** copies `d3d9.dll`, `dinput8.dll` (ASI loader), Xidi and scripts into the game folder, and rebuilds mods. The exe itself was never patched.

So "patch" mostly meant *putting the launcher's own files next to the exe*. The launcher can do that another way.

### Three ways to build it

| | A. Zero-touch (recommended) | B. Hidden prepare | C. Patch screen (as Blur/NFS) |
|---|---|---|---|
| Idea | JKPro's DLLs stay in the package or the Wine prefix (`system32`) and load via DLL overrides. Game settings go into the prefix (Documents, registry). The game folder is only read | Like A, but if a game needs a file next to its exe, JKPro places it automatically at Play, journaled and reversible. There is no screen and no player step | A Patch button and checklist, as today |
| Player steps | Copy the folder, press Play | Copy the folder, press Play | Copy, Patch, wait, Play |
| Game folder | Never written | Written only by JKPro, tracked, undoable | Written, with backups |
| Risk | Wine must load `d3d9`/`dinput8` from `system32` before the exe folder, so test it on the bench | Low: the same code as NFS, without the UI | Known to work, but it is the pain listed in section 1 |
| Robustness | Best: nothing in the game folder can rot. Re-copying the game never breaks anything | Good | Fair: an interrupted Patch, leftovers (Blur E204) |

**Recommendation:** build **A**, and keep **B** as an internal fallback per game, set in the game's definition. The player sees neither; both just mean "press Play". The exe is never patched in any of them, as in NFS.

Wine loads a DLL from the exe folder first, so a file the player left there (an old `d3d9.dll` from a PC patch, say) can beat JKPro's. A then checks the game folder for these known "foreign loader" files (the same list Blur parks: `d3d9.dll`, `dinput8.dll`, `dsound.dll`, `winmm.dll`, `version.dll`, `*.asi`). If it finds one, it shows one clear choice: *"This folder has a d3d9.dll from a PC mod. JKPro will set it aside (you can put it back). [Set aside and play] [Back]"*. Nothing is deleted.

---

## 3. Mods

### What PES mods look like (the lead's research must confirm this)

| Game | Common mod types | Notes |
|---|---|---|
| PES 6 | **Option file** (teams, names, kits): a *save* file | It lives in the save folder, so it is handled like a save (section 4) |
| PES 6 | Replaced `dat/*.afs` archives (faces, kits, balls, text) | Whole-file swaps |
| PES 6 | Kitserver 6 (a DLL loader; its setup tool writes into the exe) | JKPro never patches the exe, so "later" at best |
| PES 2013 | Extra `.cpk` files in `data/dt...` plus an edited `DpFileList.bin` | Big patch packs come as **Windows installers (.exe)**. These cannot run on the PS4 |
| PES 2013 | Option file / EDIT data in the save folder | Handled like a save |
| PES 2013 | Loader tools (a Sider-like tool, if one exists for 2013) | Needs verification |

### Three ways to support mods

| | 1. Modded copy of the game (recommended for big patches) | 2. Mods folder with swap and journal (recommended for small mods) | 3. Virtual overlay in Wine (later) |
|---|---|---|---|
| How | The player installs the patch **on the PC** and uploads the patched folder under its own name, e.g. `/data/JKPro/PES13-Patch2026`. JKPro shows it as its own tile, with its own settings | The player drops a mod (folder or zip) in `/data/JKPro/Mods/<game folder>/`. The mod's files mirror the game folder. Turning a mod on swaps those files in; the originals are kept in `System/pristine/` and swapped back when it is turned off | JKPro's Wine resolves files from `Mods/` before the game folder, so no files move at all |
| Player effort | Uses the patch's normal installer on PC. No new rules to learn | Drop and switch on | Drop and switch on |
| Robustness | Best: zero JKPro logic. The original game stays clean in its own folder | Good (NFS pattern: staging, verify, swap, journal) | Best, but it is engineering inside Wine |
| Cost | Disk: two copies of the game | Disk: pristine copies of replaced files only | Lead and Codex time |

**Recommendation:**
- v1 = **1 + option files as saves.** It is free, robust, and covers the big PES patches, which are installers anyway.
- 1.1 = **2** for small file swaps.
- **3** is the long-term "superior" answer if the lead finds it cheap.

The exe of a patched copy may differ from the known one. JKPro then shows "Modified game: not tested" once, and Play still works.

---

## 4. Saves: the most important part

### Rules (must-have)
1. **Saves live outside the game folder and outside the Wine prefix**, in a plain folder: `/data/JKPro/Saves/<game folder>/`. JKPro points the game's "My Documents" there per game, as NFS did with `Shell Folders\Personal`. Re-copying or deleting a game folder never touches its saves.
2. **Automatic snapshot before every Play.** JKPro copies the save folder to `Backups/<game folder>/<date-time>/` with a `files.txt` (size and sha256 per file). PES saves are a few MB, so this takes about a second. It keeps the last **10 plays** plus **one per day for 30 days**. The rule is fixed, so it is easy to explain.
3. **A check after every exit.** If a file shrank to 0 bytes, went missing, or the game did not end normally (start marker still present), JKPro offers: *"Your save may be damaged. Restore the one from before this match? [Restore] [Keep current]"*. Nothing happens without a press.
4. **Atomic writes, everywhere.** A restore goes temp → verify → swap, and the current save is backed up first. Nothing is ever deleted; replaced saves go to `Backups/.../replaced-<stamp>/`.
5. **Saves are never in the support bundle, never removed by an update, never moved by a migration** without a copy staying behind.
6. **Visible to the player:**
   - The Saves screen shows each snapshot: date, time and size, plus "before match" or "after crash".
   - Restore takes two presses.
   - Export copies the save to USB or `/data/JKPro/Export/`, and never overwrites an earlier export.
   - Import comes from the same places.
7. **Option files** (PES teams and kits) are imported like a save. They get their own label, "Option file", so the player can swap databases without losing a Master League save. The lead must check whether PES keeps them in separate files.

### Owner decisions on saves (2026-11-25)
- Each game folder keeps its own saves. Patch option files belong to their patch, and switching patches usually means a new Master League (community evidence in `OPEN-QUESTIONS.md`). A "Copy saves from another game folder" action covers players who want to move a career.
- **Load saves** is a must:
  1. The player drops the files in `Import/<game folder>/` (or on USB) and presses one button.
  2. JKPro backs up the current saves first, then loads the new ones.

### Open points for the lead
- The exact save paths and file names inside Documents for PES 6 and PES 2013 (the expected shape is `KONAMI/Pro Evolution Soccer <n>/save/...`). Do the games use a checksum or a machine-bound key? If so, saves from a PC may not load on the PS4.
- Can a save from the owner's Windows PC be imported as-is? The owner will want this.
- `fstatfs` returns EPERM, so free space is unknown. Snapshots are small and capped by count, so this is acceptable.

---

## 5. Simplicity rules for every screen
- **Play is one press from the moment JKPro opens:** the last played game is focused, ✕ = Play.
- At most ✕, ○ and **one** extra face button per screen, always shown in the hint bar (NFS rule).
- Readable at 3 m: body text ≥ 30 px, menu items ≥ 60 px, everything inside the 90 % safe area (NFS rule).
- Every stop message has three lines:
  - what happened;
  - "Nothing was changed." when true;
  - the next step.

  The error code goes small, in a corner.
- No setting needs to be touched to play. Defaults are the tested values per game.
- Never ask the same question twice.
