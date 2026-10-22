# 20-apache.ps1 - build APR, APR-util, httpd and mod_fcgid with CMake + Ninja (MSVC x64)
# Needs: 00-fetch.ps1 (sources) + 10-deps-vcpkg.ps1 (third-party libs)
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
. "$PSScriptRoot\lib.ps1"
$V   = (Get-Content "$Root\versions.json" -Raw | ConvertFrom-Json).sources
$Dep = "$Root\build\vcpkg_installed\x64-windows" -replace '\\','/'
$Pfx = "$Root\stage\apache" -replace '\\','/'
$B   = "$Root\build"
$Log = "$B\logs\apache.log"
New-Item -ItemType Directory -Force "$B\logs" | Out-Null
Remove-Item $Log -ErrorAction SilentlyContinue
Enter-VsEnv

# CMAKE_POLICY_VERSION_MINIMUM: APR/httpd/fcgid declare cmake_minimum_required < 3.5, which CMake 4.x rejects
# CMAKE_MAP_IMPORTED_CONFIG_*: vcpkg config packages (CURL::libcurl) have no RelWithDebInfo -> would fall back to the debug DLL
$common = @('-G','Ninja','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
            '-DCMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO=Release;RelWithDebInfo;',"-DCMAKE_INSTALL_PREFIX=$Pfx","-DCMAKE_PREFIX_PATH=$Dep")

function Build-Cmake($name, $src, [string[]]$opts) {
    $bdir = "$B\$name"
    Invoke-Step "$name configure" { cmake --fresh -S $src -B $bdir @common @opts } $Log
    Invoke-Step "$name build"     { cmake --build $bdir } $Log
    Invoke-Step "$name install"   { cmake --install $bdir } $Log
}

Build-Cmake 'apr' "$Root\src\apr-$($V.apr.version)" @('-DAPR_INSTALL_PRIVATE_H=ON','-DAPR_BUILD_TESTAPR=OFF')

Build-Cmake 'apr-util' "$Root\src\apr-util-$($V.'apr-util'.version)" @(
    '-DAPU_HAVE_CRYPTO=ON','-DAPU_HAVE_ODBC=OFF','-DAPR_HAS_LDAP=OFF','-DAPR_BUILD_TESTAPR=OFF')

Build-Cmake 'httpd' "$Root\src\httpd-$($V.httpd.version)" @(
    '-DENABLE_MODULES=i','-DINSTALL_MANUAL=OFF',
    "-DPCRE_INCLUDE_DIR=$Dep/include","-DPCRE_LIBRARIES=$Dep/lib/pcre2-8.lib",'-DPCRE_CFLAGS=-DHAVE_PCRE2',
    "-DNGHTTP2_INCLUDE_DIR=$Dep/include","-DNGHTTP2_LIBRARIES=$Dep/lib/nghttp2.lib",
    "-DBROTLI_INCLUDE_DIR=$Dep/include","-DBROTLI_LIBRARIES=$Dep/lib/brotlienc.lib;$Dep/lib/brotlicommon.lib",
    "-DCURL_INCLUDE_DIR=$Dep/include","-DCURL_LIBRARIES=$Dep/lib/libcurl.lib",
    "-DJANSSON_INCLUDE_DIR=$Dep/include","-DJANSSON_LIBRARIES=$Dep/lib/jansson.lib")
    # mod_lua is skipped automatically: httpd only looks for Lua 5.1 (not needed by the target production setup)

# cmake --install of httpd does not lay down conf/error/icons -> copy the runtime data files from the source tree
$hs = "$Root\src\httpd-$($V.httpd.version)\docs"
New-Item -ItemType Directory -Force "$Pfx\conf\extra", "$Pfx\conf\vhosts" | Out-Null
Copy-Item "$hs\conf\mime.types", "$hs\conf\magic", "$hs\conf\charset.conv" "$Pfx\conf" -Force
Copy-Item "$hs\error", "$hs\icons" $Pfx -Recurse -Force

Build-Cmake 'mod_fcgid' "$Root\src\mod_fcgid-$($V.mod_fcgid.version)\modules\fcgid" @()

# runtime DLLs of the third-party libs next to httpd.exe - only those actually imported (transitively)
$avail = @{}; Get-ChildItem "$Dep\bin\*.dll" | ForEach-Object { $avail[$_.Name.ToLower()] = $_.FullName }
$debugOnly = @{}; Get-ChildItem "$Dep\debug\bin\*.dll" -ErrorAction SilentlyContinue |
    Where-Object { -not $avail.ContainsKey($_.Name.ToLower()) } | ForEach-Object { $debugOnly[$_.Name.ToLower()] = 1 }
$queue = [System.Collections.Generic.Queue[string]]::new()
Get-ChildItem "$Pfx\bin\*.exe", "$Pfx\bin\*.dll", "$Pfx\modules\*.so" | ForEach-Object { $queue.Enqueue($_.FullName) }
$need = @{}
while ($queue.Count) {
    dumpbin /nologo /dependents $queue.Dequeue() | ForEach-Object {
        $n = $_.Trim().ToLower()
        if ($debugOnly.ContainsKey($n)) { throw "[ERR] release binary imports a debug DLL: $n" }
        if ($avail.ContainsKey($n) -and -not $need.ContainsKey($n)) { $need[$n] = $avail[$n]; $queue.Enqueue($avail[$n]) }
    }
}
$need.Values | Copy-Item -Destination "$Pfx\bin" -Force
# OpenSSL loads its providers at runtime (not an import) - keep the legacy provider for old PKCS#12 files
if (Test-Path "$Dep\bin\legacy.dll") { Copy-Item "$Dep\bin\legacy.dll" "$Pfx\bin" -Force }
Write-Host "[OK] runtime DLLs: $(($need.Keys | Sort-Object) -join ' ')"
& "$Pfx\bin\httpd.exe" -V *>> $Log
Write-Host "[OK] apache -> $Pfx"
& "$Pfx\bin\httpd.exe" -v
