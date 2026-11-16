@echo off
setlocal EnableExtensions
rem ============================================================================================
rem  NO SECOND CHANCE - package a Win64 Shipping build with RunUAT BuildCookRun.
rem  Output: Build\Windows\NoSecondChance.exe (+ NoSecondChance\ content, version.json)
rem  Usage:  Tools\Package.bat [Shipping^|Development]
rem  Engine: set UE_ROOT=C:\Program Files\Epic Games\UE_5.5   (auto-detected for 5.5 / 5.4)
rem ============================================================================================
set "PROJ_DIR=%~dp0.."
for %%I in ("%PROJ_DIR%") do set "PROJ_DIR=%%~fI"
set "UPROJECT=%PROJ_DIR%\NoSecondChance.uproject"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Shipping"
if not defined UE_ROOT (
  for %%V in (5.5 5.4 5.6) do (
    if not defined UE_ROOT if exist "C:\Program Files\Epic Games\UE_%%V\Engine\Build\BatchFiles\RunUAT.bat" set "UE_ROOT=C:\Program Files\Epic Games\UE_%%V"
  )
)
if not defined UE_ROOT (
  echo [NSC] Unreal Engine not found - set UE_ROOT.
  exit /b 1
)
if not exist "%PROJ_DIR%\Content\NSC\Maps\L_Warehouse.umap" (
  echo [NSC] Content\NSC\Maps\L_Warehouse.umap is missing - run Tools\Setup.bat first.
  exit /b 1
)
echo [NSC] Packaging %CONFIG% with %UE_ROOT% ...
if exist "%PROJ_DIR%\Build\Windows" rmdir /s /q "%PROJ_DIR%\Build\Windows"
call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="%UPROJECT%" -noP4 -utf8output ^
  -platform=Win64 -clientconfig=%CONFIG% ^
  -build -cook -stage -pak -iostore -compressed -prereqs -nodebuginfo ^
  -map=/Game/NSC/Maps/L_MainMenu+/Game/NSC/Maps/L_Warehouse ^
  -archive -archivedirectory="%PROJ_DIR%\Build"
if errorlevel 1 (
  echo [NSC] BuildCookRun failed - see Saved\Logs or the UAT log above.
  exit /b 1
)
if not exist "%PROJ_DIR%\Build\Windows\NoSecondChance.exe" (
  echo [NSC] Build\Windows\NoSecondChance.exe not found after packaging.
  exit /b 1
)
for /f "tokens=2 delims==" %%v in ('findstr /b /c:"ProjectVersion=" "%PROJ_DIR%\Config\DefaultGame.ini"') do set "VER=%%v"
> "%PROJ_DIR%\Build\Windows\version.json" echo {"version": "%VER%", "engine": "%UE_ROOT:\=/%", "config": "%CONFIG%"}
echo [NSC] Packaged %VER% to %PROJ_DIR%\Build\Windows
endlocal
