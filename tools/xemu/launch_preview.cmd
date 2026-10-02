@echo off
setlocal
cd /d "%~dp0..\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0launch_preview.ps1" %*
exit /b %ERRORLEVEL%
