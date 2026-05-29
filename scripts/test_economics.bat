@echo off
setlocal enabledelayedexpansion

REM Nodo Windows protection economics test script.

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

echo Building Nodo C crypto module for protection economics tests...

%CC% -std=c11 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    -c "%ROOT_DIR%\src\crypto\hash.c" ^
    -o "%BUILD_DIR%\hash_economics_test.o"

if errorlevel 1 (
    echo Failed to build C crypto module for protection economics tests.
    exit /b 1
)

echo Building Nodo protection economics tests...

%CXX% -std=c++20 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    "%ROOT_DIR%\tests\economics\ProtectionEconomicsTests.cpp" ^
    "%ROOT_DIR%\src\utils\Amount.cpp" ^
    "%ROOT_DIR%\src\core\CoinLot.cpp" ^
    "%ROOT_DIR%\src\economics\ValidationWorkRecord.cpp" ^
    "%ROOT_DIR%\src\economics\ValidatorScoreRecord.cpp" ^
    "%ROOT_DIR%\src\economics\EpochEmissionPolicy.cpp" ^
    "%ROOT_DIR%\src\economics\ProtectionEpoch.cpp" ^
    "%ROOT_DIR%\src\economics\GenesisRewardRecord.cpp" ^
    "%BUILD_DIR%\hash_economics_test.o" ^
    -o "%BUILD_DIR%\protection_economics_tests.exe"

if errorlevel 1 (
    echo Failed to build Nodo protection economics tests.
    exit /b 1
)

echo Building Nodo protection ledger integration tests...

%CXX% -std=c++20 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    "%ROOT_DIR%\tests\economics\ProtectionLedgerIntegrationTests.cpp" ^
    "%ROOT_DIR%\src\utils\Amount.cpp" ^
    "%ROOT_DIR%\src\utils\Time.cpp" ^
    "%ROOT_DIR%\src\economics\MintRecord.cpp" ^
    "%ROOT_DIR%\src\serialization\MintRecordCodec.cpp" ^
    "%ROOT_DIR%\src\economics\ValidationWorkRecord.cpp" ^
    "%ROOT_DIR%\src\economics\ValidatorScoreRecord.cpp" ^
    "%ROOT_DIR%\src\economics\EpochEmissionPolicy.cpp" ^
    "%ROOT_DIR%\src\economics\ProtectionEpoch.cpp" ^
    "%ROOT_DIR%\src\economics\GenesisRewardRecord.cpp" ^
    "%ROOT_DIR%\src\serialization\FieldCodec.cpp" ^
    "%ROOT_DIR%\src\serialization\LedgerRecordCodec.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivacyCommitment.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivacyNullifier.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivateAccountingRecord.cpp" ^
    "%ROOT_DIR%\src\core\CoinLot.cpp" ^
    "%ROOT_DIR%\src\core\Account.cpp" ^
    "%ROOT_DIR%\src\core\Transaction.cpp" ^
    "%ROOT_DIR%\src\core\LedgerRecord.cpp" ^
    "%ROOT_DIR%\src\core\Block.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoAlgorithm.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoPolicy.cpp" ^
    "%ROOT_DIR%\src\crypto\PublicKey.cpp" ^
    "%ROOT_DIR%\src\crypto\PrivateKey.cpp" ^
    "%ROOT_DIR%\src\crypto\Signature.cpp" ^
    "%ROOT_DIR%\src\crypto\DevelopmentSignatureProvider.cpp" ^
    "%ROOT_DIR%\src\crypto\SignatureBundle.cpp" ^
    "%BUILD_DIR%\hash_economics_test.o" ^
    -o "%BUILD_DIR%\protection_ledger_integration_tests.exe"

if errorlevel 1 (
    echo Failed to build Nodo protection ledger integration tests.
    exit /b 1
)

echo Building Nodo protection state rebuilder tests...

%CXX% -std=c++20 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    "%ROOT_DIR%\tests\economics\ProtectionStateRebuilderTests.cpp" ^
    "%ROOT_DIR%\src\utils\Amount.cpp" ^
    "%ROOT_DIR%\src\utils\Time.cpp" ^
    "%ROOT_DIR%\src\economics\MintRecord.cpp" ^
    "%ROOT_DIR%\src\serialization\MintRecordCodec.cpp" ^
    "%ROOT_DIR%\src\economics\ValidationWorkRecord.cpp" ^
    "%ROOT_DIR%\src\economics\ValidatorScoreRecord.cpp" ^
    "%ROOT_DIR%\src\economics\EpochEmissionPolicy.cpp" ^
    "%ROOT_DIR%\src\economics\ProtectionEpoch.cpp" ^
    "%ROOT_DIR%\src\economics\GenesisRewardRecord.cpp" ^
    "%ROOT_DIR%\src\economics\ProtectionEconomicsState.cpp" ^
    "%ROOT_DIR%\src\economics\ProtectionEconomicsRebuilder.cpp" ^
    "%ROOT_DIR%\src\serialization\FieldCodec.cpp" ^
    "%ROOT_DIR%\src\serialization\LedgerRecordCodec.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivacyCommitment.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivacyNullifier.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivateAccountingRecord.cpp" ^
    "%ROOT_DIR%\src\core\CoinLot.cpp" ^
    "%ROOT_DIR%\src\core\Account.cpp" ^
    "%ROOT_DIR%\src\core\Transaction.cpp" ^
    "%ROOT_DIR%\src\core\LedgerRecord.cpp" ^
    "%ROOT_DIR%\src\core\Block.cpp" ^
    "%ROOT_DIR%\src\core\Blockchain.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoAlgorithm.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoPolicy.cpp" ^
    "%ROOT_DIR%\src\crypto\PublicKey.cpp" ^
    "%ROOT_DIR%\src\crypto\PrivateKey.cpp" ^
    "%ROOT_DIR%\src\crypto\Signature.cpp" ^
    "%ROOT_DIR%\src\crypto\DevelopmentSignatureProvider.cpp" ^
    "%ROOT_DIR%\src\crypto\SignatureBundle.cpp" ^
    "%BUILD_DIR%\hash_economics_test.o" ^
    -o "%BUILD_DIR%\protection_state_rebuilder_tests.exe"

