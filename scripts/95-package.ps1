# 95-package.ps1 - assemble the portable APM release from stage\ -> dist\APM-<ver>-win64\ + .zip
#   pwsh -File scripts\95-package.ps1 [-NoZip]
# The package runs from any folder: unzip, start manager\APMManager.exe, it sets itself up on first run.
# Nothing machine-specific goes in (no config, data, certificates, logs).
param([switch]$NoZip)
$ErrorActionPreference = 'Stop'
$Root  = Split-Path $PSScriptRoot -Parent
$V     = Get-Content "$Root\versions.json" -Raw | ConvertFrom-Json
$Stage = "$Root\stage"
$Name  = "APM-$($V.apm_version)-win64"
$Out   = "$Root\dist\$Name"
foreach ($c in 'apache','php','mariadb','manager') { if (-not (Test-Path "$Stage\$c")) { throw "[ERR] $Stage\$c missing - run build.cmd first" } }
if (Test-Path $Out) { Remove-Item $Out -Recurse -Force }
New-Item -ItemType Directory -Force $Out | Out-Null

function Copy-Tree($from, $to, [string[]]$xd = @(), [string[]]$xf = @()) {
    $a = @($from, $to, '/E', '/R:1', '/W:1', '/NFL', '/NDL', '/NJH', '/NJS', '/NP')
    if ($xd) { $a += '/XD'; $a += $xd }
    if ($xf) { $a += '/XF'; $a += $xf }
    robocopy @a | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "[ERR] copy $from failed (robocopy $LASTEXITCODE)" }
}
$dev = @('*.pdb', '*.lib', '*.exp', '*.ilk')

# 1. engines (no rendered config: httpd.conf / php.ini / my.ini come from templates on first run)
Copy-Tree "$Stage\apache"  "$Out\apache"  @("$Stage\apache\include", "$Stage\apache\lib", "$Stage\apache\logs") ($dev + 'httpd.conf')
Copy-Tree "$Stage\php"     "$Out\php"     @() ($dev + 'php.ini')
Copy-Tree "$Stage\mariadb" "$Out\mariadb" @("$Stage\mariadb\include", "$Stage\mariadb\symbols", "$Stage\mariadb\sql-bench") ($dev + 'my.ini')
Copy-Tree "$Stage\manager" "$Out\manager"
Copy-Tree "$Root\templates" "$Out\templates"
Copy-Tree "$Root\tools"     "$Out\tools"
Copy-Item "$Root\versions.json" $Out
Write-Host '[OK] engines, manager, templates, tools'

