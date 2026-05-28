@echo off
setlocal enabledelayedexpansion

REM Nodo unified test runner for Windows.
REM This script runs all available project-level tests in a deterministic order.

set "ROOT_DIR=%~dp0.."

echo Nodo unified test runner
echo ------------------------
echo.

if not exist "%ROOT_DIR%\scripts\test_serialization.bat" (
    echo Error: required test script was not found:
    echo %ROOT_DIR%\scripts\test_serialization.bat
    exit /b 1
)

if not exist "%ROOT_DIR%\scripts\test_storage.bat" (
    echo Error: required test script was not found:
    echo %ROOT_DIR%\scripts\test_storage.bat
    exit /b 1
)

echo Running serialization tests...
call "%ROOT_DIR%\scripts\test_serialization.bat"

if errorlevel 1 (
    echo Serialization tests failed.
    exit /b 1
)

echo.
echo Running blockchain storage integration tests...
call "%ROOT_DIR%\scripts\test_storage.bat"

if errorlevel 1 (
    echo Blockchain storage integration tests failed.
    exit /b 1
)

echo.
echo All Nodo tests completed successfully.

endlocal
