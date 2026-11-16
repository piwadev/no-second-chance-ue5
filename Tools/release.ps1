<#
.SYNOPSIS
  NO SECOND CHANCE release script: version bump -> package -> zip -> sha256 -> manifest.json -> GitHub release.

.EXAMPLE
  .\Tools\release.ps1 -Bump patch -Notes "Fixes"            # package + publish v0.1.1 (game only)
  .\Tools\release.ps1 -Version 0.2.0 -Launcher               # also build NSC-Launcher-Setup.exe and attach it
  .\Tools\release.ps1 -Bump minor -SkipPackage -DryRun       # reuse Build\Windows, write files, publish nothing

.NOTES
  Needs: Unreal Engine (UE_ROOT or auto-detect, used by Package.bat), GitHub CLI `gh` (logged in),
  Windows 10+ tar.exe, and for -Launcher: Node.js 20+.
  GitHub release assets must be < 2 GiB each: bigger game zips are split into .001/.002 parts which the
  launcher downloads, verifies and joins (manifest.game.parts).
#>
[CmdletBinding()]
param(
  [ValidateSet('major', 'minor', 'patch', 'none')] [string] $Bump = 'patch',
  [string] $Version,
  [string] $Notes = '',
  [switch] $Launcher,
  [switch] $SkipPackage,
  [switch] $DryRun,
  [switch] $Draft,
  [string] $Repo = 'piwadev/no-second-chance-ue5',
  [string] $MinLauncher = '2.0.0',
  [long] $PartSize = 1900MB
)
$ErrorActionPreference = 'Stop'
$Root = Resolve-Path (Join-Path $PSScriptRoot '..')
$Ini = Join-Path $Root 'Config\DefaultGame.ini'
$Out = Join-Path $Root 'Build\Release'
function Step($m) { Write-Host "[release] $m" -ForegroundColor Cyan }
function Sha($f) { (Get-FileHash -Algorithm SHA256 -LiteralPath $f).Hash.ToLower() }

# ---------------------------------------------------------------- version
$iniText = Get-Content -Raw $Ini
$cur = [regex]::Match($iniText, '(?m)^ProjectVersion=(.+)$').Groups[1].Value.Trim()
if (-not $cur) { $cur = '0.0.0' }
if ($Version) { $new = $Version.TrimStart('v') }
else {
  $p = $cur.Split('.') | ForEach-Object { [int]$_ }
  while ($p.Count -lt 3) { $p += 0 }
  switch ($Bump) { 'major' { $p[0]++; $p[1] = 0; $p[2] = 0 } 'minor' { $p[1]++; $p[2] = 0 } 'patch' { $p[2]++ } 'none' { } }
  $new = "$($p[0]).$($p[1]).$($p[2])"
}
Step "version $cur -> $new"
if (-not $DryRun) {
  $iniText = [regex]::Replace($iniText, '(?m)^ProjectVersion=.*$', "ProjectVersion=$new")
  [IO.File]::WriteAllText($Ini, $iniText)
}

