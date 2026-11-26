# Candidate client d3d9.dll: native GetDisplayMode format fix (PES 2013 black frames)

Not yet run on the console. It is built and passes the host tests. The lead deploys it.

| File | sha256 | bytes |
|---|---|---|
| `d3d9.dll` (candidate) | `08cab072514f9e481a56a16454f4d9aa675f2da793bfb190cf18830712281d7a` | 6,839,061 |
| `fix.patch` | `17ceb47736cffcc62602ff317c09b17010e16a48472b212fce0bb92248042fe0` | |
| `jk_build.sh` | `66e2cb0e8a29b1d50f364c06d3b5508ae3f95fcb53f6ba63f73a503b20ab9ab8` | |
| base: release client 28c101ff | `28c101ff39c877ae9e63905d655568c62fd5bfcdea24a6ce57c9a9ae896dc207` | 6,839,067 |

## Bug

The native proxy device's `GetDisplayMode`, `GetDisplayModeEx` and the swap chain's `GetDisplayMode`
answered `Format = m_pp.BackBufferFormat` (`dxvknative_menu.inc`, `DxnDevice260`). PES 2013 creates
its device with `fmt=21`, which is A8R8G8B8. Wine's d3dx9 (`dlls/d3dx9_36/texture.c`
`check_texture_requirements`) calls `IDirect3D9::CheckDeviceFormat(params.AdapterOrdinal, params.DeviceType,
mode.Format, ...)` with that mode format as the **AdapterFormat**. DXVK (`d3d9_adapter.cpp:114`,
`IsSupportedAdapterFormat` in `d3d9_monitor.cpp`) accepts only A2R10G10B10, X8R8G8B8, X1R5G5B5 and R5G6B5 there.
With A8R8G8B8, every format check and the whole fallback search return NOTAVAILABLE, so
`D3DXCreateTextureFromFileInMemoryEx` prints `fixme:d3dx:...Couldn't find suitable texture parameters`
and fails. The PE DXVK device always answers X8R8G8B8 (`D3D9SwapChainEx::GetDisplayModeEx`), which is
why PE runs have 0 of these lines.

Fix: the three getters now answer `D3DFMT_X8R8G8B8`, as DXVK does. Width, height and refresh rate
are unchanged.

## Source

- The base is `~/nfsmw-bench/native/src6-17i` (v17i a0901c12) with NFS repo `client/codex-queue-decl.patch` (`2de7d26a…`)
  and `client/codex-world-lod.patch` (`770d6e31…`) applied, as `NFS-MostWanted-PS4/evidence/codex-re60/build.py` does.
  The NFS repo was at commit `1b932e68`, and those two patches are untracked there.
  The copy is in WSL `~/jkpro-engine-client/release-28c101ff-src`.
- Control: building that tree with `jk_build.sh release-28c101ff-src control` gives raw `06479439…`. After the 3 link-time
  fields (PE TimeDateStamp, CheckSum, export TimeDateStamp) are copied from 28c101ff, the build is
  **byte-identical to 28c101ff**. So this tree is the exact release source.
- The fixed tree is `~/jkpro-engine-client/src6-17i` (= base + `fix.patch`).

## Build (WSL)

```
bash ~/jkpro-engine-client/jk_build.sh src6-17i fix     # -> ~/jkpro-engine-client/out/fix/d3d9.c988.dll
```
`jk_build.sh` is `~/nfsmw-bench/native/scripts/build-client-native6enc17i.sh` (`68cd73e0…`) with
`N=~/jkpro-engine-client`. The tree and output names are its arguments. The DLL is raw, not restamped.

## Tests

`native-wine/native-dxvk/protocol/run_tests.sh` (the client's host suite) gained `display_mode_adapter_format` in
`test_client_server.cpp`. The test resets the native device to 960x600 with A8R8G8B8, UNKNOWN, X8R8G8B8 and R5G6B5, then
checks that all three getters answer X8R8G8B8.
- Before the fix: 6 failures, for A8R8G8B8 and R5G6B5 (`Format 21` and `Format 23`).
- After the fix: the whole suite passes, 26 PASS lines and rc=0 (`tests-fix.log`).

## What to look for in the bench (native armed, PES 2013)

1. `session.log`: `grep -c "Couldn't find suitable texture parameters"` should drop from **3648 to 0** (PE runs have 0).
   This is the direct check of this fix.
2. `d3d9.log` `BLUR_DXN_STATS`: `upload_kb` should grow well beyond 338, and `draws_per_frame` should rise above 1.
   `objects=` should be much more than 194, because textures are now created.
3. `dxn_server.txt`: `draws=` per perf line should rise above 0–7, and the shots should not be black.
4. The menu should still create (`create status=0x0 state=1 ... fmt=21`), with `errors=0` and no new `BLUR_DXN_UNSUPPORTED`.

Extra tracing for one run: `WINEDEBUG=trace+d3dx` shows `Requested format %x` and `Format chosen: %x` for each
texture load, or `Requested format is not supported, looking for a fallback`. The client has no
per-call trace for device queries. `DXVK_PS4_NATIVE_CENSUS=1` (op counts per 600 frames) and
`DXVK_PS4_NATIVE_DRAWTRACE` are the existing client knobs.

If the fixme count is 0 and the frames are still black, the cause is further down (draws). Diagnose from step 2 and 3 next.
