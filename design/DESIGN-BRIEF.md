# JKPro launcher: design brief

You design the **JKPro launcher**: a PS4 launcher (controller only, TV, 1920x1080) that runs the owner's own PC games. The first two are **PES 6** and **PES 2013**, and more come later. Reply to the owner in short Swedish. Use the `superpowers:brainstorming` skill first, and the artifact or design skills when you show mockups.

You work only in `design/` in this folder. The JKPro project lead (another session, working in this same repo) owns the plan, the code and the PKG decisions. Write your proposals as files in `design/`; the owner decides. Do not commit outside `design/`.

## Requirements from the owner
- The name is **JKPro**.
- The player puts a game folder under the JKPro folder on the PS4 (e.g. `/data/JKPro/PES13`, `/data/JKPro/PES6`). The launcher **finds it automatically** and shows it as a tile or card. No setup is needed to add a game.
- **Each game has its own settings:** graphics (resolution, quality, frame rate, display mode), controller mapping and anything else that game needs. Settings for one game never affect another.
- Room for more games later without redesign.

## Start from what already works
Read, read-only:
- the Blur launcher (JkFlow 1.00): `..\..\Ps4 BLUR - PC project\.claude\worktrees\blur-1.00-r4\launcher\`. Look at its screens, settings, Patch, Help, support bundle, error codes (E101, E202, ...) and texts;
- the NFS MW launcher in `..\..\NFS-MostWanted-PS4`.

Note what players found hard. For example:
- online patch steps;
- files to place by hand;
- crashes at start that needed a support bundle to explain.

## Deliver in `design/`
1. `FEATURES.md`: the full list of features and screens, each marked must-have / later. It includes:
   - the game library and how detection works;
   - the per-game settings screens;
   - controller mapping;
   - checking the game files;
   - help, the support bundle and error codes;
   - start and exit.
2. Mockups of the main screens (an HTML artifact or images is fine), controller-friendly.
3. `SETTINGS-MODEL.md`: what each game's settings file holds and where it lives on the PS4.
4. Open questions for the owner, kept short.

## Rules
- No game files and no Konami logos or art.
- Nothing published or pushed.
- The owner decides every feature.