# ---------------------------------------------------------------- package
if (-not $SkipPackage) {
  Step 'packaging (Tools\Package.bat)'
  & cmd /c "`"$Root\Tools\Package.bat`" Shipping"
  if ($LASTEXITCODE -ne 0) { throw 'Package.bat failed' }
}
$GameDir = Join-Path $Root 'Build\Windows'
if (-not (Test-Path (Join-Path $GameDir 'NoSecondChance.exe'))) { throw "No packaged build in $GameDir - run without -SkipPackage" }
@{ version = $new; built = (Get-Date).ToUniversalTime().ToString('o') } | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $GameDir 'version.json')

# ---------------------------------------------------------------- zip + split
New-Item -ItemType Directory -Force $Out | Out-Null
Get-ChildItem $Out -File | Remove-Item -Force
$zipName = "NSC-Game-$new.zip"
$zip = Join-Path $Out $zipName
Step "zipping $GameDir -> $zipName"
Push-Location $GameDir
& tar.exe -a -c -f $zip *
if ($LASTEXITCODE -ne 0) { Pop-Location; throw 'tar.exe failed to create the zip' }
Pop-Location
$zipSize = (Get-Item $zip).Length
$game = [ordered]@{ file = $zipName; sha256 = (Sha $zip); size = $zipSize }
$uploads = @()
if ($zipSize -gt $PartSize) {
  Step ("splitting {0:N0} MB into {1:N0} MB parts" -f ($zipSize / 1MB), ($PartSize / 1MB))
  $parts = @(); $buf = New-Object byte[] (8MB); $in = [IO.File]::OpenRead($zip); $n = 0
  try {
    while ($in.Position -lt $in.Length) {
      $n++; $pn = '{0}.{1:D3}' -f $zipName, $n; $pf = Join-Path $Out $pn
      $o = [IO.File]::Create($pf); $left = $PartSize
      try { while ($left -gt 0) { $r = $in.Read($buf, 0, [int][Math]::Min($buf.Length, $left)); if ($r -le 0) { break }; $o.Write($buf, 0, $r); $left -= $r } } finally { $o.Close() }
      $parts += [ordered]@{ file = $pn; sha256 = (Sha $pf); size = (Get-Item $pf).Length }
      $uploads += $pf
    }
  } finally { $in.Close() }
  $game.parts = $parts
} else { $uploads += $zip }

# ---------------------------------------------------------------- launcher
$launcherEntry = $null
if ($Launcher) {
  Step 'building the launcher (npm ci + electron-builder)'
  Push-Location (Join-Path $Root 'launcher')
  & npm ci; if ($LASTEXITCODE -ne 0) { Pop-Location; throw 'npm ci failed' }
  & npm run dist; if ($LASTEXITCODE -ne 0) { Pop-Location; throw 'electron-builder failed' }
  $lv = (Get-Content -Raw package.json | ConvertFrom-Json).version
  Pop-Location
  $setup = Join-Path $Root 'launcher\dist\NSC-Launcher-Setup.exe'
  Copy-Item $setup (Join-Path $Out 'NSC-Launcher-Setup.exe') -Force
  $launcherEntry = [ordered]@{ version = $lv; file = 'NSC-Launcher-Setup.exe'; sha256 = (Sha $setup); size = (Get-Item $setup).Length }
  $uploads += (Join-Path $Out 'NSC-Launcher-Setup.exe')
} else {
  # keep advertising the newest published launcher so self-update keeps working
  try {
    $prev = & gh release download --repo $Repo --pattern manifest.json --output - 2>$null | ConvertFrom-Json
    if ($prev.launcher) { $launcherEntry = $prev.launcher; $launcherEntry | Add-Member -NotePropertyName url -NotePropertyValue "https://github.com/$Repo/releases/download/v$($prev.version)/$($prev.launcher.file)" -Force }
  } catch { }
}

# ---------------------------------------------------------------- manifest
$manifest = [ordered]@{
  name = 'NO SECOND CHANCE'; engine = 'Unreal Engine 5'; version = $new; released = (Get-Date).ToUniversalTime().ToString('o')
  exe = 'NoSecondChance.exe'; minLauncher = $MinLauncher; notes = $Notes; game = $game
}
if ($launcherEntry) { $manifest.launcher = $launcherEntry }
$mf = Join-Path $Out 'manifest.json'
$manifest | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 $mf
$uploads += $mf
"$($game.sha256)  $zipName" | Set-Content -Encoding ASCII (Join-Path $Out 'SHA256SUMS.txt')
$uploads += (Join-Path $Out 'SHA256SUMS.txt')
Step ("assets: " + (($uploads | ForEach-Object { Split-Path $_ -Leaf }) -join ', '))

# ---------------------------------------------------------------- publish
if ($DryRun) { Step 'dry run: nothing committed or published'; return }
& git -C $Root add Config/DefaultGame.ini
& git -C $Root commit -m "Release v$new" | Out-Null
& git -C $Root tag "v$new"
& git -C $Root push origin HEAD --tags
$ghArgs = @('release', 'create', "v$new") + $uploads + @('--repo', $Repo, '--title', "NO SECOND CHANCE $new", '--notes', $(if ($Notes) { $Notes } else { "Release $new" }))
if ($Draft) { $ghArgs += '--draft' }
& gh @ghArgs
if ($LASTEXITCODE -ne 0) { throw 'gh release create failed' }
Step "published v$new"
