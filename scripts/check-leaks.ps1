# check-leaks.ps1 - stop secrets and private details from reaching git.
#   pwsh -File scripts\check-leaks.ps1            # staged changes (what the pre-commit hook runs)
#   pwsh -File scripts\check-leaks.ps1 -All       # every tracked file
# Generic rules catch keys, tokens, passwords and personal Windows paths. Extra private terms (internal
# hostnames, IPs, client names ...) go one regex per line in private\leak-terms.txt, which is gitignored
# so the list itself is never published. Exit code 1 = something found, commit is refused.
param([switch]$All)
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
Push-Location $Root
try {
    $rules = [ordered]@{
        'private key'          = '-----BEGIN [A-Z ]*PRIVATE KEY-----'
        'GitHub token'         = '\b(ghp|gho|ghu|ghs|ghr)_[A-Za-z0-9]{30,}|\bgithub_pat_[A-Za-z0-9_]{40,}'
        'AWS key'              = '\bAKIA[0-9A-Z]{16}\b'
        'Slack token'          = '\bxox[abposr]-[A-Za-z0-9-]{10,}'
        'Telegram bot token'   = '\b\d{8,10}:AA[A-Za-z0-9_-]{30,}\b'
        'Google API key'       = '\bAIza[0-9A-Za-z_-]{35}\b'
        'password value'       = '(?i)\b(password|passwd|pwd|secret|api[_-]?key|token)\s*[:=]\s*[''"][^''"\s{}$]{6,}[''"]'
        'Windows profile path' = '(?i)[A-Z]:[\\/]+Users[\\/]+(?!Public|Default|<|%|\$|\*)[A-Za-z0-9._-]+[\\/]'
    }
    $termFile = 'private\leak-terms.txt'
    if (Test-Path $termFile) {
        Get-Content $termFile | Where-Object { $_ -and -not $_.StartsWith('#') } | ForEach-Object { $rules["private term /$_/"] = $_ }
    }

    $files = if ($All) { git ls-files } else { git diff --cached --name-only --diff-filter=ACMR }
    $files = @($files | Where-Object { $_ -and (Test-Path $_) -and $_ -notmatch '\.(png|ico|jpg|gif|zip)$' -and $_ -notlike 'licenses-extra/*' })
    $hits = 0
    foreach ($f in $files) {
        # staged mode checks the staged blob, not the working copy
        $text = if ($All) { Get-Content $f -Raw -ErrorAction SilentlyContinue } else { (git show ":$f") -join "`n" }
        if (-not $text) { continue }
        $lines = $text -split "`n"
        foreach ($name in $rules.Keys) {
            for ($i = 0; $i -lt $lines.Count; $i++) {
                if ($lines[$i] -match $rules[$name]) {
                    Write-Host ("[LEAK] {0}:{1}  {2}: {3}" -f $f, ($i + 1), $name, $lines[$i].Trim().Substring(0, [Math]::Min(120, $lines[$i].Trim().Length)))
                    $hits++
                }
            }
        }
    }
    if ($hits) {
        Write-Host "[ERR] $hits possible leak(s). Remove them, or move private notes to private\ (gitignored)."
        exit 1
    }
    Write-Host "[OK] no leaks in $($files.Count) file(s)$(if (-not (Test-Path $termFile)) { ' (no private\leak-terms.txt - generic rules only)' })"
} finally { Pop-Location }
