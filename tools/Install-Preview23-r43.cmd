@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-preview23-r43.ps1"
set "result=%errorlevel%"
if not "%result%"=="0" pause
exit /b %result%
