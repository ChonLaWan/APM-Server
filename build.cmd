@echo off
rem build.cmd - build the whole APM stack (Apache, PHP 8.4, MariaDB, ImageMagick, libs) into stage\
rem   build.cmd                 full build
rem   build.cmd -From php       resume from a step: fetch vcpkg apache mariadb imagemagick php
setlocal
where pwsh >nul 2>&1 || (echo [ERR] PowerShell 7 ^(pwsh^) not found - see HOWTO-BUILD.md section 2.2 & exit /b 1)
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-all.ps1" %*
exit /b %ERRORLEVEL%
