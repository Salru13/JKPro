# JKPro: adding more PES games (design for later releases)

Status: design session, 2026-11-25. Builds on the owner's decisions:
- **JKPro 1.0 is PES 2013 only.**
- Later releases add games, and each game has its own PATCH.
- A game JKPro is not ready for is never shown.
- Least headache for the player.

The lead builds from this document. Engine facts come from the lead; game facts come from research (sources at the end).

Rating of each claim:
- **[C]** confirmed by a source.
- **[R]** community report.
- **[U]** unverified; must be checked on the bench before that game is built.

---

## 1. Principles

1. **One package decides which games exist.** A game appears on the shelf only when its *game pack* is in the package. JKPro 1.0 ships one pack (`pes2013`). Folders of other games are not shown and never touched, so nothing half-ready ever reaches the player.
2. **The UI is the same for every game.** The shelf, the △ game menu (PLAY · PATCH · SETTINGS · SAVE GAMES), the PATCH screen, Settings, Save games, Help, the error codes and the start screen are shared. What differs per game is data (texts, rows, steps, paths) plus a small piece of C for each file format.
3. **Each game folder is isolated.** It has its own settings, saves, registry values and PATCH record, so two folders of the same game (e.g. `PES13` and `PES13-PESEdit`) never mix.
4. **Never patch the exe, never ship game files, never crack.** The player brings their own copy. Disc-protected exes stay "Disc version: can't start" (E205).

## 2. Which games, in what order

| Game | Graphics | Ready-made parts | Main work | Proposal |
|---|---|---|---|---|
| PES 2013 | D3D9 | everything (1.0) | – | **1.0** |
| PES 2012 | D3D9, 32-bit [C] | same era as 2013: `img\` layout, registry key | settings.dat format [U]; exe name [U] | After PES 6 |
| PES 2011 | D3D9 [U] | `pes2011.exe` + `img\dt0f.img` [C]; the registry key fixes "not installed" under Wine [R] | settings.dat [U] | After PES 6 |
| PES 2010 | D3D9 [U] | `pes2010.exe` + `img\*.img` [R]; `KONAMI\PES2010` with code, installdir, version [R] | settings.dat [U] | After PES 6 |
| PES 6 | **D3D8** [R] | `PES6.exe` + `dat\*.afs` [C]; pack draft exists (`launcher/games/pes6.game`) | the D3D8 path (DXVK d3d8, 2.4+); **DirectInput only** [C], so it needs Xidi; no sound effects under Wine (bug 55998, open, workaround `dsdmo`) [R]; registry key unclear [U] | **Next after PES 2013 (owner, 2026-11-25)**; the biggest engine step |
| PES 2014 | D3D9, 32-bit [C] | `PES2014.exe` + `data\dt*.cpk` [R] | settings.exe needs .NET 4 under Wine [R] (our own writer avoids that); a start crash with native DLL overrides (bug 37329) [R] | Later, after the bench |
| PES 2008 / 2009 | D3D9 [C/U] | XInput supported [C] | exe names and layout [U] | Only if the owner wants them |
| PES 2015 / 2016 / 2017 | D3D9 (2016 is 64-bit) [C] | – | **Steam must be online to start; 2017 also has Denuvo** [C]. An owned copy cannot run offline | **Out of scope** (owner decision) |

Other facts about these games:
- None of them has graphics settings in the game. Everything goes through `settings.exe` [C], so JKPro's Settings screen writes each game's own file, as it already does for 2013.
- Saves live in `Documents\KONAMI\<full title>\save\` [C for 6, 2008, 2009, 2014–2017; the same pattern is assumed for 2010–2012, U].
- All of them run at 60 fps. In PES 6 the game speed follows the frame rate [R], so its Frame rate row is locked at 60, as for 2013.

## 3. The game pack

A pack = **one definition file** (data) + **one small C driver** (formats). Adding a game never changes the UI code.

### 3.1 Definition file (`games/<id>.game`, in the package)

This extends today's `pes2013.game`; the new sections are in **bold**.

| Section | Holds | Example (PES 2013) |
|---|---|---|
| `[identity]` | id, display name, exe, markers, version probe | `id=pes2013`, `name=PES 2013`, `exe=pes2013.exe`, `marker=settings.exe`, `marker=img/dt01.img` |
| `[foreign]` | markers of a patched copy (logged only, never shown, never blocking) | `loader_dir=kitserver` |
| **`[runtime]`** | what the title needs to start it | `api=d3d9`, `arch=32`, `client=28c101ff`, `server=enc17`, `dxvk.d3d9.maxAvailableMemory=512`, `input=xinput` |
| **`[registry]`** | the key, the required values, and where installdir points | `key=Software\KONAMI\PES2013`, `required=code`, `installdir=game` |
| **`[settings]`** | the rows: key, label, allowed values, default, locked + why, provisional | `resolution=1280x720,1920x1080;default=1280x720;provisional` · `display=fullscreen;locked=Fixed on PS4` · `frame_rate=60;locked=The game's speed follows it` · `controller=xinput;locked` |
| **`[writer]`** | which driver writes the game's settings, and where | `format=wecf2`, `path=Documents/KONAMI/Pro Evolution Soccer 2013/settings.dat` |
| **`[saves]`** | the save folder and the named files | `dir=Documents/KONAMI/Pro Evolution Soccer 2013/save`, `option=OPTION.bin`, `edit=EDIT.bin` |
| **`[patch]`** | the PATCH steps for this game, in order | `steps=check,windows,files,registry,settings,record` |
| **`[text]`** | what the player reads for this game | `start=Starting PES 2013…`, `reg_hint=PES_2013_Registry_Fix.reg`, Help lines |

