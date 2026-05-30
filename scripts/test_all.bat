@echo off
setlocal

set "ROOT_DIR=%~dp0.."

echo Nodo unified CMake/CTest runner
echo ------------------------------

cmd /c "%ROOT_DIR%\scripts\cmake_test_all.bat"
if errorlevel 1 exit /b 1

endlocal
