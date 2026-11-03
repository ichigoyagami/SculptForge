@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Test-Installation.ps1"
set "SCULPTFORGE_TEST_RESULT=%ERRORLEVEL%"
echo.
pause
exit /b %SCULPTFORGE_TEST_RESULT%