### 3.2 Driver (C, one file per game family)

```c
typedef struct JkProGameDriver {
    const char *id;                                    /* "pes2013" */
    /* settings.exe's file, from the profile's values */
    int (*write_settings)(const char *path, const JkProGameSettings *s);
    /* optional extra PATCH step (e.g. PES 6: Xidi.ini); NULL: none */
    int (*extra_patch)(const JkProGamePaths *p, char *why, size_t cap);
    /* optional: is this file a valid save/option file (import check) */
    int (*save_ok)(const char *path);
} JkProGameDriver;
```

- PES 2013 → `jk_pes13set` (WECF v2, 852 bytes, CRC16). It exists today.
- For 2010–2012 the first bench job is to check whether settings.dat is the same WECF family [U]. If it is, they share the 2013 driver with other offsets.
- PES 6 gets its own driver: its settings plus the Xidi map, because it is DirectInput only.

## 4. Isolation between folders (engine change, needed before a second folder of any game)

Today one Wine prefix holds one game's registry key and one `Documents`. With several folders this breaks in three ways:
- Two folders of the same game share `Documents\KONAMI\…`, so they would share saves and settings.dat.
- They share one `installdir`, which points at only one folder.
- 64-bit games (2016) need a 64-bit prefix.

**Proposal (the lead decides):**
1. **Documents per folder.** Before each start, write `HKCU\…\Shell Folders\Personal` (and `User Shell Folders`) = `Z:\data\JKPro\Saves\<folder>`, as the NFS port did. This also puts the live saves outside the prefix, as `design/` wanted from the start. The saves survive a broken prefix, and the player finds them in one plain folder.
2. **Registry per start.** Run the game's `[registry]` import at every PLAY for the chosen folder. `jk_regimport` replaces the key's block, so this is safe. `installdir` then always points at the folder being started.
3. **One shared prefix per architecture** (`System/prefix` for 32-bit, `System/prefix64` later), not one per folder. That saves about 330 MB per extra game.
4. **Registry guard stays per prefix.** The 1.0 guard keeps the last good copies of system.reg, user.reg and userdef.reg (E610/E611).

## 5. Seam v2 (UI ↔ title)

1.0's seam carries one `game_dir`. v2 lets the UI choose among folders, and the title starts what the decision says:

