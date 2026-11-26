# Pre-publication audit: JKPro repo before github.com/Salru13/JKPro (private)

Date: 2026-11-25. Scope: every commit reachable from `jkpro-1.0` and `main`
(74 commits at audit end, incl. 77f5702 and 95e6b1d made during the audit;
`main` is an ancestor of `jkpro-1.0`; no tags, no stash), plus the working tree
and .gitignore. Read-only: nothing pushed, committed or rewritten.

Method: `git log --all -p --text` dumped once and every added line grepped
(the `/security-review` skill could not run: it needs `origin/HEAD`, and this
repo has no remote; its checks were applied by hand). `git rev-list --objects
--all` + `cat-file` for blob sizes. All paths ever committed listed with
`git log --all --name-only`. The two late commits were grepped separately.

## Verdict

**Safe to push as-is to a PRIVATE repo. No blockers, no history rewrite
needed.** All "fix" items below are tidy-ups in the current tree (normal
commits). Before the repo is ever made public, do the "fix" items.

## Findings

| # | Item | Path / commits | Severity | Remedy |
|---|---|---|---|---|
| 1 | Game / Konami files | None in history. The only binary-ish paths ever committed: PNG screens/icons (own art, checked), Ubuntu Sans fonts (OFL, `launcher/assets/font/LICENCE.txt`), synthetic test fixtures (`launcher/tests/fixtures/**`, random bytes / own format). No .exe/.dll/.img/.bin/.dat/.afs ever committed. | ok | - |
| 2 | `"code"=` registry values | Zero hits in all history. `bench/jkpes13.reg` (ed3c28f, only version) holds `installdir` and `version` only. `"code"` appears only as a value *name* in code/tests (jk_regimport, prelaunch.sh count check). | ok | - |
| 3 | Private keys / tokens / passwords | No `BEGIN ... PRIVATE KEY`, no ghp_/github_pat_/AWS/Slack/sk- tokens, no passwords. `launcher/src/jk_release_key.h` (cab0480) is the Ed25519 **public** key; `release_keygen.sh`/`sign_release.sh` keep `release.key` in `~/.jk4speed-keys/` outside the repo. gp4 passcode is the public all-zero default. | ok | - |
| 4 | Email addresses | Only `70697807+Salru13@users.noreply.github.com` (all 74 commits, author and committer, and in file text). No @gmail/@hotmail/@outlook. | ok | - |
| 5 | Home paths / local usernames (`<user>`, WSL user `<wsl-user>`) | At tip: `bench/make_kit.sh:7,9`, `bench/runner/pes_run.py:26,119-120` (incl. a Claude scratchpad path, 19c7f29), `engine/client/candidate/jk_build.sh:7`, `engine/client/candidate/tests-fix.log:35-36`, `engine/patches/README.md:48-49`, `launcher/jkflow/make_pkg_jkpro.sh:121`, `launcher/title/author_deltas.py:40-44`, `launcher/title/make_pkg.sh:31,119`, `.coordination/inbox/001.json:21,36`, `LESSONS.md:62`. | fix (not blocker) | Replace with env vars / repo-relative paths (`$(git rev-parse --show-toplevel)`); `pes_run.py` KLOG -> CLI arg or `$TEMP`. History keeps them; acceptable for private. Rewrite only if going public and the owner wants them gone. |
| 6 | LAN IP of the bench console `<PS4 IP>` | `bench/guard/prelaunch.sh:8`, `bench/runner/native_watch.py:12`, `bench/runner/pes_run.py:24`, `bench/tools/grab.py:7,86`, `tools/klog_cap.py:1,8`, `docs/status/CASE-kernel-panic.md:4`, `.coordination/inbox/001.json:37` | fix (low) | Private RFC1918 address, not reachable from internet. Optionally read from `JK_PS4_HOST` env var. |
| 7 | `launcher/src/jk_pes13set_base.inc` - 852-byte settings.dat default image | 31738da | ok, owner note | Field values (WECF v2, resolution, flags, DirectInput keyboard GUID) of a file whose format was documented from Konami settings.exe output (`research/pes13-settings-format.md`); no Konami code/assets, no personal data (strings: only `WECF`, `DEST`). Keep; owner may prefer generating it at runtime from `bench/tools/pes13_settings.py` logic. The real samples live in ignored `evidence/pes13/settings/*.dat`. |
| 8 | Game-file name references (`.img`, `OPTION.bin`, `EDIT.bin`, `settings.dat`, `pes2013.exe`, `kload`) | research/, docs/, launcher code, PATCH.md | ok | Text references only. PATCH.md quotes a few game/kserv addresses and instructions (analysis notes), not code dumps; no hex-dump listings found in research/docs/tools/engine. |
| 9 | Own built binaries (ignored by `*.dll`, NOT in git) | `engine/client/candidate/d3d9.dll` 6,839,061 B (own DXVK-based build); `engine/jkdiag/jkdiag.dll` 23,552 B (own) | ok | Not pushed. If shipped via Releases, attach there with source refs (DXVK zlib license + fix.patch). |
| 10 | Patched Kitserver 13 binaries (ignored, NOT in git) | `engine/kitserver/kserv.dll` 294,400 B; `variants/kserv-{A-noeuro,B-nonumslots,C-guard,AC-noeuro-guard}.dll` 294,400 B each. Scripts + PATCH.md committed in 77f5702. | **resolved 2026-11-26** | Correction: Kitserver 13 is NOT GPL. Its licence (player's `kitserver/docs/license.txt`, Juce/Robbie/Jenkey1002) allows redistribution of unmodified binaries only. Decision: no Kitserver binary is ever distributed. The W7 fix is our own 28 bytes, applied in place to the player's fserv.dll by `jkpro_fservfix` (opt-in in SETTINGS). Keep `*.dll` ignored; never force-add. |
| 11 | Large blobs | Largest: 3 loader PNGs ~2.2 MB, `launcher/jkflow/icons/pic1.png` 2.2 MB, fonts 1.08/0.74 MB. Total history ~17.8 MiB. | ok | Optional: compress the 2 MB PNGs. |
| 12 | .gitignore coverage | `/evidence/` (contains `user.reg`, `jkflow-user.reg`, settings `.dat`, `log.bin` - never committed), `/.superpowers/`, `*.dll`, `*.exe`, `*.img`, `*.bin`, `/launcher/build/` all ignored. Gaps: `*.reg`, `*.dat`, `*.key`, `*.pem`, `*.log` are not globally ignored; `__pycache__` is. | fix | Add `*.dat`, `*.key`, `*.pem`, `.env`, `*.reg` with `!bench/jkpes13.reg`, and `*.log` with `!engine/client/candidate/tests-fix.log` (or drop that log, item 5). |
| 13 | Uncommitted work | `launcher/.gitignore`, `.gitattributes`, `jkpro_kitcfg.{c,h}`, `test_jkpro_kitcfg.c`, 6 modified launcher files | ok | Grepped clean (no secrets/paths/emails). |
