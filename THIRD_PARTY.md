# Third-party components

JKPro runs Pro Evolution Soccer 2013 (PC) on the PS4 through the components below. Full licence texts are in `LICENSES/`. Where to get the source for each component, with our changes, is listed in `SOURCES.txt`.

The JKPro launcher and the other code in this repository are LGPL-2.1-or-later (Copyright 2026 XxXJoker13XxX and contributors). Our native DXVK (`dxvknative.prx` and the `d3d9` DLLs) is free to use with credit ("Native DXVK by XxXJoker13XxX"). Both are described in `LICENSE`.

The game itself is not part of JKPro. The player supplies their own copy. JKPro contains no files from Konami and no Kitserver files.

| Component | What it does here | Licence | Text |
|---|---|---|---|
| JKPro launcher (ours) | The app: PLAY, PATCH, SETTINGS, SAVE GAMES, DATA PACKS, Help | LGPL-2.1-or-later | `LICENSES/LGPL-2.1.txt` |
| Wine PS4 runtime of JkFlow 1.00 (Wine 11.17 base) | Runs the game's Windows code (`sce_module/*.prx`, `win/`) | LGPL-2.1-or-later | `LICENSES/Wine-LICENSE.txt`, `LICENSES/Wine-AUTHORS.txt`, `LICENSES/Wine-NOTICES.md`, `LICENSES/Wine-bundled-libraries.txt` |
| Mesa / RADV | The Vulkan driver for the console's GPU (`libvulkan.prx`) | MIT | `LICENSES/Mesa-MIT.txt`, `LICENSES/Mesa-other-licenses.txt` |
| DXVK 1.10.3, the base of Native DXVK | Turns Direct3D 9 into Vulkan | zlib/libpng | `LICENSES/DXVK-zlib.txt`, `LICENSES/DXVK-bundled-notices.txt` |
| Native DXVK (ours) | Our changes to DXVK for the console | Free to use with credit | `LICENSES/JkFlow-Native-DXVK.txt` |
| FreeType | Font rendering | FreeType License | `LICENSES/FreeType-FTL.txt`, `LICENSES/FreeType-notices.txt` |
| LLVM runtime | Runtime support | Apache-2.0 with LLVM exception | `LICENSES/LLVM-Apache-2.0-with-LLVM-exception.txt`, `LICENSES/Apache-2.0.txt` |
| musl libc, orbis-compat | C runtime | MIT | `LICENSES/musl-MIT.txt`, `LICENSES/orbis-compat-MIT.txt` |
| mingw-w64 runtime | In the PE DLLs | ZPL-2.1 and others | `LICENSES/mingw-w64-runtime.txt` |
| puff (zlib contrib) | DEFLATE | Zlib | `LICENSES/zlib-puff.txt` |
| Nunito | Font | SIL OFL 1.1 | `LICENSES/Nunito-OFL-1.1.txt` |
| TweetNaCl | Checks the Official Build signature | Public domain | `launcher/third_party/` |

## Thanks

Thanks to [plastikman](https://github.com/plastikman) for orbis-win and his PS4 Wine port, and to [mpereiraesaa](https://github.com/mpereiraesaa) for the original Wine console port. The Wine PS4 runtime of [JkFlow](https://github.com/Salru13/JkFlow) is built from them.