```c
typedef struct {                       /* appended; 1.0 fields stay */
    const char *root_dir;              /* /data/JKPro (the UI scans it with the packs it has) */
    const char *games_dir;             /* /app0/jkpro-files/games: the packs in this package */
} JkProServicesV2;

typedef struct {
    JkProAction action;
    char game_id[16];                  /* "pes2013", "pes6" ... */
    char game_dir[256];                /* the folder the player chose */
    char exe[64];                      /* as spelled on disk */
    JkPes13Settings settings;          /* kept for pes2013 */
    char kv[16][2][48];                /* the profile's values for other games */
    int logs, pe_renderer, show_fps;
} JkProDecisionV2;
```

- The title reads `[runtime]` from the chosen pack: client/server pair, dxvk.conf caps, d3d8 or d3d9, arch, input mode. Engine knowledge stays in the pack, not in the UI.
- The UI already scans the root with `jk_games_scan` and already draws several tiles (the shelf scrolls). In 1.0 it only filters to one folder.

## 6. What the player sees with more games

A rendered preview is in `design/screens/expansion-shelf.png`. It is drawn by the real code with a model holding three games.

- **Shelf:**
  - One tile per supported game folder, in the order of last played.
  - ← → moves between tiles; ✕ plays (PATCH first when needed); △ opens that game's menu.
  - The tile shows the game's name from its pack, its state chip and the folder name.
- **PATCH:** the same screen, but the step list comes from the game's `[patch]`. PES 6, for example, gets "Install the controller map" (Xidi). Each game has its own PATCH record. "PATCH again" works per folder.
- **Settings:** the rows come from `[settings]`. Locked rows always say why. The screen title says which folder.
- **Save games:** the same screen; the drop folder is `Saves/<folder>/Import`; the file names come from `[saves]`. For PES 6 that is `KONAMI-WIN32PES6OPT` [R].
- **Start screen:** the same kick-off scene; the line comes from `[text] start` ("Starting PES 6…").
- **Help:** page 1 lists the games this release supports. Each game's PATCH page names its own registry file.
- **Not shown:** folders without a pack, and packs whose exe is a Steam/Denuvo build (detected by markers; the title says nothing about such folders).

## 7. Definition of done for a new game

1. Bench: the game starts on the PS4 with the pack's `[runtime]`, with no kernel panic in 10 starts, at 60 fps.
2. Detection: exe + marker tested on a clean copy, a patched copy and a kitserver copy (always accepted, never labelled).
3. Registry: the required values known. The "has not been installed" text is reproduced without them, and then fixed by them.
4. Settings: the writer is byte-identical to that game's settings.exe for every allowed value, with host tests as for `jk_pes13set`.
5. Saves: the save path and file names confirmed. A save from the owner's PC loads on the PS4.
6. UI: the pack's texts read through, one golden per new screen state, and Help updated.
7. Owner test on the console, then that game ships in a release.

## 8. Open questions

**For the owner:**
- **E1:** answered by the owner (2026-11-25): **PES 6 comes after PES 2013**. Then 2012/2011/2010 in an order still to be set.
- **E2:** PES 2015–2017 need Steam online, and 2017 has Denuvo. OK to leave them out for good?

**For the lead:**
- **E3:** Do you approve the Documents-per-folder redirect (section 4.1)? It also moves 1.0's live saves out of the prefix. It is best done before 1.0 ships, so saves never have to move later.
- **E4:** Seam v2: are the field names in section 5 OK, or do you prefer the pack's path only and the title reads the rest?

## Sources

- **Research report (2026-11-25):** PCGamingWiki API pages for each title (DRM, paths, D3D version, XInput) and the PES 2010 registry thread: https://evoweb.uk/threads/multiple-installs-of-pes.58769
- **WineHQ:**
  - PES 2011 "not installed": https://www.winehq.org/pipermail/wine-users/2011-July/094421.html
  - PES 2014 bugs 34572 and 37329: https://www.winehq.org/pipermail/wine-bugs/2014-September/395551.html
  - PES 2016 bug 40922
  - PES 6 sound bug 55998
- **ProtonDB:** https://www.protondb.com/app/592580 (settings.dat workaround for 2018)
- **Local:** `research/R1-wine-dxvk.md`, `research/R2-patches-tools.md`, `research/pes13-settings-format.md`.
