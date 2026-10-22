# build-all.ps1 - full APM build in dependency order. Each step can also be run on its own.
#   pwsh -File scripts\build-all.ps1                  # everything, ends with dist\APM-<ver>-win64.zip
#   pwsh -File scripts\build-all.ps1 -From php        # resume from a step
#   pwsh -File scripts\build-all.ps1 -To manager      # stop after a step
param(
    [ValidateSet('fetch','vcpkg','apache','mariadb','imagemagick','php','manager','package')][string]$From = 'fetch',
    [ValidateSet('fetch','vcpkg','apache','mariadb','imagemagick','php','manager','package')][string]$To = 'package'
)
$ErrorActionPreference = 'Stop'
$steps = [ordered]@{
    fetch       = { & "$PSScriptRoot\00-fetch.ps1" }
    vcpkg       = { & "$PSScriptRoot\10-deps-vcpkg.ps1" }
    apache      = { & "$PSScriptRoot\20-apache.ps1" }
    mariadb     = { & "$PSScriptRoot\40-mariadb.ps1" }
    imagemagick = { & "$PSScriptRoot\25-imagemagick.ps1" }
    php         = { & "$PSScriptRoot\30-php.ps1" -Step all }   # after imagemagick: needs build\im-dev
    manager     = { & "$PSScriptRoot\50-manager.ps1" }
    package     = { & "$PSScriptRoot\95-package.ps1" }
}
$run = $false
foreach ($k in $steps.Keys) {
    if ($k -eq $From) { $run = $true }
    if (-not $run) { continue }
    $t = [Diagnostics.Stopwatch]::StartNew()
    Write-Host "==== $k ===="
    & $steps[$k]
    Write-Host ("==== {0} done in {1:N1} min ====" -f $k, $t.Elapsed.TotalMinutes)
    if ($k -eq $To) { break }
}
