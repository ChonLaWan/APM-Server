# How to build APM from source on Windows

[ภาษาไทย](HOWTO-BUILD.md) · [README](README.md#en) · Thai article series on LinuxThai: [Build APM on Windows](https://www.linuxthai.org/article/apm-build-windows-index)

This guide rebuilds the whole APM stack from upstream source on a Windows x64 PC. It covers Apache httpd 2.4, PHP 8.4 NTS with redis/igbinary/imagick, MariaDB 11.8, ImageMagick 7, every third-party library, and the APM Manager (Qt 6). The result is a portable zip. Each step shows the exact command and the output it really produced on the build machine (2026-10-22, Windows 11 x64, i5-12500H with 16 threads, 32 GB RAM).

```
build.cmd       :: everything -> dist\APM-<ver>-win64.zip
deploy.cmd      :: install stage\ into C:\APM (optional, build machine only)
```

---

## 1. Requirements

### 1.1 Hardware

| | Minimum | Used for this guide |
|---|---|---|
| OS | Windows 10/11 x64 | Windows 11 x64 |
| CPU | 4 cores | 16 threads |
| RAM | 8 GB (16 GB+ recommended) | 32 GB |
| Free disk | 60 GB | about 45 GB used |
| Network | about 4 GB of downloads | |
| Admin | only to install the tools in section 2 | |

### 1.2 Software you install (section 2)

| Tool | Tested with | Why |
|---|---|---|
| Visual Studio 2022 Build Tools | 17.14, MSVC 14.44 (toolset v143 = "vs17") | compiler; same ABI as official PHP builds |
| Windows SDK | 10.0.26100 | |
| Git for Windows | 2.54 | clone; its `bash.exe` runs ImageMagick's clone script |
| CMake | 3.24+ (tested 4.1.2) | APR/httpd/MariaDB/manager; the scripts use `--fresh` |
| Ninja | 1.13 | fast CMake backend |
| PowerShell 7 | 7.5+ | runs every build script |
| Qt 6 | 6.8+ (tested 6.10.0 msvc2022_64) | APM Manager only |

### 1.3 Downloaded by the scripts (portable, sha256-checked, nothing added to PATH)

| Tool | Version | Why |
|---|---|---|
| php-sdk-binary-tools | 2.8.4 | PHP's Windows build system (bison, re2c, ...) |
| Strawberry Perl (portable) | 5.42.3.1 | OpenSSL/MariaDB build scripts. **Never use Git's perl** (it is Cygwin) |
| NASM | 3.01 | OpenSSL and ImageMagick delegate assembly |
| winflexbison | 2.5.25 | bison for MariaDB |
| vcpkg | pinned commit (`builtin-baseline` in `deps/apache/vcpkg.json`) | builds Apache's libraries from source |

---

## 2. Install the tools (once, as admin)

### 2.1 Visual Studio 2022 Build Tools

```
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --add Microsoft.VisualStudio.Component.VC.CMake.Project --add Microsoft.VisualStudio.Component.VC.ATL --add Microsoft.VisualStudio.Component.VC.ATLMFC --includeRecommended"
```

`VC.ATL` and `VC.ATLMFC` are required by ImageMagick, whose `Configure.exe` is an MFC program. Any VS 2022 edition works, because the scripts locate it with `vswhere`.

```
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -products * -property installationPath
```

### 2.2 Git, CMake, Ninja, PowerShell 7, Qt

```
winget install --id Git.Git --exact
winget install --id Kitware.CMake --exact
winget install --id Ninja-build.Ninja --exact
winget install --id Microsoft.PowerShell --exact
```

Install Qt 6 for MSVC 2022 64-bit with the Qt Online Installer. The build script defaults to `C:\Qt\6.10.0\msvc2022_64`. Set `QT_DIR` or pass `-QtDir` if yours lives elsewhere.

### 2.3 Recommended: exclude the build folders from Defender

Defender scanning tens of thousands of `.obj` files makes the build 2-3 times slower.

```
Add-MpPreference -ExclusionPath "<repo>\build","<repo>\src","<repo>\toolchain"
```

### 2.4 Turn on the leak guard (before your first commit)

```
git config core.hooksPath .githooks
```

From then on, every commit runs [scripts/check-leaks.ps1](scripts/check-leaks.ps1), which looks for:
- private keys,
- GitHub/AWS/Slack/Google/Telegram tokens,
- `password=`/`token=` values,
- personal `C:\Users\<name>\` paths.

Put your own private terms (internal hostnames, IPs, client names) one regex per line in `private/leak-terms.txt`. That folder is gitignored, so the list itself is never published. CI repeats the scan, and also runs gitleaks, on every push.

---

## 3. Repository layout

| Path | What it is |
|---|---|
| [build.cmd](build.cmd), [deploy.cmd](deploy.cmd) | entry points |
| [versions.json](versions.json) | every version + URL + sha256 (and `apm_version`) |
| [versions.lock.json](versions.lock.json) | trust-on-first-use sha256 for downloads without a published hash |
| [deps/apache/vcpkg.json](deps/apache/vcpkg.json) | Apache's third-party libs + vcpkg baseline |
| [ports-overlay/openssl/](ports-overlay/openssl/) | OpenSSL 3.5 LTS port (replaces vcpkg's 3.6) |
| [templates/](templates/) | httpd.conf, apm-ssl.conf, php.ini, my.ini, index.php |
| [tools/mkcert.php](tools/mkcert.php) | local CA + server certificates through PHP's openssl extension |
| [manager/](manager/) | APM Manager source (C++17/20, Qt 6 Widgets, Win32 API) |
| [scripts/lib.ps1](scripts/lib.ps1) | shared helpers: find VS with vswhere, load vcvars64, step logging |
| [scripts/00-fetch.ps1](scripts/00-fetch.ps1) | download + verify + extract |
| [scripts/10-deps-vcpkg.ps1](scripts/10-deps-vcpkg.ps1) | Apache's libs (OpenSSL, PCRE2, nghttp2, brotli, curl, ...) |
| [scripts/20-apache.ps1](scripts/20-apache.ps1) | APR, APR-util, httpd, mod_fcgid |
| [scripts/40-mariadb.ps1](scripts/40-mariadb.ps1) | MariaDB |
| [scripts/25-imagemagick.ps1](scripts/25-imagemagick.ps1) | ImageMagick 7 + about 40 delegate libs |
| [scripts/30-php.ps1](scripts/30-php.ps1) | PHP 8.4 + PECL redis/igbinary/imagick |
| [scripts/50-manager.ps1](scripts/50-manager.ps1) | APM Manager + windeployqt |
| [scripts/95-package.ps1](scripts/95-package.ps1) | portable zip with VC++ runtime, CA bundle, licenses |
| [scripts/build-all.ps1](scripts/build-all.ps1) | all of the above in order (`-From`, `-To`) |
| [scripts/90-deploy.ps1](scripts/90-deploy.ps1) | install into C:\APM (build machine) |
| [scripts/firewall-lan.ps1](scripts/firewall-lan.ps1) | firewall rule for LAN access (admin) |
| [scripts/trust-ca.ps1](scripts/trust-ca.ps1) | trust the APM CA for the current user |
| [scripts/new-cert.ps1](scripts/new-cert.ps1) | extra certificates for vhost names |
| [scripts/set-path.ps1](scripts/set-path.ps1) | APM php/mysql on PATH (REG_EXPAND_SZ-safe) |
| [scripts/check-leaks.ps1](scripts/check-leaks.ps1) | secret/private-data scanner (pre-commit + CI) |
| [.github/workflows/](.github/workflows/) | `release.yml` (build + smoke test + release), `security.yml` (leak scan) |

Created by the scripts (gitignored): `downloads\`, `toolchain\`, `src\`, `build\` (with `logs\`), `stage\` (built engines), `dist\` (the zip).

---

## 4. Build everything with one command

```
build.cmd
```

| Step | Script | Cold build | Re-run with caches |
|---|---|---|---|
| fetch | 00-fetch.ps1 | 3-5 min | 0.0 min |
| vcpkg | 10-deps-vcpkg.ps1 | about 45 min (OpenSSL, libiconv) | 0.1 min |
| apache | 20-apache.ps1 | 4 min | 1.6 min |
| mariadb | 40-mariadb.ps1 | 30 min | 0.3 min |
| imagemagick | 25-imagemagick.ps1 | 40-60 min | 3.3 min |
| php | 30-php.ps1 -Step all | 12 min | 5.0 min |
| manager | 50-manager.ps1 | 2 min | 1 min |
| package | 95-package.ps1 | 2 min | 2 min |

Resume after a failure, or stop early:

```
build.cmd -From php
build.cmd -From apache -To apache
```

Logs are in `build\logs\` (`apache.log`, `php-build.log`, `mariadb.log`, `imagemagick.log`, `vcpkg-apache.log`, `manager.log`).

---

## 5. Step 1 - fetch sources and tools

```
pwsh -File scripts\00-fetch.ps1
pwsh -File scripts\00-fetch.ps1 -Only php,httpd -Force
```

The script reads [versions.json](versions.json), downloads every entry that has a `url`, and checks its sha256. Upstream-published hashes are used for PHP, httpd, APR and MariaDB. For entries with no published hash (GitHub releases, PECL) it uses trust-on-first-use: the hash is recorded in `versions.lock.json` and verified on every later download. It then extracts the tools to `toolchain\<name>` and the sources to `src\<name>-<version>`.

```
[GET] php 8.4.27
[OK] php -> ...\src\php-8.4.27
[GET] httpd 2.4.69
[OK] httpd -> ...\src\httpd-2.4.69
...
```

---

## 6. Step 2 - Apache's libraries (vcpkg)

```
pwsh -File scripts\10-deps-vcpkg.ps1
```

1. If needed, clone a private vcpkg into `toolchain\vcpkg`, check out the `builtin-baseline` commit, and bootstrap it. A shared `C:\vcpkg` is never touched.
2. Run `vcpkg install --triplet x64-windows --x-manifest-root=deps\apache --x-install-root=build\vcpkg_installed --overlay-ports=ports-overlay`.
3. The result is `openssl` (3.5.9 LTS from the overlay), `pcre2`, `zlib`, `nghttp2`, `brotli`, `expat`, `libxml2`, `jansson` and `curl`, all built with MSVC.

Why the overlay: the vcpkg baseline ships OpenSSL 3.6.5, a non-LTS line that reaches end of life in Nov 2026. 3.5 is LTS until Apr 2030, and all 10 vcpkg patches apply to 3.5.9 unchanged.

---

## 7. Step 3 - Apache httpd 2.4.69 + mod_fcgid

```
pwsh -File scripts\20-apache.ps1
```

| Part | Key options |
|---|---|
| APR 1.7.6 | `-DAPR_INSTALL_PRIVATE_H=ON` |
| APR-util 1.6.5 | `-DAPU_HAVE_CRYPTO=ON -DAPU_HAVE_ODBC=OFF -DAPR_HAS_LDAP=OFF` |
| httpd 2.4.69 | `-DENABLE_MODULES=i` + PCRE2/nghttp2/brotli/curl/jansson from vcpkg |
| mod_fcgid 2.3.9 | `modules\fcgid\CMakeLists.txt` |

Every part uses:
- `-G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo`
- `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`: needed because CMake 4 rejects APR's old minimum.
- `-DCMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO=Release;RelWithDebInfo;`: without it, mod_md links the debug curl.
- `cmake --fresh`: so stale cache values never survive.

After the build, the script:
- copies `mime.types`, `magic`, `error\` and `icons\` from the source, because `cmake --install` does not,
- copies only the DLLs that the binaries actually import (found transitively with `dumpbin /dependents`),
- fails if a release binary imports a debug DLL.

```
stage\apache\bin\httpd.exe -V | Select-String "version|APR|MPM"
Server version: Apache/2.4.69 (Win64)
Server loaded:  APR 1.7.6, APR-UTIL 1.6.5, PCRE 10.49 2026-09-28
Server MPM:     WinNT
```

120 modules are built. The ones not built are ldap/authnz_ldap/charset_lite (no LDAP/XLATE in APR-util), socache_dc, and lua (httpd only looks for Lua 5.1).

---

## 8. Step 4 - MariaDB 11.8 LTS

```
pwsh -File scripts\40-mariadb.ps1
```

```
cmake -S src\mariadb-11.8.9 -B build\mariadb -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
  -DCMAKE_INSTALL_PREFIX=stage/mariadb -DWITH_SSL=bundled -DWITH_UNIT_TESTS=OFF
  -DBISON_EXECUTABLE=toolchain\winflexbison\win_bison.exe
  -DPLUGIN_ROCKSDB=NO -DPLUGIN_COLUMNSTORE=NO -DPLUGIN_CONNECT=NO -DPLUGIN_MROONGA=NO
  -DPLUGIN_SPIDER=NO -DPLUGIN_OQGRAPH=NO -DPLUGIN_SPHINX=NO -DPLUGIN_DUCKDB=NO -DPLUGIN_S3=NO
  -DPLUGIN_AWS_KEY_MANAGEMENT=NO -DPLUGIN_HASHICORP_KEY_MANAGEMENT=NO
```

`WITH_SSL=bundled` uses wolfSSL, the MariaDB default on Windows. Skipping the heavy engines saves most of the build time.

Quick test on port 3399:

```
version()       @@character_set_server  @@have_ssl
11.8.9-MariaDB  utf8mb4                 YES
```

---

## 9. Step 5 - ImageMagick 7 (for imagick)

```
pwsh -File scripts\25-imagemagick.ps1
```

This step uses ImageMagick's own Windows toolchain (github.com/ImageMagick/Windows), which builds ImageMagick plus about 40 delegate libraries from source: jpeg-turbo, png, webp, heif, jpeg-xl, tiff, rsvg, freetype, lcms and others.

1. Clone `ImageMagick/Windows` and run `clone-repositories.sh --imagemagick7 --commit <tag>` through Git's bash. The tag comes from `versions.json` (`sources.imagemagick.version`); Configure and Dependencies are pinned by that tag.
2. **Patch `Configure\src\Options.cpp` to use a 32-bit channel mask.** Configure forces 64 bit on x64, which turns MagickCore's headers into C++-only (`#error ... requires a C++ compiler`), and imagick is C. 32 is the Linux default.
3. Build `Configure.sln` with `/p:PlatformToolset=v143` (the project targets v145 of VS2026).
4. `Configure.Release.x64.exe /noWizard /VS2022 /x64 /dynamic /Q16 /hdri`
5. `msbuild IM7.Dynamic.x64.sln /m /t:Build /p:Configuration=Release,Platform=x64`
6. Lay out `build\im-dev` (`MagickCore\`, `MagickWand\`, `CORE_RL_*.lib`) for imagick.

```
Version: ImageMagick 7.1.2-34 Q16-HDRI x64
Delegates (built-in): bzlib cairo freetype gslib heic jng jp2 jpeg jxl lcms lqr lzma openexr pangocairo png ps raqm raw rsvg tiff webp xml zip zlib zstd
#define MAGICKCORE_CHANNEL_MASK_DEPTH 32
```

HEIC can be read but not written, because the x265 encoder is not bundled for licensing reasons. webp/avif/png/jpeg read and write fine.

---

## 10. Step 6 - PHP 8.4 NTS + PECL

```
pwsh -File scripts\30-php.ps1 -Step all      # or: deps | help | build
```

The build runs inside the php-sdk environment (`phpsdk-starter.bat -c vs17 -a x64 -t <task.bat>`). The `deps` step fetches the PHP team's 8.4 vs17 dependency series. `build` always starts from a clean object directory.

```
configure --disable-all --disable-zts --enable-cli --enable-cgi
  --enable-opcache --enable-opcache-jit
  --with-openssl --with-curl --enable-zlib --with-libxml --with-dom --with-xml
  --enable-xmlreader --enable-xmlwriter --with-simplexml --enable-tokenizer
  --enable-session --enable-filter --enable-ctype --enable-phar --with-iconv
  --enable-mbstring=shared --enable-mbregex --enable-intl=shared --with-gd=shared
  --with-libwebp --with-libavif --with-pcre-jit --enable-redis-session
  --with-gettext=shared --with-ffi=shared --enable-fileinfo=shared --enable-exif=shared
  --enable-bcmath --enable-calendar --enable-ftp=shared --enable-shmop=shared
  --enable-sockets=shared --enable-soap=shared --with-sodium=shared --with-xsl=shared
  --enable-zip=shared --with-mysqlnd --with-mysqli=shared --enable-pdo
  --with-pdo-mysql=shared --with-pdo-sqlite=shared --with-sqlite3=shared
  --enable-igbinary=shared --enable-redis=shared --enable-redis-igbinary
  --with-imagick=shared,build\im-dev --with-extra-includes=build\php-pecl
  --with-php-build=build\php-deps --enable-object-out-dir=build\php-obj --with-prefix=stage\php
```

After `nmake install`, the script copies the dependency DLLs that PHP really imports, plus ImageMagick's DLLs and its XML config files, next to `php.exe`.

```
PHP 8.4.27 (cli) (NTS Visual C++ 2022 x64)
{"jit":true,"webp":true,"imagick":"ImageMagick 7.1.2-34 Q16-HDRI x64 ..."}
webp=1696 png=2266 jpg=5862 avif=1456      (imagick writes, bytes)
```

You get 51 modules: the production set minus `posix sysvmsg sysvsem sysvshm`, which do not exist on Windows.

---

## 11. Step 7 - APM Manager

```
pwsh -File scripts\50-manager.ps1 [-QtDir C:\Qt\6.10.0\msvc2022_64]
```

This step builds `manager\` with CMake + Ninja (Release) and then runs `windeployqt` (no translations, no software OpenGL), giving `stage\manager` at about 51 MB.

The version is generated at configure time:
- `apm_version` from versions.json,
- `build` = `git rev-list --count HEAD`,
- the commit hash, `-dirty` if there are uncommitted changes,
- the date.

It appears in the About window, in the footer, and in the exe's VERSIONINFO (`ProductVersion 1.0.0.<build>`).

| Source file | Role |
|---|---|
| `src/Engine.*` | layout, status polling (processes, SCM, TCP listeners), start/stop/restart in process or service mode, version probing (re-probed when an exe changes) |
| `src/Win.*` | Win32: processes, named events, services + DACL, UAC launch, PATH as raw REG_EXPAND_SZ |
| `src/Setup.*`, `src/SetupDialog.*` | first run and relocation: render templates, certs, datadir, `apm.json` |
| `src/MainWindow.*` | cards, tray, minimise-to-tray |
| `src/SettingsDialog.*` | sign-in start, services, ports/LAN + firewall, PATH, CA trust |
| `src/UpdateDialog.*` | GitHub Releases update (download + sha256 + stop/replace/start), install from zip/folder, upstream versions |
| `src/Elevated.*` | admin operations, run as `APMManager.exe --elevated <op>` through UAC |
| `src/AboutDialog.*`, `src/Theme.*` | credits, version, light/dark tokens, Material icons |

How graceful stop works without a console:
- Apache's parent waits on the named event `ap<parent-pid>_shutdown` (and `_restart` for a graceful restart).
- MariaDB waits on `MySQLShutdown<pid>`. Both names are taken from the upstream sources.
- The Manager signals those events and kills the process only if it is still running after the timeout.

---

## 12. Step 8 - the portable zip

```
pwsh -File scripts\95-package.ps1
```

`dist\APM-1.0.0-win64\` is 403 MB, and `dist\APM-1.0.0-win64.zip` is 134 MB plus a `.sha256` file. The package contains:
- the engines without `.pdb/.lib/include` and without any rendered config,
- `manager\`, `templates\`, `tools\`, `versions.json`, README.txt,
- `ssl\cacert.pem` (Mozilla bundle from curl.se, sha256-checked),
- the **app-local VC++ runtime** (Microsoft.VC143.CRT) in `apache\bin`, `php`, `mariadb\bin` and `manager`, plus `vcomp140.dll` (OpenMP for ImageMagick) in `php`,
- `licenses\` with 133 files: httpd, APR, mod_fcgid, PHP, PECL, MariaDB, ImageMagick and its delegates, Qt LGPL/GPL, and the vcpkg libs.

Portable test from the real zip:

| Test | Result |
|---|---|
| unzip into `D:\APM test\portable A` (spaces), `--setup --http 8081 --https 8444 --db 3308` | config, CA + cert and datadir created, `httpd -t` OK |
| `--cli start`, open the page, https | PHP 8.4.27 + MariaDB 11.8.9 answer, https 200, php-cgi runs from that folder |
| move to `D:\APM test\moved B (copy)` (brackets), `--cli status` | refuses with exit 2 ("folder was moved, run --setup") |
| `--setup` | move detected, config re-rendered with the same ports, data kept, cert re-issued |
| start, page, stop | all OK |

---

## 13. Step 9 - install on the build machine, run, LAN, certificates, PATH

```
deploy.cmd                                            :: C:\APM, ports 8080/8443/3307
deploy.cmd -HttpPort 80 -HttpsPort 443 -DbPort 3306 -Lan -ResetConfig
```

`deploy.cmd` works like this:
- It refuses to run while APM servers or services are running.
- It mirrors the engines (keeping your `conf\`, `php.ini` and `my.ini`) and installs the manager, templates and tools.
- It renders config only when missing or on `-ResetConfig`. `-ResetConfig` also re-issues the server certificate and syncs `data\mysql\my.ini`.
- It creates the datadir with `mariadb-install-db` (bootstrap only, no listener), checks `httpd -t`, and never starts anything.

Run and test without services:

```
C:\APM\manager\APMManager.exe --cli start
curl.exe -s http://localhost/
C:\APM\manager\APMManager.exe --cli stop
```

Results measured on the build machine:
- PHP via fcgid (`cgi-fcgi`) with OPcache JIT on.
- Thai text round-trips through PDO and mysqli, and new tables get `utf8mb4_unicode_ci`.
- Sessions work, imagick and GD write webp, and curl reaches external https.
- `.htaccess` rewrite works, and so do brotli and gzip.
- `.env` returns 403.
- HTTPS over HTTP/2 with TLS 1.2/1.3; TLS 1.1 is refused.
- `ab -n 3000 -c 30` gives 678 req/s with 0 failures.

LAN (Apache only): `deploy.cmd -Lan -ResetConfig`, then once as admin:

```
pwsh -File scripts\firewall-lan.ps1          :: TCP 80,443 from LocalSubnet -> httpd.exe, all profiles
```

Tested from another PC on the LAN:
- http 200 and https 200,
- `?info=1` returns the normal page (phpinfo is local only),
- `/server-status` returns 403,
- port 3306 is closed.

Certificates:
- The CA lasts 10 years.
- The server cert lasts 825 days, with SAN `localhost, 127.0.0.1, ::1`, plus the PC name and LAN IPs when LAN is on.

```
pwsh -File scripts\trust-ca.ps1                       :: trust the APM CA for this user (one Windows prompt)
pwsh -File scripts\new-cert.ps1 -Hosts myapp.localhost :: extra vhost cert; *.localhost needs no hosts file
pwsh -File scripts\set-path.ps1 [-Scope User]          :: php + mysql on PATH for VS Code / terminals
```

---

## 14. Updating and releasing

1. Edit `version`, `url` and `sha256` in [versions.json](versions.json). Take the hashes from these places:
   - PHP: `https://www.php.net/releases/index.php?json&version=8.4&max=1`
   - httpd/APR: the `.sha256` next to each tarball on downloads.apache.org
   - MariaDB: `https://downloads.mariadb.org/rest-api/mariadb/11.8/latest/`
2. `pwsh -File scripts\00-fetch.ps1 -Only php`, then rebuild from that step: `build.cmd -From php`.
3. Bump `apm_version`, commit and push. Then publish in one of two ways:
   - **Local build (what v1.0.0 used):** `build.cmd`, test the zip, then create the release `v1.0.1` on GitHub and attach `dist\APM-1.0.1-win64.zip` and its `.sha256`.
   - **CI build:** Actions → release → Run workflow, with `release_tag` = `v1.0.1`. [release.yml](.github/workflows/release.yml) builds, smoke-tests and publishes. It is manual only, because a cold build takes 3-4 hours of Windows runner time.

   Users then press **Check for updates** in APM Manager. It reads `releases/latest` and picks the `APM-*-win64.zip` asset plus its `.sha256`.

Special cases:

| Update | Where |
|---|---|
| OpenSSL for Apache | `ports-overlay\openssl\vcpkg.json` (version) + `portfile.cmake` (SHA512 of the GitHub tarball), then `-From vcpkg` |
| other Apache libs | `git -C toolchain\vcpkg pull`, new commit as `builtin-baseline` in `deps\apache\vcpkg.json` |
| ImageMagick | `sources.imagemagick.version` = new tag, then `-From imagemagick` |
| PHP's dependency series | refreshed by `phpsdk_deps -u` on every `-Step all` |
| PHP 8.5 | only when production moves; set `sources.php` and `php_deps_series` = "8.5" |

---

## 15. Problems hit and how they were fixed

| Symptom | Cause | Fix (already in the scripts) |
|---|---|---|
| `'buildconf.bat' is not recognized` | `NoDefaultCurrentDirectoryInExePath=1` in the environment | call `.\buildconf.bat` / `.\configure.bat` |
| configure rejects `--enable-gd` | Windows flag names differ from Linux | `--with-gd`, `--with-dom`; check with `-Step help` |
| GD without WebP/AVIF, no PCRE JIT | `--disable-all` also disables default-on sub-options | `--with-libwebp --with-libavif --with-pcre-jit --enable-redis-session` |
| configure changes have no effect | nmake reuses old objects | wipe `build\php-obj` and `stage\php` every build |
| `igbinary/igbinary.h: No such file` | redis includes it with a directory prefix | `--with-extra-includes=build\php-pecl` |
| `php.exe` exits with code 53 | dependency DLLs missing | transitive `dumpbin /dependents` copy |
| PECL version bumped, old extension still built | `build\php-pecl\<ext>` was copied only once | refreshed on every build |
| `00-fetch.ps1 -Only php,httpd` via `pwsh -File` downloads nothing | `-File` passes `php,httpd` as one string | split on commas |
| `Compatibility with CMake < 3.5 has been removed` | old CMake files in APR/httpd | `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` |
| `Unexpected enablement value "N" for mod_lua` | httpd accepts only O/i/I/a/A; stale cache | no ENABLE_LUA, `cmake --fresh` |
| mod_lua built against Lua 5.5 | FindLua51 picked vcpkg's Lua | lua removed from the manifest |
| `mod_md.so` needs `libcurl-d.dll` | vcpkg imported targets have no RelWithDebInfo | `CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO` + debug-import check |
| no `conf\mime.types` | httpd's `cmake --install` skips conf | copied from `docs\conf` |
| httpd exits right after start, `AH00099 could not create ...httpd.pid` | default PidFile is `<ServerRoot>/logs`, which deploy does not use | `PidFile "${APM}/logs/apache/httpd.pid"` in the template |
| `ImageMagick was build with a 64 channel bit mask and that requires a C++ compiler` | Configure forces 64 on x64 | patch Options.cpp to 32 (Linux default) |
| Configure.exe does not build (v145 missing) | project targets VS2026 | `/p:PlatformToolset=v143` + `/VS2022` |
| OpenSSL build breaks with Git's perl | Cygwin perl makes Unix paths | Strawberry Perl only |
| MariaDB plugins (`ha_archive`, `auth_ed25519`, ...) missing after deploy | `robocopy /XD lib` also dropped `mariadb\lib\plugin` | MariaDB keeps `lib`, `.lib` files dropped by `/XF` |
| DB port change via `-ResetConfig` has no effect | the server reads `data\mysql\my.ini` | synced on `-ResetConfig` / setup |
| `Wrapper D:/APM cannot be accessed` when unzipped into a path with spaces | mod_fcgid tokenises the wrapper command line | `FcgidWrapper "\"${APM}/php/php-cgi.exe\"" .php` |
| cert step fails in `...\moved B (copy)` (`Undefined constant OPENSSL_KEYTYPE_EC`) | `( )` are INI syntax in `php -d extension_dir=...` | quote the value; mkcert reports a missing openssl clearly |
| Machine PATH turned into REG_SZ, `%SystemRoot%` expanded for good | `[Environment]::SetEnvironmentVariable` expands and writes REG_SZ | raw registry edit as REG_EXPAND_SZ (`set-path.ps1`, Manager) |
| port numbers shown in Thai digits | Qt follows the Thai Windows locale | `QLocale::setDefault(English)` |
| `.gitignore` entry `src/` hid `manager/src` | the pattern matches any `src` folder | anchored to the root: `/src/` |
| `Start-Process ... --cli start -Wait` never returns (test harness) | PowerShell 7 `-Wait` waits for the whole process tree | test via `cmd /c "... > out.txt"` |
| every PHP page 500s (`End of script output before headers`); `php-cgi -v` says `Opcode handlers are unusable due to ASLR` | two APM folders running at once share one OPcache segment name (same build, same user) | per-folder `opcache.cache_id` + `opcache.file_cache` with `file_cache_fallback` |
| relocated folder starts with config pointing at the old path | config is rendered with absolute paths | `apm.json` records the root; Manager re-renders, `--cli` refuses until `--setup` |

---

## 16. Output

```
stage\apache\    httpd 2.4.69 + 120 modules
stage\php\       PHP 8.4.27 NTS + ext\ + dependency DLLs + ImageMagick
stage\mariadb\   MariaDB 11.8.9
stage\manager\   APMManager.exe + Qt runtime
dist\APM-<ver>-win64.zip (+ .sha256)
```

Credit: ChonLaWan, wanpean@gmail.com, [wanpean.com](https://wanpean.com). License: MIT + Commons Clause (selling needs a commercial license); see [LICENSE](LICENSE).
