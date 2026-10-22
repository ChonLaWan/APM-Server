# 40-mariadb.ps1 - build MariaDB server + client (CMake + Ninja, MSVC x64, bundled wolfSSL)
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot\lib.ps1"
$Ver = (Get-Content "$Root\versions.json" -Raw | ConvertFrom-Json).sources.mariadb.version
$Src = "$Root\src\mariadb-$Ver"
$Bld = "$Root\build\mariadb"
$Pfx = "$Root\stage\mariadb" -replace '\\','/'
$Log = "$Root\build\logs\mariadb.log"
New-Item -ItemType Directory -Force "$Root\build\logs" | Out-Null
Remove-Item $Log -ErrorAction SilentlyContinue
Enter-VsEnv
# Strawberry perl (native) for build-time scripts; never Git's cygwin perl
$env:Path = "$Root\toolchain\strawberry\perl\bin;$env:Path"

$opts = @(
  '-G','Ninja','-DCMAKE_BUILD_TYPE=RelWithDebInfo',"-DCMAKE_INSTALL_PREFIX=$Pfx",
  '-DWITH_SSL=bundled','-DWITH_UNIT_TESTS=OFF','-DINSTALL_MYSQLTESTDIR=',
  "-DBISON_EXECUTABLE=$Root\toolchain\winflexbison\win_bison.exe",
  # engines/plugins a PHP dev box never touches - skipping them saves most of the build time
  '-DPLUGIN_ROCKSDB=NO','-DPLUGIN_COLUMNSTORE=NO','-DPLUGIN_CONNECT=NO','-DPLUGIN_MROONGA=NO',
  '-DPLUGIN_SPIDER=NO','-DPLUGIN_OQGRAPH=NO','-DPLUGIN_SPHINX=NO','-DPLUGIN_DUCKDB=NO',
  '-DPLUGIN_S3=NO','-DPLUGIN_AWS_KEY_MANAGEMENT=NO','-DPLUGIN_HASHICORP_KEY_MANAGEMENT=NO',
  '-DPLUGIN_EXAMPLE=NO','-DPLUGIN_TEST_SQL_DISCOVERY=NO'
)
Invoke-Step 'mariadb configure' { cmake -S $Src -B $Bld @opts } $Log
Invoke-Step 'mariadb build'     { cmake --build $Bld } $Log
Invoke-Step 'mariadb install'   { cmake --install $Bld } $Log
& "$Pfx\bin\mariadbd.exe" --version
