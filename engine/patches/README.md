# Engine patches

## 0001-wineserver-registry-crash-safe-save.patch

Status: compiled for the PS4 and tested on a host (Linux) build. Not built into a PRX,
not packaged, not tested on a PS4.

### What

`server/registry.c` of the in-process wineserver:

- `save_branch`: `fflush` + `fsync(fd)` on the temp file before `rename()`. The previous
  valid `<name>` is renamed to `<name>.bak` first (`link()` is a stub on the console, see
  `pw_wine_compat_libc.c`). After the rename, `fsync(config_dir_fd)` (the prefix
  directory). If `fsync` on the temp file fails, the old file is kept and the save is
  reported as failed.
- `load_init_registry_from_file`: if `<name>` is missing, empty or does not start with
  `WINE REGISTRY Version 2`, and `<name>.bak` does, the server loads the `.bak` and logs
  `registry: restored <name> from .bak`. The next save writes `<name>` again. An invalid
  `<name>` is never rotated over a good `.bak`.

### Why

After a kernel panic, `system.reg`, `user.reg` and `userdef.reg` were 0 bytes. The save
wrote a temp file and renamed it with no fsync. When an empty file is loaded,
`load_init_registry_from_file` returns early. It prints "is not a valid registry file",
never registers the branch for saving (so the file stays empty for good), and sets
`prefix_type` to 32-bit. Every 64-bit start then aborts in `server_init_process`. The
unpatched host wineserver reproduces this (test C0 below).

### Which tree

Apply the patch to `~/orbis-win-host-bootstrap/native247-wine-source`. Its `server/registry.c`
(sha256 `596acd84…`) rebuilds byte for byte to `native248-build-wine/build/server/registry.o`
(`96b2426a…`). The two wineservers come from these chains:

- JkFlow 1.00: `e1e793dc` ← `blur100-paths7` ← `paths6b` `83dcb508` ← `paths6` `36ec8c22`
  ← `blur100-wineserver` `2dc122bd`.
- MW Bench: `46edbb33` is `blur100-wineserver`'s control, the `native288-serverprio260` link.

Both chains link `native248-build-wine` server objects, and the configure `srcdir` is
`native247-wine-source`. The patch also applies to vanilla `wine-source` (490f6d5), at
offset -29.

### How to apply

    cd ~/orbis-win-host-bootstrap/native247-wine-source
    git apply --check <repo>/engine/patches/0001-wineserver-registry-crash-safe-save.patch
    git apply        <repo>/engine/patches/0001-wineserver-registry-crash-safe-save.patch

Then rebuild `server/registry.o` and relink `wineserver.prx` with the same chain as the
shipped one. The paths7 `/data/blurdev` → `/dev/null/bld` post-link edit has to be redone
on the new ELF. The only new import is `fsync`. The SDK's `libkernel.so` exports it, and
the shipped `ntdll.prx` already imports it.

### How it was tested (WSL, in `~/jkpro-engine-fix/`; nothing else was touched)

- PS4 compile: the `make.log` command with `prospero-clang` compiled the copied tree with no
  warnings. A control build of the unpatched file reproduced `registry.o` byte for byte.
  Compared with that control, the patched object adds only two undefined symbols:
  `fflush` and `fsync`.
- Host: the patched `registry.c` was linked with the other `wine-build` server objects,
  and an `LD_PRELOAD` shim logged `fsync` and `rename`.
  - A: fresh prefix. Each branch gets `fsync(tmp)` → `rename` → `fsync(dir)`.
  - B: resave. `<name>` → `<name>.bak`, then tmp → `<name>`.
  - C0 (unpatched, 0-byte .reg files): "not a valid registry file" ×3. Nothing is saved
    and the files stay at 0 bytes.
  - C1 (patched, 0-byte files plus `.bak`): "restored … from .bak" ×3. The files are
    rewritten with `#arch=win64` and the `.bak` is unchanged.
  - D: `system.reg` missing, `.bak` present. It is restored.
  - E: 0-byte files and no `.bak`. Same behaviour as unpatched.

### How to test on the console (lead)

1. Start once so that the `.bak` files exist.
2. Over FTP, truncate the three `.reg` files to 0 bytes.
3. Start again. Expect the restored lines in the log and a normal 64-bit start.
4. Measure the time `periodic_save` takes. It now does two fsyncs per dirty branch on
   the server thread.

Open: the patch does not recover an empty file when no `.bak` exists. The first boot after
the update has no `.bak` until the first save.
