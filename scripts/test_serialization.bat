@echo off
setlocal enabledelayedexpansion

REM Nodo Windows serialization test script.
REM This script builds and runs serialization round-trip tests.

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

echo Building Nodo C crypto module for tests...

%CC% -std=c11 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    -c "%ROOT_DIR%\src\crypto\hash.c" ^
    -o "%BUILD_DIR%\hash_test.o"

if errorlevel 1 (
    echo Failed to build C crypto module for tests.
    exit /b 1
)

echo Building Nodo serialization round-trip tests...

%CXX% -std=c++20 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    "%ROOT_DIR%\tests\serialization\SerializationRoundTripTests.cpp" ^
    "%ROOT_DIR%\src\utils\Amount.cpp" ^
    "%ROOT_DIR%\src\economics\MintRecord.cpp" ^
    "%ROOT_DIR%\src\serialization\FieldCodec.cpp" ^
    "%ROOT_DIR%\src\serialization\MintRecordCodec.cpp" ^
    "%ROOT_DIR%\src\serialization\PrivacyCommitmentCodec.cpp" ^
    "%ROOT_DIR%\src\serialization\PrivacyNullifierCodec.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivacyCommitment.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivacyNullifier.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivateAccountingRecord.cpp" ^
    "%BUILD_DIR%\hash_test.o" ^
    -o "%BUILD_DIR%\serialization_roundtrip_tests.exe"

if errorlevel 1 (
    echo Failed to build Nodo serialization round-trip tests.
    exit /b 1
)

echo.
echo Running Nodo serialization round-trip tests...
"%BUILD_DIR%\serialization_roundtrip_tests.exe"

if errorlevel 1 (
    echo Serialization round-trip tests failed.
    exit /b 1
)

echo.
echo Serialization round-trip tests completed successfully.

endlocal
