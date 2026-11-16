# SourceAssets

Free (CC0 / OFL) source files that `Tools/Setup.bat` imports into `Content/NSC/`. They are **not stored in git**
(~830 MB would blow GitHub's 1 GB LFS storage/bandwidth quota after one clone). Instead:

* `Tools/Setup.bat` downloads `NSC-SourceAssets-v1.zip` from the repo's `assets-v1` GitHub release
  (a data-only pre-release, not a game build) and extracts it here, then runs the editor import.
* If that zip is not available, rebuild the folder yourself (see below), or drop it here by hand.

`manifest.json` (tracked in git) lists every asset: name, files, source URL, author, license, and an
`aiGenerated` flag for AI-assisted models (Polyy.AI guns, 3dassets.dev props).

## Layout
| Folder | Contents | Source |
|---|---|---|
| `Characters/` | `Soldier_Male.glb`, `Soldier_Female.glb` with 31 animations each | Quaternius UBC + UAL1/UAL2 (CC0), merged with `Tools/blender/merge_character.py` |
| `Weapons/<Id>/` | one model per weapon id plus BC/N/ORM textures and `material.json` | Stein Games, 3DModelsCC0, Polyy.AI (CC0) |
| `Weapons/Attachments/` | scopes, silencers, grip, flashlight, bipod | Quaternius (CC0) |
| `Props/PolyHaven/` | ~95 warehouse/dock props (glTF, 2K) | Poly Haven (CC0) |
| `Props/3DAssets/` | gear (helmet, plate carrier, medkit, tourniquet...), dock props, attachments | 3dassets.dev (CC0, AI-generated) |
| `Textures/PolyHaven/` | PBR surfaces (concrete, metal, asphalt, ...) | Poly Haven (CC0) |
| `HDRI/` | 6 skies | Poly Haven (CC0) |
| `Audio/` | gunshots + reloads per weapon, footsteps per surface, impacts, grenades, UI | Stein Games, Free Firearm Sound Library, Kenney (CC0); some generated |
| `FX/`, `Decals/` | particle sprites; bullet-hole and blood decals | Kenney (CC0); generated |
| `Fonts/` | Rajdhani | Indian Type Foundry (OFL-1.1) |

## Rebuilding from sources (maintainers)
1. `python Tools/fetch_polyhaven.py` and `python Tools/fetch_3dassets.py` (direct API downloads).
2. Download the itch.io packs with `python Tools/itch_download.py <page> <rawdir>` (pages listed in `manifest.json`),
   the Kenney packs from kenney.nl and the Free Firearm Sound Library from OpenGameArt, and extract them.
3. Merge each character with Blender 4.x: `blender -b -P Tools/blender/merge_character.py -- <UBC char.gltf> <UAL1.glb> <UAL2.glb> SourceAssets/Characters/Soldier_Male.glb`.
4. `python Tools/curate_assets.py <rawdir>` (needs Pillow, ffmpeg and Node/npx) writes the rest plus `manifest.json`.
