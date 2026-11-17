# NO SECOND CHANCE — Unreal Engine 5

5v5 tactical FPS (attack/defend bomb rounds, economy, injuries, downed state and teammate rescue) — the native
UE5 version of the browser game, with the same **Warehouse** map layout. You build it on your own Windows PC; this
repo contains the C++ game, an editor bootstrap that generates all content from free assets, the packaging/release
scripts and an auto-updating launcher.

| Folder | What |
|---|---|
| `Source/NoSecondChance` | C++ game module: game mode (rounds, economy, halftime, MR6 + overtime), game/player state, character (stances, lean, injuries, downed/revive/drag, surface footsteps, gear fit), weapons (magazine memory, recoil, spread), grenades, bomb, bots (AI controller), LAN sessions (OnlineSubsystemNull), UMG widgets built in C++, enhanced input built in C++. |
| `Content/Python` | Editor bootstrap: imports `SourceAssets/`, builds materials in code, input assets, `L_Warehouse` + `L_MainMenu` maps (layout from `warehouse_layout.json`, exported from the browser game), nav mesh, lighting. |
| `Tools/Setup.bat` | One-time setup: downloads the asset pack, builds the editor, runs the Python bootstrap. |
| `Tools/Package.bat` | `RunUAT BuildCookRun` → Win64 Shipping build in `Build\Windows`. |
| `Tools/release.ps1` | Version bump, package, zip (+ split > 1.9 GB), SHA-256, `manifest.json`, `gh release create`. |
| `launcher/` | Electron launcher (NSIS installer `NSC-Launcher-Setup.exe`): downloads/verifies/installs the game, starts `NoSecondChance.exe`, rollback, save backups, self-update. See `launcher/README.md`. |
| `SourceAssets/` | Free source assets (git-ignored; only `manifest.json` + `README.md` are tracked). Downloaded by Setup.bat. |
| `Tools/curate_assets.py`, `Tools/blender/*`, `Tools/layout/*` | How `SourceAssets/` was produced (asset curation, weapon normalisation, prop baking, character merge, first-person arms, layout export). Only needed to rebuild the asset pack. |

## Requirements (your PC)
* Windows 10/11 x64, ~40 GB free disk.
* **Unreal Engine 5.5** (5.4 / 5.6 should work — see *Engine version*) from the Epic Games Launcher.
  In the engine's *Options* keep **Editor symbols** off, and **Windows** target platform on.
* **Visual Studio 2022** (Community is fine) with *Game development with C++*, the *MSVC v143* toolset and a Windows 10/11 SDK
  (exactly what Epic lists for your engine version).
