# 10-deps-vcpkg.ps1 - build Apache's third-party libs from source with vcpkg (manifest mode)
# OpenSSL comes from ports-overlay\openssl (3.5 LTS) instead of the vcpkg default.
$ErrorActionPreference = 'Stop'
$Root  = Split-Path $PSScriptRoot -Parent
$Vcpkg = "$Root\toolchain\vcpkg\vcpkg.exe"
$Out   = "$Root\build\vcpkg_installed"
New-Item -ItemType Directory -Force "$Root\build\logs" | Out-Null

# private vcpkg checkout (never the shared C:\vcpkg), at least as new as the manifest baseline
if (-not (Test-Path $Vcpkg)) {
    $baseline = (Get-Content "$Root\deps\apache\vcpkg.json" -Raw | ConvertFrom-Json).'builtin-baseline'
    git clone --quiet https://github.com/microsoft/vcpkg.git "$Root\toolchain\vcpkg"
    git -C "$Root\toolchain\vcpkg" checkout --quiet $baseline
    & "$Root\toolchain\vcpkg\bootstrap-vcpkg.bat" -disableMetrics
    if (-not (Test-Path $Vcpkg)) { throw '[ERR] vcpkg bootstrap failed' }
}

$env:Path = ($env:Path -split ';' | Where-Object { $_ -notmatch '(?i)xampp' }) -join ';'
$env:VCPKG_DISABLE_METRICS = '1'
& $Vcpkg install --triplet x64-windows `
    --x-manifest-root="$Root\deps\apache" `
    --x-install-root="$Out" `
    --overlay-ports="$Root\ports-overlay" `
    --clean-after-build 2>&1 | Tee-Object "$Root\build\logs\vcpkg-apache.log" | Select-Object -Last 25
if ($LASTEXITCODE) { throw "[ERR] vcpkg install failed (log: build\logs\vcpkg-apache.log)" }
Write-Host "[OK] deps -> $Out\x64-windows"