# 2. Mozilla CA bundle for PHP curl/openssl (sha256-checked)
New-Item -ItemType Directory -Force "$Out\ssl" | Out-Null
curl.exe -fsSL -o "$Out\ssl\cacert.pem" https://curl.se/ca/cacert.pem
$want = ((curl.exe -fsSL https://curl.se/ca/cacert.pem.sha256) -split '\s+')[0]
if ((Get-FileHash "$Out\ssl\cacert.pem" -Algorithm SHA256).Hash -ne $want) { throw '[ERR] cacert.pem sha256 mismatch' }
Write-Host '[OK] ssl\cacert.pem'

# 3. app-local VC++ runtime next to every exe folder (target PCs may not have the VC++ redist installed)
. "$PSScriptRoot\lib.ps1"
$redist = Get-ChildItem "$(Get-VsPath)\VC\Redist\MSVC" -Directory |
    Where-Object { Test-Path "$($_.FullName)\x64\Microsoft.VC143.CRT\vcruntime140.dll" } | Sort-Object Name | Select-Object -Last 1
if (-not $redist) { throw '[ERR] VC++ redist (Microsoft.VC143.CRT) not found in Build Tools' }
$crt = Get-ChildItem "$($redist.FullName)\x64\Microsoft.VC143.CRT\*.dll" | Where-Object Name -notlike 'vccorlib*'
foreach ($d in 'apache\bin', 'php', 'mariadb\bin', 'manager') { $crt | Copy-Item -Destination "$Out\$d" -Force }
Copy-Item "$($redist.FullName)\x64\Microsoft.VC143.OpenMP\vcomp140.dll" "$Out\php" -Force   # ImageMagick (OpenMP)
Write-Host "[OK] VC++ runtime $($redist.Name) (app-local)"

# 4. licenses (Apache-2.0, PHP-3.01, GPL-2.0 MariaDB, ImageMagick + delegates, Qt LGPL-3.0, libs)
$L = "$Out\licenses"
New-Item -ItemType Directory -Force $L | Out-Null
$lic = [ordered]@{
    'apache-httpd-LICENSE.txt' = "$Root\src\httpd-$($V.sources.httpd.version)\LICENSE"
    'apache-httpd-NOTICE.txt'  = "$Root\src\httpd-$($V.sources.httpd.version)\NOTICE"
    'apr-LICENSE.txt'          = "$Root\src\apr-$($V.sources.apr.version)\LICENSE"
    'apr-util-LICENSE.txt'     = "$Root\src\apr-util-$($V.sources.'apr-util'.version)\LICENSE"
    'mod_fcgid-LICENSE.txt'    = "$Root\src\mod_fcgid-$($V.sources.mod_fcgid.version)\LICENSE-FCGID"
    'php-LICENSE.txt'          = "$Root\src\php-$($V.sources.php.version)\LICENSE"
    'pecl-redis-LICENSE.txt'   = "$Root\src\pecl-redis-$($V.sources.'pecl-redis'.version)\LICENSE"
    'pecl-igbinary-COPYING.txt'= "$Root\src\pecl-igbinary-$($V.sources.'pecl-igbinary'.version)\COPYING"
    'pecl-imagick-LICENSE.txt' = "$Root\src\pecl-imagick-$($V.sources.'pecl-imagick'.version)\LICENSE"
    'mariadb-COPYING.txt'      = "$Root\src\mariadb-$($V.sources.mariadb.version)\COPYING"
    'imagemagick-LICENSE.txt'  = "$Root\src\imagemagick-windows\ImageMagick\LICENSE"
    'qt-LGPL-3.0.txt'          = "$Root\licenses-extra\qt-LGPL-3.0.txt"
    'qt-GPL-3.0.txt'           = "$Root\licenses-extra\qt-GPL-3.0.txt"
}
foreach ($k in $lic.Keys) { if (Test-Path $lic[$k]) { Copy-Item $lic[$k] "$L\$k" } else { Write-Host "[WARN] license missing: $($lic[$k])" } }
foreach ($p in 'openssl','curl','nghttp2','pcre2','brotli','zlib','expat','libxml2','jansson','libiconv') {
    $c = "$Root\build\vcpkg_installed\x64-windows\share\$p\copyright"
    if (Test-Path $c) { Copy-Item $c "$L\$p-LICENSE.txt" }
}
Copy-Tree "$Root\build\php-deps\share\licenses" "$L\php-deps"   # PHP-team deps (openssl, curl, icu, libxml2 ...)
Copy-Tree "$Root\src\imagemagick-windows\Artifacts\license" "$L\imagemagick-delegates"
Write-Host "[OK] licenses ($((Get-ChildItem $L -Recurse -File).Count) files)"

# 5. README for the package (build = git commit count, same numbering as APM Manager)
$build = (git -C $Root rev-list --count HEAD 2>$null); if (-not $build) { $build = 0 }
$commit = (git -C $Root rev-parse --short HEAD 2>$null); if (-not $commit) { $commit = 'nogit' }
if (git -C $Root status --porcelain --untracked-files=no 2>$null) { $commit += '-dirty' }
$stamp = (Get-Date).ToUniversalTime().ToString('yyyy-MM-dd', [cultureinfo]::InvariantCulture)
@"
APM $($V.apm_version) build $build ($commit, $stamp)
by ChonLaWan - wanpean@gmail.com - https://wanpean.com

APM $($V.apm_version) - Apache $($V.sources.httpd.version) + PHP $($V.sources.php.version) + MariaDB $($V.sources.mariadb.version) for Windows x64 (portable)

1. Unzip this folder anywhere (e.g. C:\APM or D:\Tools\APM). Paths with spaces are fine.
2. Run manager\APMManager.exe. On first run it picks free ports, writes the config,
   creates a local HTTPS certificate and an empty MariaDB database (root, no password).
3. Click "Start all", then "Open site". Your sites go in www\default\.

Moving the folder later is fine: APM Manager notices and re-creates the config.
Command line: manager\APMManager.exe --cli status|start|stop|restart [apache|mariadb|all]
Headless setup: manager\APMManager.exe --setup [--http 80 --https 443 --db 3306] [--lan]

Built from source - build scripts, docs and issues: https://github.com/ChonLaWan/APM-Server
APM scripts + APM Manager: MIT + Commons Clause - free to use; selling needs a commercial
license (wanpean@gmail.com). Third-party licenses: licenses\
"@ | Set-Content "$Out\README.txt" -Encoding utf8
Write-Host "[OK] $Out ($([math]::Round((Get-ChildItem $Out -Recurse -File | Measure-Object Length -Sum).Sum/1MB)) MB)"

if (-not $NoZip) {
    $zip = "$Root\dist\$Name.zip"
    Remove-Item $zip -ErrorAction SilentlyContinue
    Push-Location "$Root\dist"
    tar.exe -a -c -f "$Name.zip" $Name
    Pop-Location
    if ($LASTEXITCODE) { throw '[ERR] zip failed' }
    $h = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
    "$h  $Name.zip" | Set-Content "$zip.sha256" -Encoding ascii
    Write-Host "[OK] $zip ($([math]::Round((Get-Item $zip).Length/1MB)) MB) sha256 $h"
}
