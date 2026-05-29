@echo off
setlocal enabledelayedexpansion

REM Nodo Windows crypto test script.
REM This script builds and runs hash, signature provider, address, and key management tests.

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

echo Building Nodo signature provider tests...

%CXX% -std=c++20 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    "%ROOT_DIR%\tests\crypto\SignatureProviderTests.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoAlgorithm.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoPolicy.cpp" ^
    "%ROOT_DIR%\src\crypto\PublicKey.cpp" ^
    "%ROOT_DIR%\src\crypto\PrivateKey.cpp" ^
    "%ROOT_DIR%\src\crypto\Signature.cpp" ^
    "%ROOT_DIR%\src\crypto\DevelopmentSignatureProvider.cpp" ^
    "%ROOT_DIR%\src\crypto\SignatureBundle.cpp" ^
    "%BUILD_DIR%\hash_crypto_test.o" ^
    -o "%BUILD_DIR%\signature_provider_tests.exe"

if errorlevel 1 (
    echo Failed to build Nodo signature provider tests.
    exit /b 1
)

echo Building Nodo address derivation tests...

%CXX% -std=c++20 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    "%ROOT_DIR%\tests\crypto\AddressDerivationTests.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoAlgorithm.cpp" ^
    "%ROOT_DIR%\src\crypto\PublicKey.cpp" ^
    "%ROOT_DIR%\src\crypto\Address.cpp" ^
    "%ROOT_DIR%\src\crypto\AddressDerivation.cpp" ^
    "%BUILD_DIR%\hash_crypto_test.o" ^
    -o "%BUILD_DIR%\address_derivation_tests.exe"

if errorlevel 1 (
    echo Failed to build Nodo address derivation tests.
    exit /b 1
)

echo Building Nodo key management tests...

%CXX% -std=c++20 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    "%ROOT_DIR%\tests\crypto\KeyPairTests.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoAlgorithm.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoPolicy.cpp" ^
    "%ROOT_DIR%\src\crypto\PublicKey.cpp" ^
    "%ROOT_DIR%\src\crypto\PrivateKey.cpp" ^
    "%ROOT_DIR%\src\crypto\Signature.cpp" ^
    "%ROOT_DIR%\src\crypto\DevelopmentSignatureProvider.cpp" ^
    "%ROOT_DIR%\src\crypto\SignatureBundle.cpp" ^
    "%ROOT_DIR%\src\crypto\Address.cpp" ^
    "%ROOT_DIR%\src\crypto\AddressDerivation.cpp" ^
    "%ROOT_DIR%\src\crypto\KeyPair.cpp" ^
    "%BUILD_DIR%\hash_crypto_test.o" ^
    -o "%BUILD_DIR%\key_pair_tests.exe"

if errorlevel 1 (
    echo Failed to build Nodo key management tests.
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
echo Running Nodo signature provider tests...
"%BUILD_DIR%\signature_provider_tests.exe"

if errorlevel 1 (
    echo Signature provider tests failed.
    exit /b 1
)

echo.
echo Running Nodo address derivation tests...
"%BUILD_DIR%\address_derivation_tests.exe"

if errorlevel 1 (
    echo Address derivation tests failed.
    exit /b 1
)

echo.
echo Running Nodo key management tests...
"%BUILD_DIR%\key_pair_tests.exe"

if errorlevel 1 (
    echo Key management tests failed.
    exit /b 1
)

echo.
echo Crypto tests completed successfully.

endlocal
