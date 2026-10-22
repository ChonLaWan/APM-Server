# set-path.ps1 - put APM's php + MariaDB client on PATH (for VS Code, terminals, composer, artisan).
#   pwsh -File scripts\set-path.ps1                    # Machine PATH (run as Administrator)
#   pwsh -File scripts\set-path.ps1 -Scope User        # User PATH (no admin)
#   pwsh -File scripts\set-path.ps1 -Remove            # take APM entries out again
# Removes dead XAMPP entries (C:\xampp\...) and inserts the APM entries where the first of them was,
# so precedence vs other tools stays the same. Open apps (VS Code) must be restarted to see the change.
#
# Edits the registry value directly: [Environment]::SetEnvironmentVariable would read PATH already
# expanded and write it back as REG_SZ, destroying %SystemRoot%-style entries. Here the raw
# (unexpanded) text is kept and the value is written as REG_EXPAND_SZ.
param([ValidateSet('Machine','User')][string]$Scope = 'Machine', [string]$Target = 'C:\APM', [switch]$Remove)
$ErrorActionPreference = 'Stop'
if ($Scope -eq 'Machine' -and -not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) { throw '[ERR] Machine scope needs Administrator (or use -Scope User)' }

$key = if ($Scope -eq 'Machine') {
    [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey('SYSTEM\CurrentControlSet\Control\Session Manager\Environment', $true)
} else { [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Environment', $true) }
try {
    $raw   = [string]$key.GetValue('Path', '', 'DoNotExpandEnvironmentNames')
    $apm   = @("$Target\php", "$Target\mariadb\bin")
    $parts = @($raw -split ';' | Where-Object { $_ })
    $isOld = { param($p) $p.TrimEnd('\') -match '^(?i)C:\\xampp\\' -or $apm -contains $p.TrimEnd('\') }
    $at    = [array]::FindIndex([string[]]$parts, [Predicate[string]] { param($p) & $isOld $p })
    $kept  = @($parts | Where-Object { -not (& $isOld $_) })
    $removed = @($parts | Where-Object { & $isOld $_ })
    if (-not $Remove) {
        if ($at -lt 0) { $at = $kept.Count }
        $kept = @($kept[0..($at - 1)] | Where-Object { $_ }) + $apm + @(if ($at -lt $kept.Count) { $kept[$at..($kept.Count - 1)] })
    }
    $key.SetValue('Path', ($kept -join ';'), [Microsoft.Win32.RegistryValueKind]::ExpandString)
} finally { $key.Close() }

# tell running programs (Explorer, new terminals) that the environment changed
Add-Type -Namespace ApmNative -Name Env -MemberDefinition @'
[DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
public static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint Msg, UIntPtr wParam, string lParam, uint fuFlags, uint uTimeout, out UIntPtr lpdwResult);
'@
$r = [UIntPtr]::Zero
[void][ApmNative.Env]::SendMessageTimeout([IntPtr]0xffff, 0x1A, [UIntPtr]::Zero, 'Environment', 2, 5000, [ref]$r)

Write-Host "[OK] $Scope PATH updated (REG_EXPAND_SZ)"
$removed | ForEach-Object { Write-Host "     - $_" }
if (-not $Remove) { $apm | ForEach-Object { Write-Host "     + $_" } }
Write-Host '     restart VS Code / terminals to pick it up'
