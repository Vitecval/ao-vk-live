@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-preview24-r50.ps1" -Revert
pause
