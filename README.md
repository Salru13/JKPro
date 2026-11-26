# JKPro

Play your own PC copy of **Pro Evolution Soccer 2013** on a PS4: native 1920×1080, High, 60 FPS, full screen, and **online against other players** through ThirteenServer (from 1.01, see [Online play](#online-play-thirteenserver)).

[![release](https://img.shields.io/github/v/release/Salru13/JkPro)](https://github.com/Salru13/JkPro/releases/latest)
[![downloads](https://img.shields.io/github/downloads/Salru13/JkPro/total?label=downloads)](https://github.com/Salru13/JkPro/releases)
[![visits](https://hits.sh/github.com/Salru13/JkPro.svg?label=visits)](https://hits.sh/github.com/Salru13/JkPro/)
![platform](https://img.shields.io/badge/platform-PS4%20(homebrew)-0070d1)
![license](https://img.shields.io/badge/launcher-LGPL--2.1--or--later-blue)

JKPro is a PS4 app with its own launcher, the Wine runtime for the console from [JkFlow](https://github.com/Salru13/JkFlow) and a native DXVK renderer. You bring the game; JKPro checks your PES 2013 folder, sets it up on the console and starts it. JKPro ships **no game files and no Konami files**. This repository holds the source of the launcher and the title build, and the licence texts shipped in the package. Player guide: [README-player.md](README-player.md).

## Where everything goes

```
/data/JkPro/                             <- made by JKPro at its first start (capital J and P)
├── PES13/                               <- your game: PES 2013 PC
│   ├── pes2013.exe                      <- directly in PES13, not in a folder inside it
│   ├── PES_2013_Registry_Fix.reg        <- the .reg with your game's code (PATCH needs it)
│   └── (all the other game files and folders, kitserver/ too if you use it)
├── Saves/PES13/Import/                  <- drop EDIT.bin, OPTION.bin or a data pack (dt0f.img) here
├── Saves/PES13/                         <- your live saves (made by JKPro)
├── DataPacks/PES13/                     <- installed data packs (made by JKPro)
└── Logs/                                <- logs and support bundles
```

1. **Install the PKG** with your homebrew PKG installer.
2. **Open JKPro once.** It makes `/data/JkPro/` with `Saves`, `DataPacks`, `Logs` and `System`. Close JKPro. Do not make these folders yourself: the names are case-sensitive (`JkPro`).
3. **Copy your whole PES 2013 folder** into `/data/JkPro/PES13`, over FTP or from a USB drive. `pes2013.exe` must be directly in `PES13`.
4. **Open JKPro, press PATCH, then PLAY.** The tile under PES 2013 says **Ready** when everything is in place.

## Requirements

| What | Details |
|---|---|
| Console | A **jailbroken PS4** that can install homebrew PKGs. |
| Game | Your own copy of **Pro Evolution Soccer 2013 PC**, with the `.reg` file that holds its code (for example `PES_2013_Registry_Fix.reg`) in the game folder. |
| Optional | Your option file (`EDIT.bin`), a data pack (`dt0f.img`), Kitserver 13 inside the game folder. |
| Copying | FTP to `/data/JkPro/` or a USB drive. |

Use only a copy of PES 2013 you own. JKPro does not include the game.

## Download

Get the `.pkg` from the [Releases](https://github.com/Salru13/JkPro/releases) page (only there).

## The launcher: PLAY, PATCH, SETTINGS

| Tile text under PES 2013 | Meaning |
|---|---|
| **Ready** | Press PLAY. |
| **PATCH needed** | Press PATCH once (also after you copy the game again). |
| **Check game files** | Something is missing in `/data/JkPro/PES13`; Help explains what. |

- **PATCH** sets up the game's Windows files, imports the game's code from your `.reg`, and locks the display settings.
- **PLAY** starts the game.
- **SETTINGS**: Show FPS, Logs & Support, Kitserver (On / Without kits / Off) and Kitserver face fix (Off / On), shown only when your game folder has Kitserver, and **Online** (Off / ThirteenServer, with your online account).
- **SAVE GAMES** and **DATA PACKS** in the game menu: import, restore and remove.

Display settings are fixed: 1920×1080, High, full screen, 60 FPS with vsync. They are not a choice.

## Save games and the option file

Your saves live in `/data/JkPro/Saves/PES13/`. **SAVE GAMES** shows every path.

1. Put `EDIT.bin` (the option file: teams, names, kits) and/or `OPTION.bin` (game settings) in `/data/JkPro/Saves/PES13/Import/`, or a whole `save/` folder. From USB: `USB:/JKPro/saves/`.
2. Open **SAVE GAMES → Load saves**. JKPro backs up your current saves first, copies the files in and empties Import.
3. **Restore a backup** lists every backup with date, reason and files; pick any one.

The game saves only when you save in the game.

## Data packs

Put the data pack `dt0f.img` in the same Import folder and open **DATA PACKS → Install**. **Remove** keeps the old pack, and **Use** switches back to an older one. An option file made for a data pack needs that pack installed.

## Kitserver 13

Kitserver is your own mod, inside your game folder (`PES13/kitserver/`). JKPro does not ship it.

- **SETTINGS → Kitserver**: On, Without kits (kit module off) or Off (no Kitserver module).
- **SETTINGS → Kitserver face fix**: turn it On if PES 2013 closes while you scroll fast through teams. It patches Kitserver's face module (`fserv.dll`) in place and keeps your original next to it (`fserv.dll.jkpro-orig`); Off puts the original back. Players with broken face data then show a default face.

## Online play (ThirteenServer)

From 1.01, JKPro can take PES 2013 online through [ThirteenServer](https://thirteenserver.online), a community server for PES 2013. It is not affiliated with Konami or JKPro.

### Set up once

1. Create a free account on [thirteenserver.online/register](https://thirteenserver.online/register). Your username becomes your name in the game.
2. In JKPro, open **SETTINGS → Online** and set it to **ThirteenServer**.
3. Press ✕ on the Online row. Enter your **Online ID** (your ThirteenServer username) and your **Password** with the on-screen keyboard. Leave **Registration code** empty: then your ID is used, and ThirteenServer accepts any text. Press **Options** to save.
4. Leave **Kitserver** on. Without it PES 2013 stops at a "minimum system requirements" message.

### Log in, step by step

The game's own keyboard does not open on the PS4, so JKPro types your saved account into the box you have selected:

| Button | Types |
|---|---|
| **L1 + touchpad click** | your ID |
| **R1 + touchpad click** | your password |
| **L1 + R1 + touchpad click** | the registration code |

1. Press **PLAY** and choose online in the game's main menu.
2. PES asks for a **registration code**: select the box, hold **L1 + R1** and click the touchpad, then confirm.
3. At the login, select the **ID** box: hold **L1** and click the touchpad.
4. Select the **password** box: hold **R1** and click the touchpad.
5. Confirm and log in. Accept the server's data pack when the game offers it. Then pick a lobby and play.

Hold the shoulder button first, then click the touchpad. Each click types the whole text and presses Enter.

How it works:
- JKPro points the game's old online addresses to ThirteenServer, as the server's own PC `setup.bat` does with the Windows hosts file.
- Your `pes2013.exe` is not changed.
- Your account stays on your console only, in `/data/JkPro/System/online-account` (line 1 ID, line 2 password, optional line 3 registration code). It is never logged and never sent anywhere except to the game. Reinstalling or updating the pkg keeps it.
- **Off** (the default) plays exactly as before.
- The server sends its data pack inside the game. It goes into `/data/JkPro/DataPacks/PES13`.

## Official builds and verification

Official JKPro releases come **only** from https://github.com/Salru13/JkPro.

| Badge | Meaning |
|---|---|
| **Official Build by XxXJoker13XxX** | The files of this copy (the title, the renderer and the JKPro files) match the list signed with the release key (Ed25519). |
| **Unofficial build** | This copy could not be verified. It may have been changed or repackaged by someone else. It still runs normally. |

Forks are welcome under the code licences, but they must not call themselves official.

## FAQ

### Where does my game go?

Copy the whole PES 2013 PC folder into `/data/JkPro/PES13`, over FTP or with a USB drive. `pes2013.exe` must be directly in this folder. Then open JKPro and press PATCH. Only use a copy you own.

Folder names on the PS4 are case-sensitive: use exactly `/data/JkPro`. The easiest way: open JKPro once first; it makes the folders for you.

### PATCH says it cannot find the game's code (E210 / E211)

PATCH needs the `.reg` file that holds your game's registry code. Put it in `/data/JkPro/PES13` (or `/data/JkPro`, or `USB:/JKPro/`) and press PATCH again. E210: no `.reg` found. E211: the `.reg` has no code.

### Check game files

The tile shows **Check game files** when `pes2013.exe` or other game files are missing. Copy the whole folder again; Help in JKPro lists what to check.

### My option file (EDIT.bin) does not load

The option file is `EDIT.bin`, not `OPTION.bin` (that one holds settings only). An option file made for a data pack needs that data pack installed (DATA PACKS).

### Where are my saves?

In `/data/JkPro/Saves/PES13/`. SAVE GAMES shows the exact path, the Import folder and where backups go. A backup is made before every import, every restore, and before PLAY when your saves changed.

### Can I delete things in Import?

Yes. Import is yours: JKPro empties it after a checked copy, and you can add, replace or delete files there over FTP.

### The game closes when I scroll fast through teams (Kitserver)

Turn on **SETTINGS → Kitserver face fix**. Some option files give players a face that does not exist in the game's data, and Kitserver's face module then makes the game read a texture that is not there. The fix shows the game's default face for those players.

### Online: "Connecting to network" or "Different data"

- **Connecting to network** that never finishes: check that **SETTINGS → Online** is set to ThirteenServer, and that the PS4 has internet.
- **The server changed its address:** put a file `online-server.txt` in `/data/JkPro/System/` with the server's lines in hosts-file form, for example `95.214.55.111 ntl.konamionline.com`. JKPro uses that file instead of its built-in list.
- **Different data:** the data packs do not match. Let the game download the server's data pack, and do not install another pack under DATA PACKS while you play online.

### 30 FPS or 60 FPS?

Always 60 FPS at 1920×1080 High. These settings are locked so the game runs the same on every console.

### Logs, Clear logs and the support bundle

- **Logs** (SETTINGS → Logs & Support) is off by default. With it off, JKPro writes no diagnostic logs. Turn it on only when something goes wrong.
- **Save support bundle** collects the logs of the last runs and your settings into one folder in `/data/JkPro/Logs/support/`. It holds no saves and no registry file. With a USB drive plugged in, it is copied to `USB:/JKPro-support/` too.

### The game didn't start

In SETTINGS → Logs & Support turn on Logs, play again, then save a support bundle.

### Official build or unofficial build?

The corner badge says "Official Build by XxXJoker13XxX" only for a copy that matches the signed list of the JKPro project; every other copy says "Unofficial build". Get the official release from github.com/Salru13/JkPro.

## Support

JKPro has no contact address. If something goes wrong:

1. Read the [FAQ](#faq) and Help in JKPro.
2. Make a support bundle (**SETTINGS → Logs & Support → Save support bundle**) and copy its folder to a PC over FTP.
3. Open an issue on [GitHub Issues](https://github.com/Salru13/JkPro/issues): say what you did and what you saw, and attach the support bundle files.

## Repository layout

| Path | What |
|---|---|
| `launcher/` | The JKPro launcher: PLAY / PATCH / SETTINGS / SAVE GAMES / DATA PACKS / Help and the Official Build check. Host tests: `launcher/tests/run.sh` (Linux/WSL). |
| `launcher/jkflow/` | How the PS4 title is built on the JkFlow 1.00 runtime, and how the package is made and signed. |
| `engine/` | Engine patches, the Kitserver face-fix scripts and tests, and development tools (not shipped). |
| `docs/` | Release record, status and plans. |
| `README-player.md` | The player guide. |
| `.github/` | Ko-fi funding link. |
| `LICENSE`, `LICENSES.md`, `THIRD_PARTY.md`, `SOURCES.txt`, `CREDITS.md`, `LICENSES/` | Licences, sources and credits. |

## Building

Launcher host tests (Linux or WSL; Ubuntu 24.04 tested):

```bash
sudo apt install build-essential python3 libfreetype-dev
bash launcher/tests/run.sh
```

The package build is `launcher/jkflow/make_pkg_jkpro.sh` (after `apply_v01.sh` and `build-launcher.sh` on the JkFlow 1.00 runtime).

## Licence

- The JKPro launcher, the title integration and the build scripts in this repository are LGPL-2.1-or-later ([LICENSE](LICENSE), [LICENSES/LGPL-2.1.txt](LICENSES/LGPL-2.1.txt)).
- The Wine PS4 runtime is JkFlow's and keeps Wine's LGPL-2.1-or-later; where its source is: [SOURCES.txt](SOURCES.txt).
- Native DXVK (`dxvknative.prx`, `d3d9.dll`) is free to use with the credit line **"Native DXVK by XxXJoker13XxX"** ([LICENSES/JkFlow-Native-DXVK.txt](LICENSES/JkFlow-Native-DXVK.txt)). The DXVK base stays zlib/libpng.
- What ships in the package and under which licence: [LICENSES.md](LICENSES.md), [THIRD_PARTY.md](THIRD_PARTY.md).

## Credits

- Kitserver 13 by Juce, Robbie and Jenkey1002: the player's own mod, not shipped.
- [orbis-win](https://github.com/plastikman/orbis-win) and the PS4 Wine port by [plastikman](https://github.com/plastikman); the original Wine console port by [mpereiraesaa](https://github.com/mpereiraesaa).
- Wine, Mesa/RADV, [DXVK](https://github.com/doitsujin/dxvk) (Philip Rebohle, Joshua Ashton), LLVM: the stack underneath. TweetNaCl (public domain) checks the Official Build signature.
- The JKPro launcher, the Wine runtime from [JkFlow](https://github.com/Salru13/JkFlow) and Native DXVK by XxXJoker13XxX.

All names: [CREDITS.md](CREDITS.md).

## Disclaimer

JKPro is an unofficial fan project. It is **not affiliated with, made by or endorsed by Konami**. Pro Evolution Soccer and PES are trademarks of Konami. JKPro contains no Konami game files; you use your own copy of the game. PlayStation and PS4 are trademarks of Sony Interactive Entertainment; JKPro is not affiliated with Sony. JKPro is not affiliated with the authors of Kitserver. Homebrew on a PS4 is at your own risk.
