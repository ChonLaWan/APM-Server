# APM - Apache + PHP + MariaDB for Windows (built from source)

**[ภาษาไทย](#th)** · **[English](#en)** · คู่มือ build / Build guide: [ไทย](HOWTO-BUILD.md) · [English](HOWTO-BUILD.en.md) · [Release ล่าสุด / Latest release](../../releases/latest)

<p>
  <img src="docs/images/manager-dark.png" width="420" alt="APM Manager (dark)">
  <img src="docs/images/manager-light.png" width="420" alt="APM Manager (light)">
</p>

<a name="th"></a>

## ภาษาไทย

พอดีเปิด XAMPP ในเครื่องมาดูเวอร์ชัน แล้วตกใจ PHP ยังเป็น 8.2.12 ตั้งแต่ปี 2023 อยู่เลย Apache 2.4.58 ส่วน MariaDB 10.4 ก็หมดซัพพอร์ตไปแล้ว ช่องโหว่ที่ upstream แก้ไปนานแล้วยังค้างอยู่ในเครื่องผมครบทุกตัว รอ XAMPP ออกรุ่นใหม่ก็ไม่รู้จะมาเมื่อไหร่ ผมเลยตัดสินใจคอมไพล์เองทั้งชุด ตั้งชื่อว่า APM มาจาก Apache PHP MariaDB นี่แหละครับ

แล้วก็มาเจอของจริงพอดี [php2shell CVE-2026-109618 / 109619](https://www.linuxthai.org/article/php2shell-cve-2026-109618-109619-patch-now) เป็นช่องโหว่ RCE ของ PHP ระดับวิกฤต CVSS 9.2 แถม exploit ออกมาแล้วด้วย รุ่นที่แก้คือ 8.5.12 / 8.4.27 / 8.3.36 / 8.2.35 ตัว APM 1.0.0 ใช้ 8.4.27 ผม build ตามหลังวันที่แพตช์ออกแค่วันเดียว ถ้ารอ XAMPP คงยังโดนอยู่หละ

ตอบแบบฟันธงเลยว่า ถ้าต้องใช้ PHP บน Windows แล้วอยากได้ security patch ทันวัน ต้องคอมไพล์เองเท่านั้น รอแพ็กสำเร็จรูปไม่ทันแน่นอน เวอร์ชันทุกตัวผมล็อกไว้ใน [versions.json](versions.json) ไฟล์เดียว มี patch ใหม่ออกมาก็แก้เลขเวอร์ชัน สั่ง `build.cmd` ก็ได้ zip ชุดใหม่ ส่วน extension กับ module ผมตั้งให้ตรงกับเครื่อง production (Linux) ที่ใช้จริง เขียนโค้ดบนเครื่องตัวเองผ่าน ขึ้น server ก็ผ่านด้วย

**ที่อยู่ในรุ่น 1.0.0**

1. Apache httpd 2.4.69 พร้อม OpenSSL 3.5 LTS, HTTP/2, brotli, mod_fcgid
2. PHP 8.4.27 NTS x64 เป็นตัว security release ล่าสุดของสาย 8.4 (ออก 21 ต.ค. 2026) เปิด OPcache + JIT ไว้แล้ว
3. MariaDB 11.8.9 LTS ซัพพอร์ตยาวถึงปี 2030
4. ImageMagick 7.1.2-34 ให้ imagick ใช้
5. APM Manager 1.0.0 ตัวคุมที่ทำด้วย Qt 6 มีทั้งธีมสว่างและธีมมืด ตาม Windows

**วิธีใช้แบบ portable (ไม่ต้องติดตั้ง)**

1. โหลด `APM-<version>-win64.zip` จากหน้า [Releases](../../releases) แล้วเทียบค่าในไฟล์ `.sha256` ด้วยก็ดีครับ
2. แตกไว้ที่ไหนก็ได้ จะ `C:\APM` หรือ `D:\Tools\APM` ก็ตามสะดวก path มีเว้นวรรคมีวงเล็บผมลองแล้วผ่านหมด
3. เปิด `manager\APMManager.exe` ครั้งแรกจะมีหน้า Set up ขึ้นมาให้ มันจะทำให้เองทั้งหมดนี้
   - เลือกพอร์ตที่ว่าง ปกติ 80 / 443 / 3306 ถ้าไม่ว่างจะเสนอ 8080 / 8443 / 3307
   - เขียน config จาก `templates\`
   - ออก cert HTTPS ของเครื่อง
   - สร้างฐานข้อมูลเปล่า user `root` ไม่มีรหัสผ่าน ใช้ได้เฉพาะเครื่องนี้
4. กด Start all แล้ว Open site ไฟล์เว็บเอาไปวางที่ `www\default\`

VC++ runtime ผมแพ็กไว้ใน zip แล้ว เครื่องใหม่เอี่ยมก็รันได้ ไม่เขียน registry ด้วย เว้นแต่จะเปิดฟีเจอร์ที่ต้องใช้เอง เช่น เปิดตอน sign-in, ติดตั้ง service, ใส่ PATH

อยากย้ายโฟลเดอร์ไปไหนก็ย้ายได้เลย พอเปิด Manager มันจะรู้ว่าถูกย้ายมา แล้วเขียน config ใหม่ให้ พอร์ตเดิม ฐานข้อมูลเดิมยังอยู่ครบ

**APM Manager ทำอะไรได้บ้าง**

<p>
  <img src="docs/images/settings.png" width="420" alt="Settings">
  <img src="docs/images/update.png" width="420" alt="Updates">
</p>

1. Start / Stop / Restart ทั้ง Apache และ MariaDB โดย restart ของ Apache เป็นแบบ graceful คนที่กำลังเปิดเว็บอยู่ไม่หลุด กดเปิด error log กับไฟล์ config ได้จากการ์ดเลย หน้าการ์ดบอกเวอร์ชัน พอร์ต และจำนวน worker ของ PHP
2. ปิดหน้าต่างหรือกดย่อแล้วจะลงไปอยู่ที่ tray ข้างนาฬิกา สีของ icon บอกสถานะ คลิกขวามีเมนู Start/Stop/Open site ให้
3. Auto start มีให้ 3 แบบ ตามใจชอบ
   - เปิด Manager ตอน sign-in
   - start server ตอนที่ Manager เปิด
   - ติดตั้งเป็น Windows Service ให้ขึ้นพร้อม Windows ไปเลย (กด UAC ครั้งเดียว)
4. ถ้าไม่ติดตั้ง service มันจะรันเป็น process ธรรมดา ไม่ต้องใช้สิทธิ์ admin สักนิด ถ้าติดตั้ง service จะรันด้วยบัญชี `LocalService` ที่สิทธิ์ต่ำ ส่วน user ที่ login อยู่สั่ง start/stop ได้โดยไม่ต้องกด UAC ซ้ำ
5. เปลี่ยนพอร์ตได้ เปิดให้เครื่องอื่นใน LAN เข้าได้ด้วย firewall จะเปิดให้เฉพาะเครื่องใน subnet เดียวกัน และเฉพาะ httpd.exe เท่านั้น ส่วน MariaDB ผมไม่ยอมเปิดออกนอกเครื่องเด็ดขาด
6. ใส่ `php` กับ `mysql` ของ APM ลง PATH ให้ VS Code กับ terminal ใช้ได้เลย จุดนี้ผมโดนมาแล้วกับตัว รายละเอียดอยู่ในคู่มือ build
7. กดให้ Windows เชื่อ CA ของ APM แล้ว `https://localhost` จะไม่มีคำเตือนอีก
8. ปุ่ม Check for updates จะถาม GitHub Releases ของรีโปนี้ ถ้ามีรุ่นใหม่จะโหลด zip มาตรวจ sha256 แล้วทำตามลำดับให้เอง คือหยุด server → แทน binary → start ใหม่ ส่วน config, ฐานข้อมูล, www, ssl และ log ไม่แตะเลย เครื่องที่ไม่มีเน็ตก็ติดตั้งจาก zip หรือโฟลเดอร์เองได้

**สั่งผ่าน command line**

```
manager\APMManager.exe --cli status
manager\APMManager.exe --cli start|stop|restart [apache|mariadb|all]
manager\APMManager.exe --setup [--http 80 --https 443 --db 3306] [--lan] [--reset]
manager\APMManager.exe --tray
```

`--cli` ใช้ engine ตัวเดียวกับหน้าต่าง ค่า exit code มีดังนี้
- `0` ผ่าน
- `1` มี error เช่น พอร์ตชน หรือ start ไม่ขึ้น
- `2` ยังไม่ได้ setup หรือโฟลเดอร์ถูกย้ายมา ให้สั่ง `--setup` ก่อน

**ในโฟลเดอร์มีอะไรบ้าง**

```
APM\
  manager\     APMManager.exe + Qt runtime
  apache\      httpd 2.4 (conf\httpd.conf ถูกเขียนตอน setup)
  php\         PHP 8.4 NTS + ext\ + ImageMagick (php.ini ถูกเขียนตอน setup)
  mariadb\     MariaDB 11.8 (my.ini ถูกเขียนตอน setup)
  templates\   ต้นแบบ config
  tools\       mkcert.php ออก cert ด้วย ext openssl ของ PHP
  www\         เว็บ (default\ คือ DocumentRoot)
  data\        datadir ของ MariaDB
  ssl\         apm-ca.crt, localhost.crt, cacert.pem
  logs\ tmp\
  apm.json     ค่าที่ setup ไว้
  licenses\    ไลเซนส์ของทุกตัว
```

**Extension ของ PHP** มีครบตามเครื่อง production ทุกตัว ยกเว้น `posix sysvmsg sysvsem sysvshm` ซึ่ง Windows ไม่มีให้อยู่แล้ว ได้แก่ `bcmath calendar ctype curl date dom exif FFI fileinfo filter ftp gd(webp avif jpeg png freetype) gettext hash iconv igbinary imagick intl json libxml mbstring mysqli mysqlnd openssl pcre(JIT) PDO pdo_mysql pdo_sqlite Phar random redis(+igbinary, session) Reflection session shmop SimpleXML soap sockets sodium SPL sqlite3 standard tokenizer xml xmlreader xmlwriter xsl Zend OPcache zip zlib`

**เรื่องความปลอดภัย**

<span style="color:red">MariaDB ฟังที่ 127.0.0.1 เท่านั้น และ root ไม่มีรหัสผ่าน ตั้งไว้แบบนี้ให้ใช้ในเครื่อง dev สะดวก ใครจะเปิด DB ให้เครื่องอื่นเข้า ต้องตั้งรหัสก่อนนะครับ อย่าลืม</span>

- `phpinfo()` บนหน้าแรกกับ `/server-status` ตอบเฉพาะเครื่องตัวเอง
- `.env`, `.git` และ `.ht*` ถูกปฏิเสธ
- TLS ใช้แค่ 1.2/1.3
- service รันด้วย `LocalService` ไม่ใช่ LocalSystem
- ทุกไฟล์ที่ดาวน์โหลดมาถูกตรวจ sha256

ฝั่งรีโปผมกันข้อมูลหลุดไว้ 2 ชั้น
- [scripts/check-leaks.ps1](scripts/check-leaks.ps1) ทำงานเป็น pre-commit hook (สั่ง `git config core.hooksPath .githooks`) ตัวนี้ดักทั้ง key, token, password และ path ส่วนตัว ส่วนคำเฉพาะ เช่น ชื่อ server ภายใน เก็บไว้ใน `private/leak-terms.txt` ที่ไม่ขึ้น git
- ทุกครั้งที่ push จะมี [.github/workflows/security.yml](.github/workflows/security.yml) สแกนซ้ำด้วย gitleaks อีกรอบ

**บทความที่เขียนไว้บน LinuxThai**
- [Build APM บน Windows: สารบัญ](https://www.linuxthai.org/article/apm-build-windows-index) ซีรีส์ 7 ตอน พาคอมไพล์ Apache 2.4.69, PHP 8.4.27 และ MariaDB 11.8.9 บน Windows 11 เอง มีบอกเวลา พื้นที่ดิสก์ และทางลัดสามคำสั่ง
- [PHP มีช่องโหว่ RCE ระดับวิกฤตสองตัว (php2shell) อัปเดตด่วนวันนี้](https://www.linuxthai.org/article/php2shell-cve-2026-109618-109619-patch-now) ต้นเหตุที่ต้องรีบออก release นี้ มีวิธีเช็กเวอร์ชันกับวิธีอัปเดตทั้ง Debian, RHEL, control panel ของ hosting, Docker และ Windows

**จะ build เองจาก source** ตามคู่มือนี้ได้เลย [HOWTO-BUILD.md](HOWTO-BUILD.md) ผมเขียนไว้ทุกขั้น คำสั่ง ผลลัพธ์จริง และปัญหาที่เจอทุกข้อพร้อมวิธีแก้ (ภาษาอังกฤษอยู่ที่ [HOWTO-BUILD.en.md](HOWTO-BUILD.en.md))

```
build.cmd     :: fetch -> lib ของ vcpkg -> Apache -> MariaDB -> ImageMagick -> PHP -> Manager -> dist\APM-<ver>-win64.zip
deploy.cmd    :: ติดตั้ง stage\ ลง C:\APM บนเครื่องที่ build
```

ฝั่ง CI ตัว [.github/workflows/release.yml](.github/workflows/release.yml) ตั้งให้กดรันเองที่ Actions → release → Run workflow มันทำให้ 3 อย่าง
1. build ทั้งชุดบน Windows runner
2. แตก zip แล้วลอง setup → start → เปิดหน้าเว็บผ่าน PHP + MariaDB → stop
3. ถ้าใส่ release_tag มาด้วย จะแนบ zip เข้า Release นั้นให้

ที่ไม่ตั้งให้รันเองอัตโนมัติ เพราะ build รอบแรกกินเวลาราว 3-4 ชั่วโมง ส่วนการสแกนข้อมูลหลุดรันทุกครั้งที่ push

**เลขเวอร์ชัน** เป็นรูปแบบ `<version> build <n> (<commit>, <date>)` เช่น `1.0.0 build 3 (7a106aa, 2026-10-22)`
- build คือจำนวน commit ใน git
- ถ้า build ตอนที่มีไฟล์ยังไม่ commit จะมี `-dirty` ต่อท้าย เห็นปุ๊บรู้เลยว่ามาจากโค้ดชุดไหน
- ดูได้ที่หน้า About, ที่ Properties ของ exe และใน README.txt ใน zip

**Credit** ออกแบบและทำโดย ChonLaWan
- [wanpean@gmail.com](mailto:wanpean@gmail.com)
- [wanpean.com](https://wanpean.com)
- [github.com/ChonLaWan/APM-Server](https://github.com/ChonLaWan/APM-Server)

ระบบนี้พัฒนาด้วย Claude + VS Code ทั้งหมด UI ผมออกแบบให้ดูง่ายที่สุดแล้ว ถ้าตรงไหนใช้ยากบอกได้ครับ :)

**License** สคริปต์กับ APM Manager ในรีโปนี้เป็น MIT + Commons Clause ([LICENSE](LICENSE))
- ใช้ฟรี แก้ไขได้ แจกต่อได้ ทั้งที่บ้านและที่ทำงาน
- ถ้าจะเอาไปขาย หรือเอาไปทำสินค้าหรือบริการที่เก็บเงินโดยพึ่งตัวนี้เป็นหลัก ต้องซื้อ license เชิงพาณิชย์ก่อน ติดต่อ [wanpean@gmail.com](mailto:wanpean@gmail.com)

ส่วน binary แต่ละตัวยังใช้ไลเซนส์ของมันเอง ได้แก่
- Apache-2.0
- PHP-3.01
- GPL-2.0 (MariaDB)
- ImageMagick License
- LGPL-3.0 (Qt แบบ dynamic link)

ไลเซนส์ทั้งหมดรวมไว้ใน `licenses\` ใน zip แล้ว ส่วน source ของตัวที่เป็น GPL/LGPL ดูได้ที่ URL ต้นทางใน [versions.json](versions.json) และ build ซ้ำได้ด้วยสคริปต์ในรีโปนี้

ไว้นึกอะไรออก หรือตรงไหนพิมพ์ผิด เดี่ยวมาแก้แล้วกันครับ

ปล. ถ้าใครเอาไปใช้แล้วเจอปัญหา เปิด issue ไว้ได้เลย ผมใช้ตัวนี้ทำงานทุกวันอยู่แล้ว เจออะไรก็แก้ไปพร้อมกันหละ

---

<a name="en"></a>

## English

APM is a portable XAMPP-style web stack for Windows x64. Every engine is **compiled from upstream source**, so a security release can be rebuilt and shipped the same day. You unzip it anywhere and run it. A small tray app (APM Manager) controls everything.

| Component | Release 1.0.0 | Notes |
|---|---|---|
| Apache httpd | 2.4.69 | OpenSSL 3.5 LTS, HTTP/2, brotli, mod_fcgid |
| PHP | 8.4.27 NTS x64 | latest 8.4 security release (2026-10-21), OPcache + JIT |
| MariaDB | 11.8.9 LTS | supported until 2030 |
| ImageMagick | 7.1.2-34 | for the imagick extension |
| APM Manager | 1.0.0 | Qt 6, light/dark |



### Why

XAMPP ships updates slowly. The machine this project started on still had PHP 8.2.12 (2023), Apache 2.4.58 and an end-of-life MariaDB 10.4. Each of these has security fixes that landed upstream long ago.

A real example is the **php2shell** pair, [CVE-2026-109618 / CVE-2026-109619](https://www.linuxthai.org/article/php2shell-cve-2026-109618-109619-patch-now): critical PHP remote code execution (CVSS 9.2) with a public exploit, fixed in PHP 8.5.12 / **8.4.27** / 8.3.36 / 8.2.35. APM 1.0.0 ships 8.4.27, built the day after the fix came out.

APM pins every version in one file, [versions.json](versions.json). Bump a version and run `build.cmd`, and you get a new zip. The extension and module set matches a real Linux production server, so code that works on APM works there.

### Use it (portable)

1. Download `APM-<version>-win64.zip` from [Releases](../../releases) and check the `.sha256`.
2. Unzip it anywhere, e.g. `C:\APM` or `D:\Tools\APM`. Paths with spaces or brackets work.
3. Run `manager\APMManager.exe`. On first run it:
   - picks free ports (80/443/3306, falling back to 8080/8443/3307),
   - writes the config from `templates\`,
   - creates a local HTTPS certificate,
   - creates an empty MariaDB database (`root`, no password, this PC only).
4. Click **Start all**, then **Open site**. Your sites go in `www\default\`.

Nothing has to be installed. The VC++ runtime ships inside the zip. Nothing is written to the registry unless you turn on a feature that needs it (sign-in start, services, PATH).

**Moving the folder:** move or copy it whenever you like. The Manager notices, re-creates the config with the same ports and keeps the database.

### APM Manager

| Feature | What it does |
|---|---|
| Control | Start, stop and restart Apache and MariaDB (Apache restarts gracefully). Open the error logs and config files. Shows versions, ports and the number of PHP workers. |
| Tray | Closing or minimising sends it to the tray. The icon colour shows the status. The right-click menu has Start/Stop/Open site. |
| Auto start | Open the Manager at sign-in, start the servers when it opens, or install **Windows services** that start at boot (one UAC prompt). |
| Two modes | Without services the servers run as normal processes, with no admin needed. Services run as the low-privilege `LocalService` account, and the signed-in user can start and stop them without elevation. |
| Ports and LAN | Change ports, or let other PCs on your subnet open the sites. The firewall rule is limited to `httpd.exe` and `LocalSubnet`. MariaDB stays on 127.0.0.1. |
| PATH for VS Code | Puts APM's `php` and `mysql` on PATH. It edits the registry value raw as `REG_EXPAND_SZ`, so `%SystemRoot%` entries are not broken. |
| HTTPS | Trusts the APM local CA, so `https://localhost` shows no warning. |
| Updates | **Check for updates** reads this repo's GitHub Releases, downloads the zip and verifies its sha256. It then stops the servers, replaces the binaries and starts them again. Config, databases, `www`, `ssl` and logs are never touched. Installing from a zip or folder (offline) and comparing the engines with upstream are also available. |

<p>
  <img src="docs/images/settings.png" width="420" alt="Settings">
  <img src="docs/images/update.png" width="420" alt="Updates">
</p>

#### Command line

```
manager\APMManager.exe --cli status
manager\APMManager.exe --cli start|stop|restart [apache|mariadb|all]
manager\APMManager.exe --setup [--http 80 --https 443 --db 3306] [--lan] [--reset]
manager\APMManager.exe --tray
```

`--cli` uses the same engine as the window. Exit codes: `0` ok, `1` error (port in use, did not start), `2` not set up for this folder (or the folder was moved; run `--setup`).

### Folder layout

```
APM\
  manager\     APMManager.exe + Qt runtime
  apache\      httpd 2.4 (conf\httpd.conf is written by setup)
  php\         PHP 8.4 NTS + ext\ + ImageMagick (php.ini is written by setup)
  mariadb\     MariaDB 11.8 (my.ini is written by setup)
  templates\   config templates (httpd.conf, apm-ssl.conf, php.ini, my.ini, index.php)
  tools\       mkcert.php (certificates via PHP's openssl extension)
  www\         sites (default\ is the DocumentRoot)
  data\        MariaDB datadir
  ssl\         apm-ca.crt, localhost.crt, cacert.pem (Mozilla CA bundle)
  logs\ tmp\
  apm.json     setup state (root, ports, lan)
  licenses\    third-party licenses
```

### PHP extensions

`bcmath calendar ctype curl date dom exif FFI fileinfo filter ftp gd(webp, avif, jpeg, png, freetype) gettext hash iconv igbinary imagick intl json libxml mbstring mysqli mysqlnd openssl pcre(JIT) PDO pdo_mysql pdo_sqlite Phar random redis(+igbinary, session) Reflection session shmop SimpleXML soap sockets sodium SPL sqlite3 standard tokenizer xml xmlreader xmlwriter xsl Zend OPcache zip zlib`

This is the same set as the Linux production server, minus `posix sysvmsg sysvsem sysvshm`, which do not exist on Windows.

### Security

- MariaDB listens on `127.0.0.1` only, and `root` has no password (local dev). Set a password before exposing it.
- `phpinfo()` on the start page and `/server-status` answer only this PC. `.env`, `.git` and `.ht*` are denied.
- TLS 1.2/1.3 only.
- Services run as `NT AUTHORITY\LocalService`, never LocalSystem.
- Every download is sha256-checked ([versions.json](versions.json), [versions.lock.json](versions.lock.json)).
- The repo is guarded against leaking secrets:
  - [scripts/check-leaks.ps1](scripts/check-leaks.ps1) runs as a pre-commit hook (`git config core.hooksPath .githooks`).
  - [.github/workflows/security.yml](.github/workflows/security.yml) runs it plus gitleaks on every push.
  - Private terms live in a gitignored `private/leak-terms.txt`.

### Articles (LinuxThai, Thai)

- [Build APM on Windows - series index](https://www.linuxthai.org/article/apm-build-windows-index): a 7-part walkthrough of compiling Apache 2.4.69, PHP 8.4.27 and MariaDB 11.8.9 from source on Windows 11, with time and disk estimates and the three-command shortcut.
- [php2shell: CVE-2026-109618 / 109619, patch now](https://www.linuxthai.org/article/php2shell-cve-2026-109618-109619-patch-now): the PHP RCE pair behind this release, with version checks and update steps for Debian, RHEL, hosting control panels, Docker and Windows.

### Build from source

The step-by-step guide covers tools, every command and its real output, and every problem hit so far with its fix: **[HOWTO-BUILD.en.md](HOWTO-BUILD.en.md)** (English) / **[HOWTO-BUILD.md](HOWTO-BUILD.md)** (ไทย).

```
build.cmd     :: fetch -> vcpkg libs -> Apache -> MariaDB -> ImageMagick -> PHP -> Manager -> dist\APM-<ver>-win64.zip
deploy.cmd    :: install stage\ into C:\APM on the build machine
```

CI: [.github/workflows/release.yml](.github/workflows/release.yml) is a manual run (Actions → release → Run workflow). It does three things:
1. build the whole stack on a Windows runner,
2. smoke-test the zip (setup, start, request a page through PHP + MariaDB, stop),
3. with a `release_tag`, attach the zip to that GitHub Release.

[security.yml](.github/workflows/security.yml) runs the leak scan on every push.

### Versioning

`<version> build <n> (<commit>, <date>)`, e.g. `1.0.0 build 3 (7a106aa, 2026-10-22)`. Here `build` is the git commit count, and `-dirty` marks a build made from uncommitted changes. You can see it in the Manager's About window, in the exe's Properties and in the zip's README.txt.

### Credit

Designed and built by **ChonLaWan**:
- [wanpean@gmail.com](mailto:wanpean@gmail.com)
- [wanpean.com](https://wanpean.com)
- [github.com/ChonLaWan/APM-Server](https://github.com/ChonLaWan/APM-Server)

Developed with Claude Code in VS Code.

### License

The scripts and APM Manager in this repo are **MIT + Commons Clause** ([LICENSE](LICENSE)):
- Free to use, modify and share, at home or at work.
- **Selling** it, or a paid product or service whose value comes mainly from it, needs a commercial license. Contact [wanpean@gmail.com](mailto:wanpean@gmail.com).

This makes the repo source-available rather than OSI open source. The bundled binaries keep their own licenses: Apache-2.0, PHP-3.01, GPL-2.0 (MariaDB), the ImageMagick License, and LGPL-3.0 (Qt, dynamically linked). All of them are in `licenses\` inside the zip. Sources for the GPL/LGPL parts are the upstream URLs in [versions.json](versions.json), rebuildable with the scripts in this repo.
