# 50-manager.ps1 - build APM Manager (Qt 6 Widgets, MSVC x64) and lay it out with its Qt runtime in stage\manager
#   pwsh -File scripts\50-manager.ps1 [-QtDir C:\Qt\6.10.0\msvc2022_64]
param([string]$QtDir = $(if ($env:QT_DIR) { $env:QT_DIR } else { 'C:\Qt\6.10.0\msvc2022_64' }))
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot\lib.ps1"
$Bld = "$Root\build\manager"
$Out = "$Root\stage\manager"
$Log = "$Root\build\logs\manager.log"
New-Item -ItemType Directory -Force "$Root\build\logs" | Out-Null
Remove-Item $Log -ErrorAction SilentlyContinue
if (-not (Test-Path "$QtDir\bin\windeployqt.exe")) { throw "[ERR] Qt not found at $QtDir (set -QtDir or QT_DIR)" }
Enter-VsEnv

Invoke-Step 'manager configure' { cmake --fresh -S "$Root\manager" -B $Bld -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtDir" } $Log
Invoke-Step 'manager build'     { cmake --build $Bld } $Log

if (Test-Path $Out) { Remove-Item $Out -Recurse -Force }
New-Item -ItemType Directory -Force $Out | Out-Null
Copy-Item "$Bld\APMManager.exe" $Out
Invoke-Step 'windeployqt' {
    & "$QtDir\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw `
        --no-compiler-runtime --skip-plugin-types generic,networkinformation,qmltooling "$Out\APMManager.exe"
} $Log
Write-Host "[OK] manager -> $Out ($([math]::Round((Get-ChildItem $Out -Recurse -File | Measure-Object Length -Sum).Sum/1MB,1)) MB)"
