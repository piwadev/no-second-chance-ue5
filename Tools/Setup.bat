@echo off
setlocal EnableExtensions EnableDelayedExpansion
rem ============================================================================================
rem  NO SECOND CHANCE - one-time project setup (Windows)
rem    1. downloads + verifies the free source-asset pack into SourceAssets\  (skipped if present)
rem    2. builds the editor target (C++)
rem    3. runs the Python bootstrap in the editor: imports assets, builds materials, the Warehouse
rem       and main-menu maps, input assets, and saves everything.
rem  Usage:  Tools\Setup.bat [/skipdownload] [/skipbuild] [/maponly] [/keepopen]
rem  Engine: set UE_ROOT=C:\Program Files\Epic Games\UE_5.5   (auto-detected for 5.5 / 5.4)
rem ============================================================================================
set "PROJ_DIR=%~dp0.."
for %%I in ("%PROJ_DIR%") do set "PROJ_DIR=%%~fI"
set "UPROJECT=%PROJ_DIR%\NoSecondChance.uproject"
set "ASSET_TAG=assets-v1"
set "ASSET_ZIP=NSC-SourceAssets-v1.zip"
set "ASSET_URL=https://github.com/piwadev/no-second-chance-ue5/releases/download/%ASSET_TAG%"
set SKIPDL=0& set SKIPBUILD=0& set MAPONLY=0& set KEEPOPEN=0
for %%A in (%*) do (
  if /I "%%~A"=="/skipdownload" set SKIPDL=1
  if /I "%%~A"=="/skipbuild" set SKIPBUILD=1
  if /I "%%~A"=="/maponly" set MAPONLY=1
  if /I "%%~A"=="/keepopen" set KEEPOPEN=1
)

rem ---------------------------------------------------------------- engine
if not defined UE_ROOT (
  for %%V in (5.5 5.4 5.6) do (
    if not defined UE_ROOT if exist "C:\Program Files\Epic Games\UE_%%V\Engine\Binaries\Win64\UnrealEditor.exe" set "UE_ROOT=C:\Program Files\Epic Games\UE_%%V"
  )
)
if not defined UE_ROOT (
  for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.5" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE_ROOT=%%b"
)
if not defined UE_ROOT (
  echo [NSC] Unreal Engine 5.4/5.5 not found. Install it from the Epic Games Launcher or set UE_ROOT.
  exit /b 1
)
echo [NSC] Engine: %UE_ROOT%

rem ---------------------------------------------------------------- assets
if "%SKIPDL%"=="1" goto :build
if exist "%PROJ_DIR%\SourceAssets\Characters\Soldier_Male.glb" (
  echo [NSC] SourceAssets already present - skipping download ^(delete SourceAssets\Characters to force^).
  goto :build
)
echo [NSC] Downloading the CC0 asset pack ^(about 0.8 GB^) ...
if not exist "%PROJ_DIR%\Build\Download" mkdir "%PROJ_DIR%\Build\Download"
pushd "%PROJ_DIR%\Build\Download"
curl.exe -fL --retry 3 -o "%ASSET_ZIP%.sha256" "%ASSET_URL%/%ASSET_ZIP%.sha256" || goto :dlfail
curl.exe -fL --retry 3 -C - -o "%ASSET_ZIP%" "%ASSET_URL%/%ASSET_ZIP%" || goto :dlfail
for /f "usebackq tokens=1" %%h in ("%ASSET_ZIP%.sha256") do set "WANT=%%h"
for /f "usebackq delims=" %%h in (`powershell -NoProfile -Command "(Get-FileHash -Algorithm SHA256 '%ASSET_ZIP%').Hash.ToLower()"`) do set "HAVE=%%h"
if /I not "!WANT!"=="!HAVE!" (
  echo [NSC] Checksum mismatch ^(expected !WANT!, got !HAVE!^). Delete Build\Download and retry.
  popd & exit /b 1
)
echo [NSC] Extracting ...
tar -xf "%ASSET_ZIP%" -C "%PROJ_DIR%" || (popd & echo [NSC] extraction failed & exit /b 1)
popd
goto :build

:dlfail
popd
echo [NSC] Could not download %ASSET_URL%/%ASSET_ZIP%
echo [NSC] Either download it by hand and extract it into the project folder, or rebuild SourceAssets
echo [NSC] from the original sources (SourceAssets\README.md), then run Setup.bat /skipdownload.
exit /b 1

rem ---------------------------------------------------------------- C++
:build
if "%SKIPBUILD%"=="1" goto :editor
echo [NSC] Building NoSecondChanceEditor (Development, Win64) ...
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" NoSecondChanceEditor Win64 Development -Project="%UPROJECT%" -WaitMutex -NoHotReload
if errorlevel 1 (
  echo [NSC] C++ build failed - see the log above. Visual Studio 2022 with "Game development with C++" is required.
  exit /b 1
)

rem ---------------------------------------------------------------- editor bootstrap
:editor
set "NSC_SETUP_ONLY=all"
if "%MAPONLY%"=="1" set "NSC_SETUP_ONLY=map"
set "NSC_SETUP_QUIT=1"
if "%KEEPOPEN%"=="1" set "NSC_SETUP_QUIT=0"
echo [NSC] Importing assets and building maps in the editor (10-30 minutes the first time; shaders compile too) ...
"%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%UPROJECT%" -ExecutePythonScript="%PROJ_DIR%\Content\Python\nsc_setup.py" -nosplash -log=NSC_Setup.log
if exist "%PROJ_DIR%\Saved\NSC_Setup_Report.txt" (
  type "%PROJ_DIR%\Saved\NSC_Setup_Report.txt"
) else (
  echo [NSC] No report written - check Saved\Logs\NSC_Setup.log
)
echo [NSC] Done. Open NoSecondChance.uproject, press Play in L_Warehouse, or run Tools\Package.bat for a build.
endlocal
