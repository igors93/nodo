@echo off
setlocal

REM Nodo Windows clean script.
REM This removes the local build directory.

set "ROOT_DIR=%~dp0.."
set "BUILD_DIR=%ROOT_DIR%\build"

if exist "%BUILD_DIR%" (
    rmdir /s /q "%BUILD_DIR%"
    echo Build directory removed.
) else (
    echo Build directory does not exist. Nothing to clean.
)

endlocal