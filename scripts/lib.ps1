# lib.ps1 - shared helpers (dot-source from build scripts)
# Any VS 2022 edition with the C++ x64 tools (Build Tools here, Enterprise on GitHub runners)
function Get-VsPath {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $p = & $vswhere -latest -products * -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $p) { throw '[ERR] Visual Studio 2022 with C++ x64 tools not found (see HOWTO-BUILD.md section 2.1)' }
    return $p
}
$Script:VcVars = "$(Get-VsPath)\VC\Auxiliary\Build\vcvars64.bat"

# Import the MSVC x64 developer environment into this PowerShell process (once).
function Enter-VsEnv {
    if ($env:VSCMD_ARG_TGT_ARCH -eq 'x64') { return }
    $vswhereDir = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
    cmd /c "set `"PATH=$vswhereDir;%PATH%`" && `"$Script:VcVars`" >nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2]) }
    }
    # never let XAMPP binaries leak into a build
    $env:Path = ($env:Path -split ';' | Where-Object { $_ -and $_ -notmatch '(?i)xampp' }) -join ';'
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw '[ERR] MSVC environment not loaded' }
}

function Invoke-Step([string]$Name, [scriptblock]$Block, [string]$Log) {
    Write-Host "[..] $Name"
    & $Block *>> $Log
    if ($LASTEXITCODE) { Get-Content $Log -Tail 30; throw "[ERR] $Name failed rc=$LASTEXITCODE (log: $Log)" }
    Write-Host "[OK] $Name"
}
