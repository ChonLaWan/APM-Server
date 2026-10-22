# 00-fetch.ps1 - download + verify + extract everything pinned in versions.json
# Usage: pwsh scripts\00-fetch.ps1 [-Only php,httpd] [-Force]
param([string[]]$Only, [switch]$Force)
$ErrorActionPreference = 'Stop'
# `pwsh -File ... -Only php,httpd` passes ONE string "php,httpd" (no array parsing in -File mode)
$Only = @($Only | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$Root = Split-Path $PSScriptRoot -Parent
$V    = Get-Content "$Root\versions.json" -Raw | ConvertFrom-Json
$LockFile = "$Root\versions.lock.json"
$Lock = if (Test-Path $LockFile) { Get-Content $LockFile -Raw | ConvertFrom-Json -AsHashtable } else { @{} }
$Dl   = "$Root\downloads"; New-Item -ItemType Directory -Force $Dl, "$Root\src" | Out-Null

function Get-One($name, $item, $destDir, $strip) {
    if ($Only -and $name -notin $Only) { return }
    if (-not $item.url) { return }   # git-based sources (imagemagick) are fetched by their own build script
    $file = Join-Path $Dl ([IO.Path]::GetFileName(([uri]$item.url).AbsolutePath))
    if (-not (Test-Path $file) -or $Force) {
        Write-Host "[GET] $name $($item.version)"
        curl.exe -fsSL --retry 3 -o "$file.part" $item.url
        if ($LASTEXITCODE) { throw "download failed: $($item.url)" }
        Move-Item -Force "$file.part" $file
    }
    $sha = (Get-FileHash $file -Algorithm SHA256).Hash.ToLower()
    $want = if ($item.sha256) { $item.sha256 } elseif ($Lock[$name]) { $Lock[$name] } else { $null }
    if ($want -and $sha -ne $want) { throw "[ERR] sha256 mismatch $name`n  got  $sha`n  want $want" }
    if ($item.sha1) {
        $s1 = (Get-FileHash $file -Algorithm SHA1).Hash.ToLower()
        if ($s1 -ne $item.sha1) { throw "[ERR] sha1 mismatch $name" }
    }
    if (-not $want) { Write-Host "[TOFU] $name sha256=$sha" }
    $Lock[$name] = $sha

    if ((Test-Path $destDir) -and -not $Force) { Write-Host "[OK] $name (already extracted)"; return }
    if (Test-Path $destDir) { Remove-Item $destDir -Recurse -Force }
    New-Item -ItemType Directory -Force $destDir | Out-Null
    tar.exe -xf $file -C $destDir --strip-components=$strip
    if ($LASTEXITCODE) { throw "extract failed: $file" }
    Write-Host "[OK] $name -> $destDir"
}

foreach ($p in $V.toolchain.PSObject.Properties) {
    Get-One $p.Name $p.Value (Join-Path $Root $p.Value.dest) $p.Value.strip
}
foreach ($p in $V.sources.PSObject.Properties) {
    Get-One $p.Name $p.Value (Join-Path "$Root\src" "$($p.Name)-$($p.Value.version)") 1
}
$Lock | ConvertTo-Json | Set-Content $LockFile -Encoding utf8
