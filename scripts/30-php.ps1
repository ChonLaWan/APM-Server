# 30-php.ps1 - build PHP 8.4 NTS x64 (vs17) with php-sdk + PHP-team deps series
# Usage: pwsh scripts\30-php.ps1 [-Step deps|help|build|all]
param([ValidateSet('deps','help','build','all')][string]$Step = 'all')
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
$V    = Get-Content "$Root\versions.json" -Raw | ConvertFrom-Json
$PhpV = $V.sources.php.version

$Sdk   = "$Root\toolchain\php-sdk"
$Src   = "$Root\src\php-$PhpV"
$Deps  = "$Root\build\php-deps"
$Pecl  = "$Root\build\php-pecl"
$Obj   = "$Root\build\php-obj"
$Stage = "$Root\stage\php"
New-Item -ItemType Directory -Force $Deps, $Pecl, $Obj, "$Root\build\logs" | Out-Null

# PECL sources -> build\php-pecl\<extname> (buildconf --add-modules-dir wants ext-named folders).
# Refresh on every run: the folder name carries no version, so a stale copy would be reused silently.
foreach ($e in 'redis','igbinary','imagick') {
    $from = "$Root\src\pecl-$e-$($V.sources."pecl-$e".version)"
    if (-not (Test-Path $from)) { throw "[ERR] $from missing - run 00-fetch.ps1" }
    if (Test-Path "$Pecl\$e") { Remove-Item "$Pecl\$e" -Recurse -Force }
    Copy-Item $from "$Pecl\$e" -Recurse
}

$Configure = @(
  '--disable-all','--disable-zts','--enable-cli','--enable-cgi',
  '--enable-opcache','--enable-opcache-jit',
  '--with-openssl','--with-curl','--enable-zlib','--with-libxml','--with-dom','--with-xml',
  '--enable-xmlreader','--enable-xmlwriter','--with-simplexml','--enable-tokenizer',
  '--enable-session','--enable-filter','--enable-ctype','--enable-phar','--with-iconv',
  '--enable-mbstring=shared','--enable-mbregex','--enable-intl=shared','--with-gd=shared',
  '--with-libwebp','--with-libavif','--with-pcre-jit','--enable-redis-session',  # default-on, but --disable-all turns them off
  '--with-gettext=shared','--with-ffi=shared','--enable-fileinfo=shared','--enable-exif=shared',
  '--enable-bcmath','--enable-calendar','--enable-ftp=shared','--enable-shmop=shared',
  '--enable-sockets=shared','--enable-soap=shared','--with-sodium=shared','--with-xsl=shared',
  '--enable-zip=shared','--with-mysqlnd','--with-mysqli=shared','--enable-pdo',
  '--with-pdo-mysql=shared','--with-pdo-sqlite=shared','--with-sqlite3=shared',
  '--enable-igbinary=shared','--enable-redis=shared','--enable-redis-igbinary',
  $(if (Test-Path "$Root\build\im-dev") { "--with-imagick=shared,$Root\build\im-dev" }),  # from 25-imagemagick.ps1
  "--with-extra-includes=$Pecl",  # redis includes "igbinary/igbinary.h"
  "--with-php-build=$Deps","--enable-object-out-dir=$Obj","--with-prefix=$Stage"
) -join ' '

$task = switch ($Step) {
  'deps'  { "call phpsdk_deps -u -b $($V.php_deps_series) -d `"$Deps`" || exit /b 1" }
  'help'  { "call .\buildconf.bat --force --add-modules-dir=`"$Pecl`" || exit /b 1`r`ncall .\configure.bat --help" }
  'build' { @"
call .\buildconf.bat --force --add-modules-dir="$Pecl" || exit /b 1
call .\configure.bat $Configure || exit /b 1
nmake || exit /b 1
nmake install || exit /b 1
"@ }
  'all'   { @"
call phpsdk_deps -u -b $($V.php_deps_series) -d "$Deps" || exit /b 1
call .\buildconf.bat --force --add-modules-dir="$Pecl" || exit /b 1
call .\configure.bat $Configure || exit /b 1
nmake || exit /b 1
nmake install || exit /b 1
"@ }
}

# configure option changes do not trigger recompiles of existing objects -> always build clean
if ($Step -in 'build','all') {
    foreach ($d in $Obj, $Stage) { if (Test-Path $d) { Remove-Item $d -Recurse -Force } }
    New-Item -ItemType Directory -Force $Obj | Out-Null
}

# Keep XAMPP's php off PATH so configure/tools never pick it up
$env:Path = ($env:Path -split ';' | Where-Object { $_ -notmatch '(?i)xampp' }) -join ';'
$bat = "$Root\build\php-task-$Step.bat"
"@echo off`r`ncd /d `"$Src`"`r`n$task`r`nexit /b 0" | Set-Content $bat -Encoding ascii
$log = "$Root\build\logs\php-$Step.log"
& cmd /c "`"$Sdk\phpsdk-starter.bat`" -c vs17 -a x64 -t `"$bat`" > `"$log`" 2>&1"
$rc = $LASTEXITCODE
Get-Content $log -Tail 40
if ($rc) { throw "[ERR] php step '$Step' failed rc=$rc (log: $log)" }
Write-Host "[OK] php step '$Step' (log: $log)"
if ($Step -notin 'build','all') { return }

# Copy only the dependency DLLs that php/ext actually import (transitively), from deps\bin
. "$PSScriptRoot\lib.ps1"; Enter-VsEnv
$avail = @{}; Get-ChildItem "$Deps\bin\*.dll" | ForEach-Object { $avail[$_.Name.ToLower()] = $_.FullName }
$queue = [System.Collections.Generic.Queue[string]]::new()
Get-ChildItem "$Stage\*.exe", "$Stage\php8.dll", "$Stage\ext\*.dll" | ForEach-Object { $queue.Enqueue($_.FullName) }
$need = @{}
while ($queue.Count) {
    dumpbin /nologo /dependents $queue.Dequeue() | ForEach-Object {
        $n = $_.Trim().ToLower()
        if ($avail.ContainsKey($n) -and -not $need.ContainsKey($n)) { $need[$n] = $avail[$n]; $queue.Enqueue($avail[$n]) }
    }
}
$need.Values | Copy-Item -Destination $Stage -Force
Write-Host "[OK] runtime DLLs: $(($need.Keys | Sort-Object) -join ' ')"

# ImageMagick: CORE_RL_* plus the coder modules (IM_MOD_*) and delegates it loads at runtime, plus its
# config xml files - all next to php.exe/php-cgi.exe, same as the official imagick-on-Windows layout.
$ImBin = "$Root\src\imagemagick-windows\Artifacts\bin"
if (Test-Path "$Stage\ext\php_imagick.dll") {
    Copy-Item "$ImBin\*.dll", "$ImBin\*.xml" $Stage -Force
    Write-Host "[OK] ImageMagick runtime -> $Stage"
}
& "$Stage\php.exe" -v


