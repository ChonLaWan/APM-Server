# 90-deploy.ps1 - install the built stage\ into a runtime folder (default C:\APM). Does NOT start anything.
#   pwsh -File scripts\90-deploy.ps1 [-Target C:\APM] [-HttpPort 8080] [-HttpsPort 8443] [-DbPort 3307] [-Lan]
# Re-running upgrades binaries in place; existing config files, data, www, ssl and logs are kept
# (use -ResetConfig to re-render config from templates\).
# -Lan: Apache listens on all interfaces + the cert covers this PC's name and LAN IPs. MariaDB always
#       stays on 127.0.0.1 (root has no password). Windows Firewall still needs scripts\firewall-lan.ps1 (admin).
param(
    [string]$Target = 'C:\APM',
    [int]$HttpPort = 8080, [int]$HttpsPort = 8443, [int]$DbPort = 3307,   # XAMPP keeps 80/443/3306 meanwhile
    [switch]$Lan,
    [switch]$ResetConfig
)
$Bind = if ($Lan) { '0.0.0.0' } else { '127.0.0.1' }
$LanIPs = @()
if ($Lan) {
    $LanIPs = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
        Where-Object { $_.IPAddress -notlike '127.*' -and $_.IPAddress -notlike '169.254.*' -and $_.AddressState -eq 'Preferred' } |
        ForEach-Object IPAddress
}
$ErrorActionPreference = 'Stop'
$Root  = Split-Path $PSScriptRoot -Parent
$Stage = "$Root\stage"
foreach ($c in 'apache','php','mariadb') { if (-not (Test-Path "$Stage\$c")) { throw "[ERR] $Stage\$c missing - build first" } }
foreach ($exe in 'httpd','mariadbd','php-cgi') {
    $p = Get-Process $exe -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$Target\*" }
    if ($p) { throw "[ERR] $exe from $Target is running - stop APM before deploying" }
}
# services run as LocalService: their processes are not visible to Get-Process .Path -> ask the SCM
$svc = Get-Service 'APM-Apache','APM-MariaDB' -ErrorAction SilentlyContinue | Where-Object Status -ne 'Stopped'
if ($svc) { throw "[ERR] service $($svc.Name -join ', ') is running - stop APM before deploying" }

# 1. binaries (robocopy /MIR per component, minus debug symbols and dev files)
# /XD and /XF match names on both sides, so they also keep /MIR from deleting the target's conf/logs/ini
$skip = @{
    apache  = @('/XD', 'include', 'lib', 'conf', 'logs', '/XF', '*.pdb', '*.lib', '*.exp')
    php     = @('/XF', '*.pdb', '*.lib', '*.exp', 'php.ini')
    # NOT 'lib' here: mariadb\lib\plugin holds the plugin DLLs (ha_archive, auth_ed25519, ...); *.lib files are dropped by /XF
    mariadb = @('/XD', 'include', 'symbols', 'sql-bench', '/XF', '*.pdb', '*.lib', '*.exp', 'my.ini')
}
foreach ($c in 'apache','php','mariadb') {
    robocopy "$Stage\$c" "$Target\$c" /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS /NP @($skip[$c]) | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "[ERR] copy $c failed (robocopy rc=$LASTEXITCODE)" }
    Write-Host "[OK] $c -> $Target\$c"
}
# templates + tools travel with the install: APM Manager re-renders config from them (first run, moved folder)
foreach ($d in 'templates','tools') {
    robocopy "$Root\$d" "$Target\$d" /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "[ERR] copy $d failed" }
}
# APM Manager (skipped while it runs: its exe/DLLs are locked - use its own Update dialog instead)
if (Test-Path "$Stage\manager\APMManager.exe") {
    if (Get-Process APMManager -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$Target\*" }) {
        Write-Host "[WARN] APM Manager is running - manager not updated"
    } else {
        robocopy "$Stage\manager" "$Target\manager" /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
        if ($LASTEXITCODE -ge 8) { throw '[ERR] copy manager failed' }
        Write-Host "[OK] manager -> $Target\manager"
    }
}
# licenses: APM's own LICENSE + the third-party licenses folder assembled by 95-package.ps1 (if built)
Copy-Item "$Root\LICENSE" "$Target\LICENSE.txt" -Force
$pkgLic = Get-ChildItem "$Root\dist\APM-*-win64\licenses" -Directory -ErrorAction SilentlyContinue | Select-Object -Last 1
if ($pkgLic) { robocopy $pkgLic.FullName "$Target\licenses" /MIR /NFL /NDL /NJH /NJS /NP | Out-Null }
else { Write-Host '[WARN] no dist\APM-*\licenses yet (run scripts\95-package.ps1) - licenses folder not installed' }
# Apache conf data files (mime.types, magic ...) - copy, but never overwrite an edited httpd.conf
New-Item -ItemType Directory -Force "$Target\apache\conf\extra", "$Target\apache\conf\vhosts" | Out-Null
Get-ChildItem "$Stage\apache\conf" -File | Where-Object Name -ne 'httpd.conf' | Copy-Item -Destination "$Target\apache\conf" -Force

# 2. runtime folders
foreach ($d in 'www\default','logs\apache','logs\php','logs\mariadb','tmp\sessions','tmp\uploads','tmp\opcache','ssl','data') {
    New-Item -ItemType Directory -Force "$Target\$d" | Out-Null
}

