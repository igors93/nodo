@echo off
setlocal

set "ROOT_DIR=%~dp0.."
cmd /c "%ROOT_DIR%\scripts\cmake_build.bat"
if errorlevel 1 exit /b 1

endlocal
