@echo off
set "AO_VK_POWERSHELL=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" set "AO_VK_POWERSHELL=%SystemRoot%\SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
"%AO_VK_POWERSHELL%" -NoLogo -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0setup-live.ps1" -Action "Reset settings"
set "result=%errorlevel%"
if not "%result%"=="0" pause
exit /b %result%
