@echo off
setlocal enabledelayedexpansion

REM Nodo Windows build script.
REM This script is intended for Windows CMD or PowerShell.
REM It expects gcc.exe and g++.exe to be available in PATH.
REM If using MSYS2 UCRT64, make sure this path exists in Windows PATH:
REM C:\msys64\ucrt64\bin

set "ROOT_DIR=%~dp0.."
set "BUILD_DIR=%ROOT_DIR%\build"

set "CC=gcc"
set "CXX=g++"

where %CC% >nul 2>nul
if errorlevel 1 (
    echo Error: gcc was not found in PATH.
    echo.
    echo If you installed MSYS2 UCRT64, add this folder to PATH:
    echo C:\msys64\ucrt64\bin
    echo.
    exit /b 1
)

where %CXX% >nul 2>nul
if errorlevel 1 (
    echo Error: g++ was not found in PATH.
    echo.
    echo If you installed MSYS2 UCRT64, add this folder to PATH:
    echo C:\msys64\ucrt64\bin
    echo.
    exit /b 1
)

if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
)

echo Building Nodo C crypto module...

%CC% -std=c11 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    -c "%ROOT_DIR%\src\crypto\hash.c" ^
    -o "%BUILD_DIR%\hash.o"

if errorlevel 1 (
    echo Failed to build C crypto module.
    exit /b 1
)

echo Building Nodo C++ application...

%CXX% -std=c++20 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    "%ROOT_DIR%\apps\cli\main.cpp" ^
    "%ROOT_DIR%\src\app\DemoScenario.cpp" ^
    "%ROOT_DIR%\src\utils\Amount.cpp" ^
    "%ROOT_DIR%\src\utils\Time.cpp" ^
    "%ROOT_DIR%\src\economics\MintRecord.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivacyCommitment.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivacyNullifier.cpp" ^
    "%ROOT_DIR%\src\privacy\NullifierSet.cpp" ^
    "%ROOT_DIR%\src\core\Account.cpp" ^
    "%ROOT_DIR%\src\core\CoinLot.cpp" ^
    "%ROOT_DIR%\src\core\State.cpp" ^
    "%ROOT_DIR%\src\core\Transaction.cpp" ^
    "%ROOT_DIR%\src\core\LedgerRecord.cpp" ^
    "%ROOT_DIR%\src\core\Block.cpp" ^
    "%ROOT_DIR%\src\core\Blockchain.cpp" ^
    "%ROOT_DIR%\src\core\ChainStateRebuilder.cpp" ^
    "%ROOT_DIR%\src\staking\SecurityWeight.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoAlgorithm.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoPolicy.cpp" ^
    "%ROOT_DIR%\src\crypto\PublicKey.cpp" ^
    "%ROOT_DIR%\src\crypto\PrivateKey.cpp" ^
    "%ROOT_DIR%\src\crypto\Signature.cpp" ^
    "%ROOT_DIR%\src\crypto\SignatureBundle.cpp" ^
    "%BUILD_DIR%\hash.o" ^
    -o "%BUILD_DIR%\nodo.exe"

if errorlevel 1 (
    echo Failed to build Nodo application.
    exit /b 1
)

echo.
echo Build completed successfully.
echo Executable: %BUILD_DIR%\nodo.exe

endlocal