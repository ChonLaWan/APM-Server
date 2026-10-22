# 25-imagemagick.ps1 - build ImageMagick 7 (+ all delegate libs) from source with the official
# ImageMagick/Windows toolchain, then lay out headers/libs for the PHP imagick extension.
# Version comes from versions.json -> sources.imagemagick.version (an ImageMagick git tag).
param([switch]$SkipClone)
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot\lib.ps1"
$Tag  = (Get-Content "$Root\versions.json" -Raw | ConvertFrom-Json).sources.imagemagick.version
$W    = "$Root\src\imagemagick-windows"
$Dev  = "$Root\build\im-dev"
$Log  = "$Root\build\logs\imagemagick.log"
New-Item -ItemType Directory -Force "$Root\build\logs" | Out-Null

# 1. sources: ImageMagick at $Tag + the Configure/Dependencies versions that tag pins
if (-not $SkipClone) {
    if (-not (Test-Path $W)) { git clone --quiet https://github.com/ImageMagick/Windows.git $W }
    Push-Location $W
    & "$env:ProgramFiles\Git\bin\bash.exe" -c "./clone-repositories.sh --imagemagick7 --commit $Tag" *> "$Root\build\logs\im-clone.log"
    if ($LASTEXITCODE) { Pop-Location; throw "[ERR] clone failed (build\logs\im-clone.log)" }
    Pop-Location
}

Enter-VsEnv
$env:Path = "$Root\toolchain\nasm;$env:Path"

# 2. Configure.exe (project targets v145/VS2026; this box has v143/VS2022)
Push-Location "$W\Configure"
# Windows Configure hardcodes a 64-bit channel mask on x64, which makes MagickCore headers C++-only
# (#error in magick-baseconfig.h). imagick is C -> use 32 like the Linux default.
$opt = 'src\Options.cpp'
$src = Get-Content $opt -Raw
$patched = $src -replace '(?s)(Options::channelMaskDepth\(\) const.*?if \(architecture != Architecture::x86\)\s*return\(L")64("\);)', '${1}32$2'
if ($patched -ne $src) { Set-Content $opt $patched -NoNewline -Encoding utf8; Write-Host '[OK] Configure patched: channel mask 32' }
elseif ($src -notmatch 'channelMaskDepth') { throw '[ERR] Options.cpp layout changed - re-check the channel mask patch' }
Invoke-Step 'IM Configure.exe build' { msbuild Configure.sln /m /t:Rebuild /p:Configuration=Release,Platform=x64 /p:PlatformToolset=v143 /v:minimal /nologo } $Log
Copy-Item Artifacts\Release\x64\bin\Configure.Release.x64.exe . -Force
# 3. generate IM7.Dynamic.x64.sln: VS2022 toolset, x64, DLL build, Q16 + HDRI (IM7 default)
Invoke-Step 'IM configure' { .\Configure.Release.x64.exe /noWizard /VS2022 /x64 /dynamic /Q16 /hdri } $Log
Pop-Location

# 4. build everything (ImageMagick + delegates: jpeg, png, webp, heif, tiff, freetype, xml, zlib ...)
Push-Location $W
Invoke-Step 'IM build' { msbuild IM7.Dynamic.x64.sln /m /t:Build /p:Configuration=Release,Platform=x64 /v:minimal /nologo } $Log
Pop-Location

# 5. dev layout for imagick's config.w32: <dir>\MagickCore\*.h, <dir>\MagickWand\*.h, <dir>\CORE_RL_*.lib
if (Test-Path $Dev) { Remove-Item $Dev -Recurse -Force }
New-Item -ItemType Directory -Force "$Dev\MagickCore", "$Dev\MagickWand" | Out-Null
Copy-Item "$W\ImageMagick\MagickCore\*.h" "$Dev\MagickCore"
Copy-Item "$W\ImageMagick\MagickWand\*.h" "$Dev\MagickWand"
Copy-Item "$W\Artifacts\lib\CORE_RL_Magick*_.lib" $Dev
Write-Host "[OK] ImageMagick $Tag -> $W\Artifacts\bin ; dev layout -> $Dev"
