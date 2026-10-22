@echo off
rem deploy.cmd - install stage\ into C:\APM (does not start anything)
rem   deploy.cmd                                       defaults: C:\APM, http 8080, https 8443, db 3307
rem   deploy.cmd -Target D:\APM -HttpPort 80 -HttpsPort 443 -DbPort 3306
rem   deploy.cmd -ResetConfig                          re-render config files from templates\
setlocal
where pwsh >nul 2>&1 || (echo [ERR] PowerShell 7 ^(pwsh^) not found - see HOWTO-BUILD.md section 2.2 & exit /b 1)
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\90-deploy.ps1" %*
exit /b %ERRORLEVEL%
