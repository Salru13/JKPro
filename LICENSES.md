# Licences

This page lists what ships inside the JKPro PKG and under which licence, what JKPro does **not** ship and why, and the licence of JKPro's own code. The licence texts are in [LICENSES/](LICENSES/) and also ship inside the PKG (`LICENSE`, `LICENSES/`). Credits are in [CREDITS.md](CREDITS.md).

## 1. What ships in the PKG

| Component | File(s) in the PKG | Licence |
|---|---|---|
| JKPro launcher (ours) | `eboot.bin`, `sce_module/relocated_wine_app.prx` | LGPL-2.1-or-later |
| Native DXVK (ours) | `jkpro-files/d3d9.dll`, `jkpro-files/d3d9-pe.dll`, `sce_module/dxvknative.prx` | Free to use with credit "Native DXVK by XxXJoker13XxX"; the DXVK base is zlib/libpng |
| DXVK 1.10.3 (base of Native DXVK) | inside the DLLs | zlib/libpng |
| Wine PS4 runtime of JkFlow 1.00 (Wine 11.17 base) | `sce_module/*.prx`, `win/` | LGPL-2.1-or-later |
| Mesa / RADV | `sce_module/libvulkan.prx` | MIT (each file's SPDX line governs) |
| FreeType, LLVM runtime, musl, orbis-compat, mingw-w64 runtime, puff | inside the runtime | FTL, Apache-2.0 with LLVM exception, MIT, ZPL-2.1, Zlib |
| Fonts | inside the title | SIL OFL 1.1 / Ubuntu Font Licence 1.0 |
| TweetNaCl | inside `eboot.bin` (signature check) | Public domain |

The licence text files, one per component, are in [LICENSES/](LICENSES/).

## 2. What JKPro does not ship

- **Pro Evolution Soccer 2013** and any Konami file: you use your own copy.
- **Kitserver 13**: your own mod. Its licence allows redistribution of unmodified binaries only, so JKPro never ships a Kitserver file; the optional face fix changes your own `fserv.dll` on the console and keeps the original.

## 3. JKPro's own code

LGPL-2.1-or-later, Copyright 2026 XxXJoker13XxX and contributors. See [LICENSE](LICENSE).
