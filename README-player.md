# JKPro player guide

JKPro runs your own copy of Pro Evolution Soccer 2013 (PC) on a PS4 at 1920×1080, High, 60 FPS. It is an unofficial fan project. The game is not included: you need your own copy. Official page: github.com/Salru13/JkPro

## Where everything goes

```
/data/JkPro/                             <- made by JKPro at its first start (capital J and P)
├── PES13/                               <- your game: PES 2013 PC
│   ├── pes2013.exe                      <- directly in PES13
│   ├── PES_2013_Registry_Fix.reg        <- the .reg with your game's code
│   └── (all the other game files, kitserver/ too if you use it)
├── Saves/PES13/Import/                  <- drop EDIT.bin, OPTION.bin or dt0f.img here
├── DataPacks/PES13/                     <- installed data packs
└── Logs/                                <- logs and support bundles
```

## Before you start

- A jailbroken PS4 that can install homebrew PKGs.
- A way to copy files to the PS4: FTP or a USB drive.
- Your own PES 2013 PC folder, with the `.reg` file that holds the game's code.

## Install

1. **Install the PKG** with your homebrew PKG installer.
2. **Open JKPro once.** It makes `/data/JkPro/` with `Saves`, `DataPacks`, `Logs` and `System`. Close JKPro. Do not make these folders yourself: the names are case-sensitive.
3. **Copy your whole PES 2013 folder** into `/data/JkPro/PES13`. `pes2013.exe` must be directly in `PES13`.
4. **Open JKPro, press PATCH, then PLAY.** The tile under PES 2013 says **Ready** when everything is in place.

## Saves and the option file

- `EDIT.bin` is the option file (teams, names, kits). `OPTION.bin` holds the game settings.
- Put them in `/data/JkPro/Saves/PES13/Import/` (or `USB:/JKPro/saves/`), then **SAVE GAMES → Load saves**. Your current saves are backed up first.
- **SAVE GAMES → Restore a backup** lists every backup; pick any one.
- The game saves only when you save in the game.

## Data packs

Put `dt0f.img` in the same Import folder, then **DATA PACKS → Install**. An option file made for a data pack needs that pack installed.

## Online play (ThirteenServer)

1. Create a free account on thirteenserver.online.
2. **SETTINGS → Online → ThirteenServer**, then ✕ on the row: enter your **Online ID** and **Password** (registration code optional). Options saves.
3. Leave Kitserver on.
4. **PLAY**, choose online in the game. In each box: **L1 + touchpad** types your ID, **R1 + touchpad** your password, **L1 + R1 + touchpad** the registration code (any text works).

Your account stays on your console (`/data/JkPro/System/online-account`) and survives pkg updates.

The server sends its own data pack inside the game. If it ever changes its address, put `online-server.txt` (lines like `95.214.55.111 ntl.konamionline.com`) in `/data/JkPro/System/`.

## Kitserver 13 (your own mod)

- **SETTINGS → Kitserver**: On, Without kits, or Off.
- **SETTINGS → Kitserver face fix**: turn it On if the game closes while you scroll fast through teams. Off puts your original `fserv.dll` back.

## Logs and the support bundle

Logs are off by default. If the game does not start: SETTINGS → Logs & Support, turn on Logs, play again, then **Save support bundle** and copy its folder from `/data/JkPro/Logs/support/` to a PC.

## Help and problems

Help in JKPro explains every tile text and error. The FAQ is on the GitHub page.

## Support

JKPro has no contact address. Open an issue on github.com/Salru13/JkPro/issues with what you did, what you saw and the support bundle files.