if errorlevel 1 (
    echo Failed to build Nodo protection state rebuilder tests.
    exit /b 1
)

echo Building Nodo coin lot registry tests...

%CXX% -std=c++20 -Wall -Wextra -I"%ROOT_DIR%\include" ^
    "%ROOT_DIR%\tests\core\CoinLotRegistryTests.cpp" ^
    "%ROOT_DIR%\src\utils\Amount.cpp" ^
    "%ROOT_DIR%\src\utils\Time.cpp" ^
    "%ROOT_DIR%\src\economics\MintRecord.cpp" ^
    "%ROOT_DIR%\src\serialization\MintRecordCodec.cpp" ^
    "%ROOT_DIR%\src\economics\ValidationWorkRecord.cpp" ^
    "%ROOT_DIR%\src\economics\ValidatorScoreRecord.cpp" ^
    "%ROOT_DIR%\src\economics\EpochEmissionPolicy.cpp" ^
    "%ROOT_DIR%\src\economics\ProtectionEpoch.cpp" ^
    "%ROOT_DIR%\src\economics\GenesisRewardRecord.cpp" ^
    "%ROOT_DIR%\src\economics\ProtectionEconomicsState.cpp" ^
    "%ROOT_DIR%\src\economics\ProtectionEconomicsRebuilder.cpp" ^
    "%ROOT_DIR%\src\serialization\FieldCodec.cpp" ^
    "%ROOT_DIR%\src\serialization\LedgerRecordCodec.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivacyCommitment.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivacyNullifier.cpp" ^
    "%ROOT_DIR%\src\privacy\PrivateAccountingRecord.cpp" ^
    "%ROOT_DIR%\src\core\CoinLot.cpp" ^
    "%ROOT_DIR%\src\core\CoinLotVerificationResult.cpp" ^
    "%ROOT_DIR%\src\core\CoinLotRegistry.cpp" ^
    "%ROOT_DIR%\src\core\CoinLotRegistryRebuilder.cpp" ^
    "%ROOT_DIR%\src\core\Account.cpp" ^
    "%ROOT_DIR%\src\core\Transaction.cpp" ^
    "%ROOT_DIR%\src\core\LedgerRecord.cpp" ^
    "%ROOT_DIR%\src\core\Block.cpp" ^
    "%ROOT_DIR%\src\core\Blockchain.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoAlgorithm.cpp" ^
    "%ROOT_DIR%\src\crypto\CryptoPolicy.cpp" ^
    "%ROOT_DIR%\src\crypto\PublicKey.cpp" ^
    "%ROOT_DIR%\src\crypto\PrivateKey.cpp" ^
    "%ROOT_DIR%\src\crypto\Signature.cpp" ^
    "%ROOT_DIR%\src\crypto\DevelopmentSignatureProvider.cpp" ^
    "%ROOT_DIR%\src\crypto\SignatureBundle.cpp" ^
    "%BUILD_DIR%\hash_economics_test.o" ^
    -o "%BUILD_DIR%\coin_lot_registry_tests.exe"

if errorlevel 1 (
    echo Failed to build Nodo coin lot registry tests.
    exit /b 1
)

echo.
echo Running Nodo protection economics tests...
"%BUILD_DIR%\protection_economics_tests.exe"

if errorlevel 1 (
    echo Protection economics tests failed.
    exit /b 1
)

echo.
echo Running Nodo protection ledger integration tests...
"%BUILD_DIR%\protection_ledger_integration_tests.exe"

if errorlevel 1 (
    echo Protection ledger integration tests failed.
    exit /b 1
)

echo.
echo Running Nodo protection state rebuilder tests...
"%BUILD_DIR%\protection_state_rebuilder_tests.exe"

if errorlevel 1 (
    echo Protection state rebuilder tests failed.
    exit /b 1
)

echo.
echo Running Nodo coin lot registry tests...
"%BUILD_DIR%\coin_lot_registry_tests.exe"

if errorlevel 1 (
    echo Coin lot registry tests failed.
    exit /b 1
)

echo.
echo Protection economics tests completed successfully.

endlocal
