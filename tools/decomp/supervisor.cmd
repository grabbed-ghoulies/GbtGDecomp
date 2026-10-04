@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0supervisor.ps1" -Mode Loop %*
pause
