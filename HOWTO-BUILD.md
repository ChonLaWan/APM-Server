# คู่มือ build APM เองจาก source บน Windows

[English](HOWTO-BUILD.en.md) · [README](README.th.md)

พอดีต้อง build ทั้งชุดใหม่ตั้งแต่ศูนย์อยู่หลายรอบ ผมเลยจดทุกขั้นไว้ตรงนี้ เผื่อใครอยากคอมไพล์เองบ้าง หรือเอาไปทำต่อ ทุกคำสั่งในหน้านี้ผมรันจริงบนเครื่องตัวเองแล้ว ผลลัพธ์ที่แปะไว้ก็คัดมาจากหน้าจอจริง เครื่องที่ใช้เป็น Windows 11 x64, i5-12500H 16 threads, RAM 32GB (ทำเมื่อ 22 ต.ค. 2026) ใครเครื่องเล็กกว่านี้ก็ทำได้ แค่รอนานหน่อย

ของที่ได้ออกมาครบชุดมีดังนี้ ทุกตัวคอมไพล์จาก source ต้นทาง แล้วแพ็กเป็น zip ตัวเดียวแบบ portable
- Apache httpd 2.4
- PHP 8.4 NTS พร้อม redis / igbinary / imagick
- MariaDB 11.8
- ImageMagick 7
- lib ทุกตัวที่ต้องใช้
- ตัวคุม APM Manager

ใครรีบ ติดตั้งเครื่องมือตามข้อ 2 ให้ครบแล้วสั่งแค่นี้พอ

```
build.cmd       :: ทั้งหมด -> dist\APM-<ver>-win64.zip
deploy.cmd      :: ติดตั้ง stage\ ลง C:\APM (เฉพาะเครื่องที่ build จะใช้หรือไม่ใช้ก็ได้)
```

---

**1. ของที่ต้องมีในเครื่อง**

เครื่องขั้นต่ำ
- Windows 10/11 x64 CPU 4 core
- RAM 8GB (ส่วนตัวผมแนะนำ 16GB ขึ้นไป ไม่งั้น MariaDB กับ ImageMagick คอมไพล์นานมาก)
- ดิสก์ว่าง 60GB (เครื่องผมใช้จริงราวๆ 45GB)
- เน็ตต้องโหลดราว 4GB
- สิทธิ์ admin ใช้แค่ตอนติดตั้งเครื่องมือในข้อ 2 ตอน build ไม่ต้องใช้

ที่ต้องลงเอง 7 ตัว
1. Visual Studio 2022 Build Tools ที่ใช้คือ 17.14 MSVC 14.44 (toolset v143 หรือ "vs17") ต้องเป็นตัวนี้เท่านั้น เพราะ ABI ตรงกับ PHP official
2. Windows SDK 10.0.26100
3. Git for Windows ใช้ clone และสคริปต์ของ ImageMagick ต้องใช้ bash.exe ที่มากับมัน
4. CMake 3.24 ขึ้นไป (ผมใช้ 4.1.2) สคริปต์สั่ง `--fresh` ซึ่งมีตั้งแต่ 3.24
5. Ninja 1.13
6. PowerShell 7 ใช้รันสคริปต์ทุกตัว
7. Qt 6 (ผมใช้ 6.10.0 msvc2022_64) ใช้กับ APM Manager อย่างเดียว

ที่สคริปต์โหลดให้เอง แบบ portable ตรวจ sha256 ทุกตัว และไม่ไปยุ่งกับ PATH ของเครื่อง
- php-sdk-binary-tools 2.8.4
- Strawberry Perl 5.42.3.1
- NASM 3.01
- winflexbison 2.5.25
- vcpkg ที่ pin commit ไว้

<span style="color:red">perl ของ Git ห้ามเอามาใช้เด็ดขาดนะครับ มันเป็น cygwin สร้าง path แบบ Unix ทำให้ OpenSSL พัง ต้องใช้ Strawberry ที่สคริปต์โหลดมาเท่านั้น</span>

---

**2. ติดตั้งเครื่องมือ (ทำครั้งเดียว ใช้ admin)**

Build Tools พร้อม component ที่ต้องใช้ สั่งบรรทัดเดียวจบ

```
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --add Microsoft.VisualStudio.Component.VC.CMake.Project --add Microsoft.VisualStudio.Component.VC.ATL --add Microsoft.VisualStudio.Component.VC.ATLMFC --includeRecommended"
```

ATL กับ ATLMFC ห้ามลืม เพราะ ImageMagick ต้องใช้ ตัว Configure.exe ของมันเป็นโปรแกรม MFC ใครมี VS 2022 รุ่นไหนอยู่แล้วก็ใช้ได้หมด สคริปต์หาเองผ่าน vswhere

