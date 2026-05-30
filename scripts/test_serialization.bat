@echo off
setlocal

set "ROOT_DIR=%~dp0.."
set "CMAKE_BUILD_DIR=%ROOT_DIR%\build\cmake"

where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: cmake was not found in PATH.
    echo MSYS2 UCRT64: pacman -S mingw-w64-ucrt-x86_64-cmake
    exit /b 1
)

cmd /c "%ROOT_DIR%\scripts\cmake_build.bat"
if errorlevel 1 exit /b 1

ctest --test-dir "%CMAKE_BUILD_DIR%" --output-on-failure -L serialization
if errorlevel 1 exit /b 1

echo.
echo Serialization CMake tests completed successfully.

endlocal
