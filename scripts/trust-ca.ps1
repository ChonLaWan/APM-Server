# trust-ca.ps1 - make this Windows user trust the APM local CA, so https://localhost:8443 shows no warning
# in Edge/Chrome (and Firefox, which reads the Windows store by default via security.enterprise_roots).
#   pwsh -File scripts\trust-ca.ps1            # trust (no admin; Windows asks for confirmation once)
#   pwsh -File scripts\trust-ca.ps1 -Remove    # untrust
# Other LAN machines: copy C:\APM\ssl\apm-ca.crt there and run the same import (or double-click the file
# -> Install Certificate -> Current User -> Trusted Root Certification Authorities).
param([string]$Target = 'C:\APM', [switch]$Remove)
$ErrorActionPreference = 'Stop'
$crtPath = "$Target\ssl\apm-ca.crt"
if (-not (Test-Path $crtPath)) { throw "[ERR] $crtPath missing - run deploy.cmd first" }
$ca = [Security.Cryptography.X509Certificates.X509Certificate2]::new($crtPath)
$store = [Security.Cryptography.X509Certificates.X509Store]::new('Root', 'CurrentUser')
$store.Open('ReadWrite')
try {
    $have = $store.Certificates | Where-Object Thumbprint -eq $ca.Thumbprint
    if ($Remove) {
        if ($have) { $store.Remove($ca); Write-Host "[OK] removed $($ca.Subject)" } else { Write-Host '[OK] not trusted - nothing to remove' }
    } elseif ($have) {
        Write-Host "[OK] already trusted: $($ca.Subject) ($($ca.Thumbprint))"
    } else {
        $store.Add($ca)   # Windows shows a security warning dialog; answer Yes
        Write-Host "[OK] trusted: $($ca.Subject) ($($ca.Thumbprint)), valid until $($ca.NotAfter.ToString('yyyy-MM-dd', [cultureinfo]::InvariantCulture))"
    }
} finally { $store.Close() }
