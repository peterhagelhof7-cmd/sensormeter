@echo off
rem Doppelklick-Wrapper fuer flash-sensormeter.ps1 - umgeht die Execution-
rem Policy-Sperre fuer unsignierte .ps1-Dateien nur fuer diesen einen Aufruf.
setlocal
set SCRIPT_DIR=%~dp0
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%flash-sensormeter.ps1" %*
echo.
pause