```
winget install --id Git.Git --exact
winget install --id Kitware.CMake --exact
winget install --id Ninja-build.Ninja --exact
winget install --id Microsoft.PowerShell --exact
```

Qt ลงผ่าน Qt Online Installer เลือก MSVC 2022 64-bit สคริปต์จะหาที่ `C:\Qt\6.10.0\msvc2022_64` ก่อน ถ้าลงไว้ที่อื่นตั้ง `QT_DIR` หรือส่ง `-QtDir` เอา

ส่วนตัวผมแนะนำให้ exclude โฟลเดอร์ build ออกจาก Defender ด้วย มันสแกน .obj เป็นหมื่นไฟล์ ช้าลงไป 2-3 เท่าเลยหละ

```
Add-MpPreference -ExclusionPath "<repo>\build","<repo>\src","<repo>\toolchain"
```

<b>เปิดตัวกันข้อมูลหลุดก่อน commit ครั้งแรก</b> สั่งบรรทัดนี้

```
git config core.hooksPath .githooks
```

จากนั้นทุก commit จะวิ่งผ่าน [scripts/check-leaks.ps1](scripts/check-leaks.ps1) ก่อน มันดักให้
- private key
- token ของ GitHub/AWS/Slack/Google/Telegram
- ค่า password= หรือ token=
- path `C:\Users\<ชื่อ>\`

คำเฉพาะของเรา เช่น ชื่อ server ภายใน IP ภายใน ชื่อลูกค้า ให้ใส่ไว้ใน `private/leak-terms.txt` บรรทัดละ regex โฟลเดอร์ private ไม่ขึ้น git รายการพวกนี้เลยไม่หลุดออกไปเอง ตอน push ก็มี CI สแกนซ้ำด้วย gitleaks อีกชั้น

---

**3. ในรีโปมีอะไรบ้าง**

- [build.cmd](build.cmd) / [deploy.cmd](deploy.cmd) ทางเข้าหลัก
- [versions.json](versions.json) เวอร์ชัน + URL + sha256 ของทุกตัว และเลข `apm_version`
- [versions.lock.json](versions.lock.json) sha256 ของไฟล์ที่ต้นทางไม่ได้ประกาศ hash ไว้
- [deps/apache/vcpkg.json](deps/apache/vcpkg.json) รายชื่อ lib ของ Apache กับ baseline ของ vcpkg
- [ports-overlay/openssl/](ports-overlay/openssl/) port OpenSSL 3.5 LTS ใช้แทน 3.6 ของ vcpkg
- [templates/](templates/) ต้นแบบ httpd.conf, apm-ssl.conf, php.ini, my.ini, index.php
- [tools/mkcert.php](tools/mkcert.php) ออก CA กับ cert ด้วย ext openssl ของ PHP
- [manager/](manager/) source ของ APM Manager (C++ Qt 6 + Win32 API)
- [scripts/](scripts/) สคริปต์ build ทุกขั้น เลขนำหน้าคือลำดับ (00 fetch, 10 vcpkg, 20 apache, 25 imagemagick, 30 php, 40 mariadb, 50 manager, 90 deploy, 95 package) มีตัวช่วยอีก 4 ตัว
  - firewall-lan เปิด firewall ให้เครื่องใน LAN
  - trust-ca ให้ Windows เชื่อ CA ของ APM
  - new-cert ออก cert เพิ่มสำหรับชื่อ vhost
  - set-path ใส่ php กับ mysql ลง PATH
- [.github/workflows/](.github/workflows/) release.yml (build + ทดสอบ + ปล่อย release) กับ security.yml (สแกนข้อมูลหลุด)

ส่วนที่สคริปต์สร้างขึ้นเองและไม่ขึ้น git ได้แก่ `downloads\` `toolchain\` `src\` `build\` (log อยู่ใน `build\logs\`) `stage\` (ตัวที่ build เสร็จ) และ `dist\` (zip)

---

**4. build ทั้งหมดในคำสั่งเดียว**

```
build.cmd
```

เวลาที่เครื่องผมใช้ ครั้งแรกแบบเปล่าๆ
1. fetch 3-5 นาที
2. vcpkg ราว 45 นาที (OpenSSL กับ libiconv กินเวลาที่สุด)
3. apache 4 นาที
4. mariadb 30 นาที
5. imagemagick 40-60 นาที
6. php 12 นาที
7. manager 2 นาที
8. package 2 นาที

รอบหลังที่มี cache แล้วทั้งชุดเหลือแค่ราวๆ 10 นาที

ถ้าพังกลางทาง แก้แล้วรันต่อจากขั้นนั้นได้ ไม่ต้องเริ่มใหม่ จะสั่งให้หยุดที่ขั้นไหนก็ได้

```
build.cmd -From php
build.cmd -From apache -To apache
```

ไว้นึกออก หรือพิมพ์อะไรผิดจะมาแก้แล้วกันน

ต่อๆ

---

**5. ขั้นที่ 1 ดาวน์โหลด source กับเครื่องมือ**

```
pwsh -File scripts\00-fetch.ps1
pwsh -File scripts\00-fetch.ps1 -Only php,httpd -Force
```

สคริปต์อ่าน [versions.json](versions.json) แล้วโหลดทุกตัวที่มี url พร้อมตรวจ sha256
- PHP, httpd, APR และ MariaDB ใช้ hash ที่ต้นทางประกาศไว้
- ตัวที่ต้นทางไม่ประกาศ hash (GitHub release, PECL) จะจด hash ไว้ใน versions.lock.json ตอนโหลดครั้งแรก รอบต่อไปถ้า hash ไม่ตรงจะหยุดทันที

เสร็จแล้วแตกเครื่องมือลง `toolchain\` และ source ลง `src\<ชื่อ>-<เวอร์ชัน>`

---

**6. ขั้นที่ 2 lib ของ Apache (vcpkg)**

```
pwsh -File scripts\10-deps-vcpkg.ps1
```

1. ถ้ายังไม่มี vcpkg จะ clone แยกไว้ที่ `toolchain\vcpkg` แล้ว checkout ไปที่ commit ใน builtin-baseline `C:\vcpkg` ที่ใช้ร่วมกับงานอื่นจะไม่ถูกแตะ
2. สั่ง `vcpkg install --triplet x64-windows --x-manifest-root=deps\apache --x-install-root=build\vcpkg_installed --overlay-ports=ports-overlay`
3. ได้ openssl 3.5.9 LTS, pcre2, zlib, nghttp2, brotli, expat, libxml2, jansson และ curl คอมไพล์ด้วย MSVC ทั้งหมด

ทำไมต้อง overlay OpenSSL เอง vcpkg ให้มาเป็น 3.6.5 ซึ่งเป็นสายสั้น หมดซัพพอร์ต พ.ย. 2026 ผมฟันธงว่าของที่จะแจกคนอื่นต้องเป็น LTS เท่านั้น 3.5 อยู่ถึง เม.ย. 2030 แล้ว patch ของ vcpkg ทั้ง 10 ตัวก็ใส่กับ 3.5.9 ได้โดยไม่ต้องแก้สักบรรทัด

---

**7. ขั้นที่ 3 Apache httpd 2.4.69 + mod_fcgid**

```
pwsh -File scripts\20-apache.ps1
```

build 4 ตัวเรียงกัน คือ APR 1.7.6 → APR-util 1.6.5 (เปิด crypto ปิด ODBC/LDAP) → httpd 2.4.69 (`-DENABLE_MODULES=i` แล้วชี้ PCRE2/nghttp2/brotli/curl/jansson ไปที่ lib ของ vcpkg) → mod_fcgid 2.3.9

ทุกตัวใส่ option เหมือนกันหมด
- `-G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo`
- `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` CMake 4 ไม่รับไฟล์ CMake รุ่นเก่าของ APR
- `-DCMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO=Release;RelWithDebInfo;` ถ้าไม่ใส่ mod_md จะไปลิงก์ curl ตัว debug
- `cmake --fresh` ทุกรอบ กันค่าเก่าค้างใน cache

เสร็จแล้วสคริปต์ทำต่อให้อีก 3 อย่าง
- คัดลอก mime.types, magic, error\ และ icons\ จาก source เพราะ cmake --install ไม่ได้ลงให้
- คัดลอกเฉพาะ DLL ที่ exe/module เรียกใช้จริง (ไล่ด้วย dumpbin /dependents แบบต่อเนื่อง)
- ถ้าเจอ binary ตัว release ไปเรียก DLL ตัว debug จะหยุดทันที

```
Server version: Apache/2.4.69 (Win64)
Server loaded:  APR 1.7.6, APR-UTIL 1.6.5, PCRE 10.49 2026-09-28
Server MPM:     WinNT
```

ได้ module 120 ตัว ที่ไม่ได้มีแค่ ldap, authnz_ldap, charset_lite, socache_dc กับ lua (httpd หาแค่ Lua 5.1)

---

**8. ขั้นที่ 4 MariaDB 11.8 LTS**

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

WITH_SSL=bundled คือใช้ wolfSSL ที่มากับ source ซึ่งเป็นค่าปกติของ MariaDB บน Windows ส่วน engine หนักๆ ที่เครื่อง dev ไม่ได้ใช้ผมตัดทิ้งหมด เวลาคอมไพล์ลดไปเยอะ

ลองรันที่พอร์ต 3399 ได้แบบนี้

```
version()       @@character_set_server  @@have_ssl
11.8.9-MariaDB  utf8mb4                 YES
```

---

**9. ขั้นที่ 5 ImageMagick 7 (ให้ imagick ใช้)**

```
pwsh -File scripts\25-imagemagick.ps1
```

ตัวสำเร็จรูปของ PHP team เป็น 7.1.1-47 เก่าไป 19 เดือน ImageMagick แก้ช่องโหว่บ่อยมาก ผมเลยไม่เอา คอมไพล์เองด้วย toolchain ทางการของ ImageMagick (github.com/ImageMagick/Windows) มันคอมไพล์ delegate lib ให้อีกราว 40 ตัว เช่น jpeg-turbo, png, webp, heif, jpeg-xl, tiff, rsvg, freetype, lcms

1. clone ImageMagick/Windows แล้วสั่ง `clone-repositories.sh --imagemagick7 --commit <tag>` ผ่าน bash ของ Git ตัว tag เอามาจาก versions.json ส่วนเวอร์ชันของ Configure กับ Dependencies จะถูกล็อกตาม tag นั้นให้เอง
2. <b>แก้ Configure\src\Options.cpp ให้ใช้ channel mask 32 bit</b> ตัว Configure บังคับ 64 bit บน x64 ซึ่งทำให้ header ใช้ได้แต่กับ C++ แต่ imagick เขียนด้วย C เลยคอมไพล์ไม่ผ่าน ส่วน 32 คือค่าปกติบน Linux อยู่แล้ว
3. build Configure.sln ด้วย `/p:PlatformToolset=v143` (โปรเจกต์ตั้งไว้เป็น v145 ของ VS2026)
4. `Configure.Release.x64.exe /noWizard /VS2022 /x64 /dynamic /Q16 /hdri`
5. `msbuild IM7.Dynamic.x64.sln /m /t:Build /p:Configuration=Release,Platform=x64`
6. จัดโฟลเดอร์ `build\im-dev` (MagickCore\, MagickWand\, CORE_RL_*.lib) ให้ imagick ใช้ตอนคอมไพล์

```
Version: ImageMagick 7.1.2-34 Q16-HDRI x64
Delegates (built-in): bzlib cairo freetype gslib heic jng jp2 jpeg jxl lcms lqr lzma openexr pangocairo png ps raqm raw rsvg tiff webp xml zip zlib zstd
#define MAGICKCORE_CHANNEL_MASK_DEPTH 32
```

ปล. HEIC อ่านได้แต่เขียนไม่ได้ เพราะไม่ได้รวม x265 ด้วยเหตุผลเรื่องไลเซนส์ ส่วน webp avif png jpeg อ่านเขียนได้ปกติ

---

**10. ขั้นที่ 6 PHP 8.4 NTS + PECL**

```
pwsh -File scripts\30-php.ps1 -Step all      :: หรือ deps | help | build
```

ทั้งหมดรันในสภาพแวดล้อมของ php-sdk (`phpsdk-starter.bat -c vs17 -a x64`) ขั้น deps จะดึง dependency ชุด 8.4 vs17 ของ PHP team มาให้ ส่วนขั้น build ลบ obj เก่าทิ้งทุกครั้ง

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

พอ nmake install เสร็จ สคริปต์จะคัดลอก DLL ที่ PHP เรียกใช้จริง พร้อม DLL กับไฟล์ xml ของ ImageMagick ไปวางข้าง php.exe ให้

```
PHP 8.4.27 (cli) (NTS Visual C++ 2022 x64)
{"jit":true,"webp":true,"imagick":"ImageMagick 7.1.2-34 Q16-HDRI x64 ..."}
webp=1696 png=2266 jpg=5862 avif=1456      (imagick เขียนไฟล์ได้ หน่วยเป็น byte)
```

ได้ 51 module ตรงกับเครื่อง production ทุกตัว ขาดแค่ posix sysvmsg sysvsem sysvshm ซึ่ง Windows ไม่มีให้อยู่แล้ว

---

**11. ขั้นที่ 7 APM Manager**

```
pwsh -File scripts\50-manager.ps1 [-QtDir C:\Qt\6.10.0\msvc2022_64]
```

build โฟลเดอร์ manager\ ด้วย CMake + Ninja แล้ว windeployqt ให้ ได้ stage\manager ราว 51MB

เลขเวอร์ชันสร้างตอน configure
- apm_version จาก versions.json
- build = จำนวน commit
- hash ของ commit ถ้ามีไฟล์ยังไม่ commit จะมี -dirty ต่อท้าย
- วันที่

ไปโผล่ที่หน้า About, ท้ายหน้าต่าง และ Properties ของ exe

source แต่ละไฟล์ทำอะไร
- `Engine` ดูสถานะ (process, service, พอร์ตที่ฟังอยู่) แล้วสั่ง start/stop/restart ทั้งแบบ process และแบบ service อ่านเวอร์ชันจาก exe ใหม่ทุกครั้งที่ไฟล์เปลี่ยน อัปเดต engine เมื่อไหร่ก็ยังคุมได้
- `Win` ตัวคุยกับ Win32 คือ process, named event, service พร้อม DACL, การขอ UAC และ PATH แบบ REG_EXPAND_SZ
- `Setup` / `SetupDialog` ใช้ตอนรันครั้งแรกและตอนย้ายโฟลเดอร์ เขียน config จาก template ออก cert สร้าง datadir จด apm.json
- `MainWindow` การ์ด tray และการย่อลง tray
- `SettingsDialog` เปิดตอน sign-in, service, พอร์ต/LAN/firewall, PATH, trust CA
- `UpdateDialog` อัปเดตผ่าน GitHub Releases (โหลด + ตรวจ sha256 + หยุด/แทน/start) และติดตั้งจาก zip/โฟลเดอร์
- `Elevated` งานที่ต้องใช้ admin เรียกผ่าน `APMManager.exe --elevated <op>` กับ UAC
- `AboutDialog` / `Theme` credit, เวอร์ชัน, ธีมสว่าง/มืด, icon ของ Material

<b>หยุดแบบ graceful ทำยังไงในเมื่อไม่มีหน้าต่าง console</b> ผมไปไล่ดูใน source ของแต่ละตัว
- Apache ตัวแม่รอ named event ชื่อ `ap<pid>_shutdown` (ส่วน `_restart` คือ restart แบบ graceful)
- MariaDB รอ event `MySQLShutdown<pid>`
- Manager ยิง event พวกนี้ก่อน ถ้าเกินเวลาแล้วยังไม่ปิดค่อย kill ทิ้ง

ผมดู log ตอนทดสอบแล้ว MariaDB ขึ้น Shutdown complete ปิดถูกต้องจริง

---

**12. ขั้นที่ 8 แพ็ก zip แบบ portable**

```
pwsh -File scripts\95-package.ps1
```

ได้ `dist\APM-1.0.0-win64\` ขนาด 403MB กับ zip 134MB พร้อมไฟล์ .sha256 ข้างในมีดังนี้
- engine ทั้งหมด ไม่มี pdb/lib/include และยังไม่มี config ที่ render แล้ว
- manager\ templates\ tools\ versions.json README.txt
- cacert.pem ของ Mozilla (ตรวจ sha256 แล้ว)
- VC++ runtime แบบ app-local วางไว้ทุกโฟลเดอร์ที่มี exe และ vcomp140.dll ใน php (OpenMP ของ ImageMagick)
- licenses\ 133 ไฟล์

VC++ runtime สำคัญมาก ถ้าไม่ใส่ เครื่องที่ไม่เคยลง VC++ redist จะเปิดไม่ขึ้นเลย

ผลทดสอบ portable กับ zip ตัวจริง
1. แตกลง `D:\APM test\portable A` (มีเว้นวรรค) แล้วสั่ง `--setup --http 8081 --https 8444 --db 3308` ได้ config, CA, cert, datadir ครบ httpd -t ผ่าน
2. `--cli start` เปิดหน้าเว็บได้ PHP 8.4.27 + MariaDB 11.8.9 https ตอบ 200 และ php-cgi รันจากโฟลเดอร์นั้นจริง
3. ย้ายไป `D:\APM test\moved B (copy)` (มีวงเล็บ) แล้วสั่ง `--cli status` มันไม่ยอมทำงาน ตอบ exit 2 บอกว่าโฟลเดอร์ถูกย้าย ให้ --setup ก่อน
4. สั่ง `--setup` แล้วมันรู้ว่าถูกย้าย เขียน config ใหม่โดยใช้พอร์ตเดิม ข้อมูลเดิมยังอยู่ ออก cert ใหม่ให้
5. start → เปิดหน้า → stop ผ่านหมด

---

**13. ขั้นที่ 9 ติดตั้งบนเครื่องที่ build, ลองรัน, LAN, cert, PATH**

```
deploy.cmd                                            :: C:\APM พอร์ต 8080/8443/3307
deploy.cmd -HttpPort 80 -HttpsPort 443 -DbPort 3306 -Lan -ResetConfig
```

deploy ทำงานแบบนี้
- ไม่ยอมทำงานถ้า server หรือ service ของ APM ยังรันอยู่
- mirror engine ให้ โดยไม่ทับ conf, php.ini และ my.ini ของเรา แล้วติดตั้ง manager, templates, tools
- render config เฉพาะไฟล์ที่ยังไม่มี หรือตอนสั่ง -ResetConfig (ซึ่งจะออก cert ของ server ใหม่และ sync data\mysql\my.ini ให้ด้วย)
- สร้าง datadir ด้วย mariadb-install-db (เป็นแค่ bootstrap ไม่เปิดพอร์ต) แล้วเช็ก httpd -t
- ไม่ start อะไรเองเลย

ลองรันแบบไม่ต้องลง service

```
C:\APM\manager\APMManager.exe --cli start
curl.exe -s http://localhost/
C:\APM\manager\APMManager.exe --cli stop
```

ผลที่เครื่องผม
- PHP วิ่งผ่าน fcgid (cgi-fcgi) และ JIT เปิดอยู่
- บันทึกภาษาไทยผ่าน PDO/mysqli แล้วอ่านกลับมาได้ตรง ตารางใหม่ได้ collation utf8mb4_unicode_ci
- session ใช้ได้ imagick กับ GD เขียน webp ได้ curl ออก https ข้างนอกได้
- .htaccess rewrite ใช้ได้ brotli กับ gzip ทำงาน
- ขอ .env ได้ 403
- HTTP/2 ผ่าน TLS 1.2/1.3 ได้ ส่วน TLS 1.1 ถูกปฏิเสธ
- ab -n 3000 -c 30 ได้ 678 req/s ไม่มี request ล้มสักตัว

เปิดให้ LAN เข้า (เฉพาะ Apache) สั่ง `deploy.cmd -Lan -ResetConfig` แล้วเปิด firewall ครั้งเดียวด้วย admin

```
pwsh -File scripts\firewall-lan.ps1          :: TCP 80,443 จาก LocalSubnet -> httpd.exe ทุก profile
```

ผมลองจากอีกเครื่องใน LAN
- http ได้ 200 https ได้ 200
- ?info=1 ได้หน้าปกติ (phpinfo ดูได้แค่เครื่องตัวเอง)
- /server-status ได้ 403
- พอร์ต 3306 ปิด

ตามที่ตั้งใจไว้หมด

cert ของเครื่อง
- CA อายุ 10 ปี
- cert ของ server อายุ 825 วัน SAN คือ localhost, 127.0.0.1, ::1 ถ้าเปิด LAN จะเพิ่มชื่อเครื่องกับ IP ของ LAN ให้

```
pwsh -File scripts\trust-ca.ps1                       :: ให้ Windows เชื่อ CA (เด้งถามยืนยันครั้งเดียว)
pwsh -File scripts\new-cert.ps1 -Hosts myapp.localhost :: cert ของ vhost ชื่อ *.localhost ไม่ต้องแก้ hosts
pwsh -File scripts\set-path.ps1 [-Scope User]          :: ใส่ php กับ mysql ลง PATH ให้ VS Code ใช้
```

<span style="color:red">เรื่อง PATH ผมพลาดมาแล้วกับตัว ตอนแรกใช้ [Environment]::SetEnvironmentVariable แก้ PATH ปรากฏว่ามันขยาย %SystemRoot% เป็น C:\WINDOWS แล้วเขียนกลับเป็น REG_SZ ถาวร ตอนนี้สคริปต์กับ Manager แก้ registry ตรงแบบ REG_EXPAND_SZ แล้ว ใครเขียนสคริปต์แก้ PATH เองระวังจุดนี้ด้วย</span>

---

**14. อัปเดตตอนมี security patch และการปล่อย release**

1. แก้ version, url, sha256 ใน versions.json โดย hash เอามาจาก
   - PHP: `https://www.php.net/releases/index.php?json&version=8.4&max=1`
   - httpd/APR: ไฟล์ .sha256 ข้าง tarball บน downloads.apache.org
   - MariaDB: `https://downloads.mariadb.org/rest-api/mariadb/11.8/latest/`
