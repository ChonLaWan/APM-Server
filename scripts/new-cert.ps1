# new-cert.ps1 - issue a certificate from the APM local CA for extra local names (vhosts).
#   pwsh -File scripts\new-cert.ps1 -Hosts myapp.localhost,myapp.test
# Writes C:\APM\ssl\<first-host>.crt/.key. Use them in a vhost:
#   SSLCertificateFile    "${APM}/ssl/myapp.localhost.crt"
#   SSLCertificateKeyFile "${APM}/ssl/myapp.localhost.key"
# *.localhost names resolve to 127.0.0.1 in Chrome/Edge/Firefox without editing the hosts file.
param([Parameter(Mandatory)][string[]]$Hosts, [string]$Target = 'C:\APM')
$ErrorActionPreference = 'Stop'
$Hosts = @($Hosts | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$Root = Split-Path $PSScriptRoot -Parent
& "$Target\php\php.exe" -n -d "extension_dir=`"$Target\php\ext`"" -d extension=openssl "$Root\tools\mkcert.php" "$Target\ssl" @Hosts
if ($LASTEXITCODE) { throw '[ERR] certificate generation failed' }
