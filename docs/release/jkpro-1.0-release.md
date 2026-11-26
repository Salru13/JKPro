# JKPro 1.0, release record (2026-11-26)

"Official Build by XxXJoker13XxX". It runs the player's own PES 2013 PC copy on a PS4 at native 1920×1080, High, 60 FPS, full screen.

## Package

| Item | Value |
|---|---|
| File | `JKPro-1.00-JKPR00001.pkg` (529,137,664 bytes), production build |
| sha256 | `974d64bda817c4ff15b5fcb86168e59ba54d46b03463111ca2742eadd84879f2` |
| Content ID / version | `IV0000-JKPR00001_00-JKPRO00000000001` / 01.00 |
| Built from | launcher commit `2e4bb61` (branch `jkpro-1.0`): rc10 plus production settings (no diagnostic logs unless Logs is on, and a signature that covers the code) |
| Title prx | `4f8ac807b3fa2174a28298aaf77dd41dade276d29b31ad4052ac10b3061ed47c` |
| `jkpro-files/release.txt` | `ad57ac0b90153e16f73ff27d9c834024e6d235122b90a6402ae71222b8e73c3e` |
| `jkpro-files/release.sig` | `198992c77a891911…` (Ed25519, 64 bytes) |

How it was signed:
- The owner signed `release.txt` himself, with the encrypted release key and his own passphrase.
- The lead verified the signature against `launcher/src/jk_release_key.h`: "Signature Verified Successfully".
- `make_pkg_jkpro.sh` verified it again before packaging.

`release.txt` lists 7 files: `eboot.bin` `a71f3f02…`, `sce_module/relocated_wine_app.prx` `4f8ac807…`, the native server `86e62a08…` (in both `sce_module/` and `win/.../x86_64-unix/`), and:

| File | sha256 |
|---|---|
| `jkpro-files/d3d9.dll` (native client) | `08cab072514f9e481a56a16454f4d9aa675f2da793bfb190cf18830712281d7a` |
| `jkpro-files/d3d9-pe.dll` (PE fallback, c988) | `16ca3a5452026ead32fc45813b87a4cc601a4ec7b8ac009f4e502de20e12737d` |
| `jkpro-files/about/CREDITS.txt` | `4cd387a9f41c6430fd44f04328d63f4a0c351f38f22de342192d528ce633e647` |

The native server `dxvknative.prx` is `86e62a08…`. It is pinned in the packaging script.

The package contains no game, Konami or Kitserver file, and no key.

## Verified

| Gate | Evidence |
|---|---|
| Engine (native, 1080p High 60, locked) | `docs/status/BENCH-MATRIX.md`: NATSOAK 10/10; the owner accepted the native match |
| Saves, option files (EDIT.bin), data packs, Restore | W1–W4 and W6 fixed (`docs/status/WEAKNESSES.md`); owner tests on rc6–rc10 |
| Kitserver 13 fast-scroll crash (W7) | Root cause: faces (fserv). Fixed by SETTINGS > Kitserver face fix, which patches the player's `fserv.dll` in place (`jkpro_fservfix`, original `24915aa2…` → `b81aef01…`; the original is kept as `fserv.dll.jkpro-orig`). Owner-verified: 6 fast-scroll sessions and an Arsenal match with no crash; rc10 tested with the fix on and with it off. Details: `engine/kitserver/PATCH.md`. |
| rc10 owner test | PATCH, SETTINGS (Kitserver, face fix), PLAY, SAVE GAMES, DATA PACKS and Help: all OK (2026-11-26) |
| Signed release on the console | The badge shows "Official Build by XxXJoker13XxX"; the game starts (owner, 2026-11-26) |

## Licences

- Kitserver 13 (Juce & Robbie) has a BSD-style licence that allows redistribution of **unmodified** binaries only. That licence is not GPL.
- JKPro therefore ships no Kitserver file. It carries only its own 28 edited bytes, which are applied in place to the player's own `fserv.dll`, and only when the player turns the fix on.

## Known issues and follow-ups

- **W5:** the Wine caption bar shows briefly at start. The fix is identified (ps5drv GetWindowStyleMasks + WINE_PS5_UNDECORATED) and needs a reproducible win32u rebuild.
- **W7 side note:** the owner's EDIT.bin has 17 teams with saved kserv x-slots and 25 kit-slot values shared by several teams. These may give wrong kits but do not cause crashes. Possible cause: kserv's InitSlotMap GetTeamIndexById behaviour.
- With the face fix on, players whose face data is broken show the game's default face.
- **Not done in this release:** the clean-PS4 checklist on a second console (Task 6), the GitHub push (waits for the owner's go) and the YouTube video.

## Production build (2026-11-26)

- **Logs:** with Logs off (the default), nothing diagnostic is written: `DXVK_LOG_LEVEL=none`, `DXVK_PS4_LOGS=0`, `WINEDEBUG=-all`.
- **Signature:** it now covers the title and the native server, not only `jkpro-files/`.
- **Personal data:** no personal data in any shipped binary (string scan). The repository has no home paths, usernames or console IPs.
- **Engine pair:** the client `08cab072` and the server `86e62a08` are reproducible from source (byte-identical rebuild). They still contain inactive developer code, reachable only through environment variables or `/data/blurdev` files that JKPro never sets. The one side effect is a small `dxn_stage.txt` in the game folder.
- **Follow-up (1.1):** a `JKFLOW_PRODUCT` engine. The current product switch also disables 9 of the native speed-ups JKPro uses, so it needs an allow-list fix and an FPS A/B on the bench first.