# 3. config from templates (only if missing, or -ResetConfig)
# CACHE_ID: per-folder OPcache segment name, same formula as the manager (Setup.cpp cacheId)
$sha1 = [Security.Cryptography.SHA1]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes(($Target -replace '\\','/').ToLower()))
$cacheId = 'apm' + (-join ($sha1[0..3] | ForEach-Object { $_.ToString('x2') }))
$vars = @{ APM_ROOT = ($Target -replace '\\','/'); HTTP_PORT = $HttpPort; HTTPS_PORT = $HttpsPort; DB_PORT = $DbPort; BIND = $Bind; CACHE_ID = $cacheId }
function Render($tpl, $dest, [switch]$WinPaths) {
    if ((Test-Path $dest) -and -not $ResetConfig) { Write-Host "[KEEP] $dest"; return }
    $t = Get-Content "$Root\templates\$tpl" -Raw
    foreach ($k in $vars.Keys) {
        $v = if ($WinPaths -and $k -eq 'APM_ROOT') { $Target } else { "$($vars[$k])" }
        $t = $t.Replace("{{$k}}", $v)
    }
    Set-Content $dest $t -Encoding ascii -NoNewline
    Write-Host "[OK] $dest"
}
Render 'httpd.conf'    "$Target\apache\conf\httpd.conf"
Render 'apm-ssl.conf'  "$Target\apache\conf\extra\apm-ssl.conf"
Render 'php.ini'       "$Target\php\php.ini" -WinPaths
Render 'my.ini'        "$Target\mariadb\my.ini"
Copy-Item "$Root\versions.json" "$Target\versions.json" -Force

# 4. CA bundle for curl/openssl in PHP (Mozilla bundle from curl.se, sha256-checked)
$ca = "$Target\ssl\cacert.pem"
if (-not (Test-Path $ca) -or $ResetConfig) {
    curl.exe -fsSL -o "$ca.part" https://curl.se/ca/cacert.pem
    $want = ((curl.exe -fsSL https://curl.se/ca/cacert.pem.sha256) -split '\s+')[0]
    if ((Get-FileHash "$ca.part" -Algorithm SHA256).Hash -ne $want) { Remove-Item "$ca.part"; throw '[ERR] cacert.pem sha256 mismatch' }
    Move-Item -Force "$ca.part" $ca
    Write-Host "[OK] $ca"
}

# 5. local CA + localhost certificate (generated with APM's own php.exe). The CA is created once and
#    kept; the server cert is re-issued on -ResetConfig/-Lan so its SAN matches how the site is reached.
if (-not (Test-Path "$Target\ssl\localhost.crt") -or $ResetConfig -or $Lan) {
    $certHosts = @('localhost', '127.0.0.1', '::1')
    if ($Lan) { $certHosts += @($env:COMPUTERNAME.ToLower()) + $LanIPs }
    & "$Target\php\php.exe" -n -d "extension_dir=`"$Target\php\ext`"" -d extension=openssl `
        "$Root\tools\mkcert.php" "$Target\ssl" @certHosts
    if ($LASTEXITCODE) { throw '[ERR] certificate generation failed' }
}

# 6. default page
if (-not (Test-Path "$Target\www\default\index.php")) {
    Copy-Item "$Root\templates\index.php" "$Target\www\default\index.php"
}

# 7. MariaDB datadir (bootstrap only - no network listener is opened)
if (-not (Test-Path "$Target\data\mysql\mysql")) {
    & "$Target\mariadb\bin\mariadb-install-db.exe" --datadir="$Target\data\mysql" --port=$DbPort --config="$Target\mariadb\my.ini" --silent
    if ($LASTEXITCODE) { throw '[ERR] mariadb-install-db failed' }
    Write-Host "[OK] datadir $Target\data\mysql (root@localhost, no password)"
} elseif ($ResetConfig) {
    # the server runs with <datadir>\my.ini (written once by mariadb-install-db) -> keep it in sync
    Copy-Item "$Target\mariadb\my.ini" "$Target\data\mysql\my.ini" -Force
    Write-Host "[OK] $Target\data\mysql\my.ini (synced)"
}

# 8. config syntax check (parses only, binds nothing)
& "$Target\apache\bin\httpd.exe" -t
if ($LASTEXITCODE) { throw '[ERR] httpd -t failed' }
$live = Select-String -Path "$Target\apache\conf\httpd.conf" -Pattern '^Listen\s+(\S+)' | ForEach-Object { $_.Matches[0].Groups[1].Value }
Write-Host "[OK] APM deployed to $Target  (http :$HttpPort, https :$HttpsPort, db 127.0.0.1:$DbPort) - not started"
Write-Host "     httpd.conf Listen: $live"
if ($Lan) {
    if ($live -like '127.0.0.1:*') { Write-Host '[WARN] httpd.conf was kept and still binds 127.0.0.1 - add -ResetConfig to switch it to LAN' }
    foreach ($ip in $LanIPs) { Write-Host "     LAN: http://${ip}:$HttpPort/  https://${ip}:$HttpsPort/" }
    Write-Host '     Firewall: run scripts\firewall-lan.ps1 once as Administrator'
}