2. `pwsh -File scripts\00-fetch.ps1 -Only php` แล้ว `build.cmd -From php`
3. ขยับ apm_version แล้ว commit และ push จากนั้นปล่อย release ได้ 2 ทาง
   - build เองที่เครื่อง (v1.0.0 ใช้ทางนี้): สั่ง build.cmd ทดสอบ zip ให้ผ่าน แล้วสร้าง release v1.0.1 บน GitHub แนบ `dist\APM-1.0.1-win64.zip` กับไฟล์ .sha256
   - ให้ CI build: เข้า Actions → release → Run workflow แล้วใส่ release_tag = v1.0.1 ตัวนี้ผมตั้งให้กดเองเท่านั้น เพราะ build รอบแรกกิน runner Windows ราว 3-4 ชั่วโมง repo private นาทีหมดไวมาก

   ฝั่งคนใช้แค่กด Check for updates ใน Manager มันจะอ่าน releases/latest แล้วหยิบไฟล์ APM-*-win64.zip กับ .sha256 มาเอง

กรณีพิเศษ
- OpenSSL ของ Apache แก้ที่ ports-overlay\openssl\vcpkg.json (เวอร์ชัน) กับ portfile.cmake (SHA512 ของ tarball บน GitHub) แล้ว `-From vcpkg`
- lib อื่นของ Apache ให้ `git -C toolchain\vcpkg pull` แล้วเปลี่ยน builtin-baseline เป็น commit ใหม่
- ImageMagick เปลี่ยน tag ใน sources.imagemagick.version แล้ว `-From imagemagick`
- PHP 8.5 ผมจะย้ายก็ต่อเมื่อเครื่อง production ย้ายเท่านั้น แก้แค่ sources.php กับ php_deps_series ส่วนใครจะย้ายก่อนก็ตามสะดวก

