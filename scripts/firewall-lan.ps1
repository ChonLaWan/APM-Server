# firewall-lan.ps1 - let other machines on the same subnet reach APM's Apache (run as Administrator).
#   pwsh -File scripts\firewall-lan.ps1                 # allow (ports read from C:\APM\apache\conf)
#   pwsh -File scripts\firewall-lan.ps1 -Remove         # delete the rule
# Scope: inbound TCP to httpd.exe only, from LocalSubnet only, all profiles (Wi-Fi may be "Public").
# MariaDB is never opened (it binds 127.0.0.1 and root has no password).
param([string]$Target = 'C:\APM', [switch]$Remove)
$ErrorActionPreference = 'Stop'
$name = 'APM Apache (LAN)'
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) { throw '[ERR] run as Administrator' }

Get-NetFirewallRule -DisplayName $name -ErrorAction SilentlyContinue | Remove-NetFirewallRule
if ($Remove) { Write-Host "[OK] removed firewall rule '$name'"; return }

$conf = "$Target\apache\conf"
$ports = Select-String -Path "$conf\httpd.conf", "$conf\extra\apm-ssl.conf" -Pattern '^Listen\s+\S*:(\d+)' |
    ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique
if (-not $ports) { throw "[ERR] no Listen ports found in $conf" }
New-NetFirewallRule -DisplayName $name -Direction Inbound -Action Allow -Protocol TCP `
    -LocalPort $ports -RemoteAddress LocalSubnet -Program "$Target\apache\bin\httpd.exe" -Profile Any | Out-Null
Write-Host "[OK] firewall rule '$name': TCP $($ports -join ',') from LocalSubnet -> $Target\apache\bin\httpd.exe"
