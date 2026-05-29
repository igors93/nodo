@echo off
setlocal enabledelayedexpansion

REM Nodo Windows crypto test script.
REM This script builds and runs cryptographic hash provider tests.

set "ROOT_DIR=%~dp0.."
set "BUILD_DIR=%ROOT_DIR%\build\tests"

set "CC=gcc"
set "CXX=g++"

where %CC% >nul 2>nul
if errorlevel 1 (
    echo Error: gcc was not found in PATH.
    echo If using MSYS2 UCRT64, add this folder to PATH:
    echo C:\msys64\ucrt64\bin
    exit /b 1
)

where %CXX% >nul 2>nul
if errorlevel 1 (
    echo Error: g++ was not found in PATH.
    echo If using MSYS2 UCRT64, add this folder to PATH:
    echo C:\msys64\ucrt64\bin
    exit /b 1
)

if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
)

echo Building Nodo C crypto module for crypto tests...

%CC% -std=c11 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    -c "%ROOT_DIR%\src\crypto\hash.c" ^
    -o "%BUILD_DIR%\hash_crypto_test.o"

if errorlevel 1 (
    echo Failed to build C crypto module for crypto tests.
    exit /b 1
)

echo Building Nodo crypto hash tests...

%CXX% -std=c++20 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    "%ROOT_DIR%\tests\crypto\HashTests.cpp" ^
    "%BUILD_DIR%\hash_crypto_test.o" ^
    -o "%BUILD_DIR%\crypto_hash_tests.exe"

if errorlevel 1 (
    echo Failed to build Nodo crypto hash tests.
    exit /b 1
)

echo.
echo Running Nodo crypto hash tests...
"%BUILD_DIR%\crypto_hash_tests.exe"

if errorlevel 1 (
    echo Crypto hash tests failed.
    exit /b 1
)

echo.
echo Crypto hash tests completed successfully.

endlocal
