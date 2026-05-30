@echo off
setlocal

set "ROOT_DIR=%~dp0.."
set "CMAKE_BUILD_DIR=%ROOT_DIR%\build\cmake"

where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: cmake was not found in PATH.
    exit /b 1
)

cmake -S "%ROOT_DIR%" -B "%CMAKE_BUILD_DIR%" -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 exit /b 1

cmake --build "%CMAKE_BUILD_DIR%" --parallel
if errorlevel 1 exit /b 1

ctest --test-dir "%CMAKE_BUILD_DIR%" --output-on-failure -L storage
if errorlevel 1 exit /b 1

echo.
echo Storage CMake tests completed successfully.

endlocal