---

**15. ปัญหาที่เจอมาทั้งหมดและวิธีแก้ (แก้ในสคริปต์ให้แล้วทุกข้อ)**

1. `'buildconf.bat' is not recognized` สาเหตุคือ environment ตั้ง NoDefaultCurrentDirectoryInExePath=1 ไว้ แก้โดยเรียกเป็น `.\buildconf.bat` / `.\configure.bat`
2. configure ไม่รู้จัก `--enable-gd` เพราะชื่อ flag บน Windows ไม่เหมือน Linux ต้องใช้ `--with-gd` `--with-dom` ตรวจได้ด้วย `-Step help`
3. GD ไม่มี WebP/AVIF และ PCRE JIT ไม่ทำงาน เพราะ `--disable-all` ไปปิด option ที่ปกติเปิดอยู่ด้วย ต้องสั่งเปิดเองทุกตัว
4. แก้ configure แล้วผลไม่เปลี่ยน เพราะ nmake ใช้ obj เก่าต่อ เลยต้องลบ build\php-obj กับ stage\php ทุกรอบ
5. `igbinary/igbinary.h: No such file` เพราะ redis include มันแบบมีชื่อโฟลเดอร์นำหน้า แก้ด้วย `--with-extra-includes=build\php-pecl`
6. php.exe ไม่ยอมรัน exit 53 เพราะ DLL ของ deps ไม่อยู่ข้าง exe แก้โดยไล่ dumpbin แล้วคัดลอกให้ครบ
7. เปลี่ยนเวอร์ชัน PECL แล้วยังได้ตัวเก่า เพราะเดิมคัดลอกไป build\php-pecl แค่ครั้งแรก ตอนนี้ลบแล้วคัดลอกใหม่ทุกรอบ
8. `00-fetch.ps1 -Only php,httpd` ผ่าน pwsh -File แล้วไม่โหลดอะไรเลย เพราะโหมด -File ส่ง "php,httpd" มาเป็น string ก้อนเดียว ตอนนี้สคริปต์แยกลูกน้ำเอง
9. `Compatibility with CMake < 3.5 has been removed` เพราะไฟล์ CMake ของ APR/httpd เก่า แก้ด้วย `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`
10. `Unexpected enablement value "N" for mod_lua` เพราะ httpd รับค่าแค่ O/i/I/a/A แล้วค่าเก่ายังค้างใน cache แก้โดยไม่ใส่ ENABLE_LUA และใช้ `cmake --fresh`
11. mod_lua ถูก build กับ Lua 5.5 เพราะ FindLua51 ไปจับ Lua ของ vcpkg เลยเอา lua ออกจาก manifest
12. mod_md.so ไปเรียก libcurl-d.dll เพราะ imported target ของ vcpkg ไม่มี RelWithDebInfo แก้ด้วย CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO และตั้งให้สคริปต์ดัก
13. ไม่มี conf\mime.types เพราะ cmake --install ของ httpd ไม่ลงให้ เลยคัดลอกจาก docs\conf เอง
14. httpd ปิดตัวทันทีที่เปิด log ขึ้น AH00099 could not create httpd.pid เพราะ PidFile ปกติอยู่ใน `<ServerRoot>/logs` แต่ deploy เก็บ log ไว้ที่อื่น ใส่ `PidFile "${APM}/logs/apache/httpd.pid"` ใน template แล้ว
15. `ImageMagick was build with a 64 channel bit mask and that requires a C++ compiler` เพราะ Configure บังคับ 64 บน x64 แก้ Options.cpp ให้เป็น 32
16. Configure.exe ของ ImageMagick คอมไพล์ไม่ผ่าน เพราะหา v145 ไม่เจอ ใส่ `/p:PlatformToolset=v143` กับ `/VS2022`
17. OpenSSL พังเพราะใช้ perl ของ Git ต้องใช้ Strawberry เท่านั้น
18. plugin ของ MariaDB (ha_archive, auth_ed25519 ฯลฯ) หายหลัง deploy เพราะ `robocopy /XD lib` ตัดโฟลเดอร์ชื่อ lib ทุกตัวรวม mariadb\lib\plugin ไปด้วย ตอนนี้เก็บ lib ไว้ แล้วตัดแค่ไฟล์ .lib
19. เปลี่ยนพอร์ต DB ด้วย -ResetConfig แล้วไม่เปลี่ยน เพราะ server อ่าน data\mysql\my.ini ตอนนี้ sync ให้แล้ว
20. แตก zip ไว้ใน path ที่มีเว้นวรรคแล้ว Apache ขึ้น Wrapper D:/APM cannot be accessed เพราะ mod_fcgid ตัดคำสั่งด้วยช่องว่าง แก้โดยใส่ quote ซ้อนข้างใน `FcgidWrapper "\"${APM}/php/php-cgi.exe\"" .php`
21. path มีวงเล็บ (copy) แล้วออก cert ไม่ได้ ขึ้น Undefined constant OPENSSL_KEYTYPE_EC เพราะ `( )` เป็น syntax ของ INI ใน php -d แก้โดยครอบค่าด้วย quote และให้ mkcert แจ้งชัดๆ
22. PATH ของเครื่องกลายเป็น REG_SZ และ %SystemRoot% ถูกขยายถาวร (เรื่องตามที่เล่าไว้ข้อ 13)
23. ช่องพอร์ตใน UI ขึ้นเป็นเลขไทย ๘๐ เพราะ Qt ใช้ locale ไทยของ Windows ตั้งให้เป็นเลขอารบิกแล้ว
24. .gitignore ที่เขียนว่า src/ ไปซ่อน manager/src ด้วย ต้องเขียนเป็น /src/ ให้จับเฉพาะ root
25. ทดสอบด้วย `Start-Process ... -Wait` แล้วค้าง เพราะ -Wait ของ PowerShell 7 รอทั้งต้นไม้ process รวม httpd/mariadbd ที่ start ขึ้นมา ตัว manager ไม่ได้ผิด เวลาทดสอบให้ใช้ `cmd /c "... > out.txt"` แทน
26. ย้ายโฟลเดอร์แล้ว server ไปอ่าน config ที่ชี้ path เก่า ตอนนี้ apm.json จด root ไว้ Manager เทียบให้ทุกครั้ง ส่วน --cli จะไม่ยอมทำงานจนกว่าจะ --setup

---

**16. ของที่ได้ออกมา**

```
stage\apache\    httpd 2.4.69 + module 120 ตัว
stage\php\       PHP 8.4.27 NTS + ext\ + DLL ของ deps + ImageMagick
stage\mariadb\   MariaDB 11.8.9
stage\manager\   APMManager.exe + Qt runtime
dist\APM-<ver>-win64.zip (+ .sha256)
```

ระบบนี้ทำด้วย Claude + VS Code ทั้งหมด ผมคุมทิศทางกับทดสอบเอง เห็นว่าดีเลยมาบอกต่อ ใครเอาไปใช้ฟรีได้เลย แต่ถ้าจะเอาไปขายต่อคุยกันก่อนนะครับ (ดู [LICENSE](LICENSE)) ติดต่อได้ที่ wanpean@gmail.com หรือ [wanpean.com](https://wanpean.com)

ตรงไหนพิมพ์ผิดหรือขั้นไหนทำตามแล้วไม่ผ่าน บอกได้เลย เดี่ยวมาแก้ให้ครับ :)
