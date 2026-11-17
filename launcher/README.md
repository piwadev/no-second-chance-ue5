# NSC Launcher (Unreal Engine build)

Electron launcher + auto-updater for **NO SECOND CHANCE** (the UE5 game in this repo). Installer: `NSC-Launcher-Setup.exe`
(NSIS, one-click, per-user, desktop + start-menu shortcut).

## What it does
* **Update check** – `GET https://api.github.com/repos/piwadev/no-second-chance-ue5/releases/latest`, reads the release's
  `manifest.json` (written by `Tools/release.ps1`). If the REST API is rate-limited it falls back to
  `releases/latest/download/manifest.json`.
* **Download + verify** – downloads `NSC-Game-<ver>.zip` (or its `.001/.002…` parts when the build is over ~1.9 GB),
  checks the SHA-256 of every part and of the joined zip, then **streams** the extraction (yauzl, zip-slip guarded) into
  `%LOCALAPPDATA%\NoSecondChance\game\<ver>`. A version is only accepted if it contains `NoSecondChance.exe` and a
  `version.json` with the expected version. The previous version is kept for **rollback** (Settings).
* **Play** – starts `NoSecondChance.exe` from the current version folder, minimizes (or hides) itself and comes back
  when the game exits.
* **Saves** – the game writes its SaveGame slots to `%LOCALAPPDATA%\NoSecondChance\Saved\SaveGames` (outside the version
  folders). The launcher backs them up on play/exit (`%APPDATA%\NoSecondChance\saves\backups`, last 5), and can export /
  import the main slot (`NSCSettings.sav`, GVAS header checked) or copy it to a cloud-synced folder (`NSC-save-latest.sav` + 10 copies).
* **Self-update** – if `manifest.launcher.version` is newer than the running launcher, a banner offers to download
  `NSC-Launcher-Setup.exe` (SHA-256 checked) and runs it. `manifest.minLauncher` blocks game updates that need a newer launcher.

## manifest.json
```json
{ "name": "NO SECOND CHANCE", "version": "0.2.0", "exe": "NoSecondChance.exe", "minLauncher": "2.0.0", "notes": "...",
  "game": { "file": "NSC-Game-0.2.0.zip", "sha256": "…", "size": 2400000000,
            "parts": [ { "file": "NSC-Game-0.2.0.zip.001", "sha256": "…", "size": 1992294400 },
                       { "file": "NSC-Game-0.2.0.zip.002", "sha256": "…", "size": 407705600 } ] },
  "launcher": { "version": "2.0.0", "file": "NSC-Launcher-Setup.exe", "sha256": "…", "size": 90000000 } }
```
`parts` is optional (only for zips over the part size).

## Develop / build
```
cd launcher
npm ci
npm start                 # run unpackaged
npm test                  # end-to-end test against a local mock release server (needs playwright-core)
npm run dist              # dist/NSC-Launcher-Setup.exe (Windows, or Linux + wine)
```
Environment overrides (used by the test): `NSC_API`, `NSC_ROAMING`, `NSC_LOCAL`, `NSC_UE_SAVES`, `NSC_GAME_CMD`, `NSC_TEST`.
