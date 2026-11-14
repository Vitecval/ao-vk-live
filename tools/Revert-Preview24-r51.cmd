@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-preview24-r51.ps1" -Revert
pause