* `curl.exe` and `tar.exe` (built into Windows 10 1803+). Optional: [GitHub CLI](https://cli.github.com) (`gh auth login`) for releases,
  Node.js 20+ to build the launcher.

## Build steps
```bat
git clone https://github.com/piwadev/no-second-chance-ue5.git
cd no-second-chance-ue5
Tools\Setup.bat
```
`Setup.bat` (30–90 min the first time, mostly shader compilation):
1. downloads `NSC-SourceAssets-v1.zip` (~760 MB) from the [`assets-v1`](https://github.com/piwadev/no-second-chance-ue5/releases/tag/assets-v1)
   release, checks its SHA-256 and extracts it to `SourceAssets\` (skipped if already there);
2. builds `NoSecondChanceEditor Win64 Development` (UE's `Build.bat`);
3. starts the editor headless with `Content/Python/nsc_setup.py`: imports weapons, props, characters + animations, textures,
   HDRIs, audio, fonts → `/Game/NSC/...`, creates the materials, `IA_*`/`IMC_Default`, and builds/saves `L_Warehouse` and `L_MainMenu`.
   A summary is written to `Saved\NSC_Setup_Report.txt` (warnings = things to check by hand).

Flags: `/skipdownload` (use your own `SourceAssets\`), `/skipbuild`, `/maponly` (only rebuild the maps), `/keepopen` (leave the editor open).
Engine path: auto-detected (registry / default install folders); override with `set UE_ROOT=C:\Program Files\Epic Games\UE_5.5`.

Then:
* **Play in editor** — open `NoSecondChance.uproject`, open `/Game/NSC/Maps/L_Warehouse`, press Play (or start from `L_MainMenu`).
* **Package** — `Tools\Package.bat` (Shipping; `Tools\Package.bat Development` for a dev build) → `Build\Windows\NoSecondChance.exe`.
* **Release** (needs `gh auth login`) — `powershell -ExecutionPolicy Bypass -File Tools\release.ps1 -Bump minor -Notes "What changed"`
  packages, zips, hashes, writes `manifest.json` and publishes a GitHub release the launcher picks up. Add `-Launcher` to also
  build and ship a new `NSC-Launcher-Setup.exe`; `-DryRun` to do everything except committing/publishing; `-Draft` for a draft release.
  Builds over ~1.9 GB are uploaded as `.zip.001/.002…` parts (GitHub's 2 GB per-file limit); the launcher joins and verifies them.
* **Launcher** — `cd launcher && npm ci && npm run dist` → `launcher\dist\NSC-Launcher-Setup.exe`.

## Controls (default)
WASD move · Mouse look · Space jump · Shift sprint · Alt walk · Ctrl/C crouch · LMB fire · RMB aim · R reload ·
1/2 primary/secondary · 3/4/5 smoke/flash/frag · Wheel cycle · Q/E lean · F interact (revive / plant / defuse) · G drag downed teammate ·
X bandage · V tourniquet · B buy menu · Tab scoreboard · Esc/P pause.

## Assets & licensing
Everything in the asset pack is **CC0** (plus the Rajdhani font under SIL OFL) — see [`CREDITS.md`](CREDITS.md) and
`SourceAssets/manifest.json` for every file's source. No NC/ND or ripped assets. A few models are AI-assisted CC0 placeholders (flagged in CREDITS).

**Git LFS:** the free GitHub quota is 1 GB, so the ~760 MB asset pack lives in a release asset instead of LFS, and the generated
`Content/NSC` assets are not committed at all (they're reproducible with `Setup.bat`). `.uasset/.umap` are LFS-tracked in
`.gitattributes` in case you commit hand-made content later — mind the quota.

### Better-looking optional packs (download yourself — not redistributable here)
These are free for UE projects but their license doesn't allow putting them in a public repo, so fetch them with your own Epic account
(Fab / Epic Games Launcher → *Add to project*) into `Content/ThirdParty/` (git-ignored), then swap meshes/materials in the editor:
* **Lyra Starter Game** — animated Manny/Quinn characters, weapon animations, FX, UI (Fab, free).
* **Paragon** characters and environments (Fab, free; e.g. *Paragon: Lt. Belica*, *Paragon: Wraith*, *Paragon: Agora and Monolith*).
* **Monthly free Fab content** ("Limited-Time Free" on fab.com, claim it each month) — often industrial/military environment kits.
* Quixel Megascans on Fab (free tiers) for higher-res surfaces.
* Sketchfab **CC0** collections (weapons, props) — check each model's license and add it to `CREDITS.md`.

## Engine version
`EngineAssociation` is `5.5`. For another version right-click `NoSecondChance.uproject` → *Switch Unreal Engine version*, or set `UE_ROOT`
before running the scripts. A **dedicated server** target needs a source-built engine; LAN play (listen server) works with the launcher build.

## Saves
The game stores settings/progress via `USaveGame` (`NSCSettings.sav`) in `%LOCALAPPDATA%\NoSecondChance\Saved\SaveGames`; the
launcher backs that folder up and can export/import/cloud-copy the slot.

## Status / known risks
This project was written without a Windows machine or Unreal Engine available, so the C++ and editor scripts are **statically
reviewed but not yet compiled or run**. Expect a round of fixes on first build. See the *Known risks* list in the commit history /
setup report; the main ones: first compile errors, Interchange import naming/orientation/scale, first-person arm and gear offsets,
third-person weapon attach rotation, single-clip animations (no blendspaces/montages), AI-assisted placeholder models.
