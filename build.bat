@echo off
setlocal
cd /d "%~dp0"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build\build.ps1" %*
set "EXIT_CODE=%ERRORLEVEL%"

if "%~1"=="" pause
exit /b %EXIT_CODE%
