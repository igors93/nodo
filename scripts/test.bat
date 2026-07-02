@echo off
setlocal

REM Nodo test runner.
REM
REM Usage:
REM   scripts\test.bat                Build and run the full test suite.
REM   scripts\test.bat <module>       Build and run one module's tests, where
REM                                   <module> is a tests\ subdirectory used
REM                                   as the ctest name prefix: app, config,
REM                                   consensus, core, crypto, economics,
REM                                   mempool, node, p2p, serialization,
REM                                   staking, storage, utils.
REM   scripts\test.bat -R <regex>     Build and run tests whose ctest names
REM                                   match <regex>.

set "ROOT_DIR=%~dp0.."
set "CMAKE_BUILD_DIR=%ROOT_DIR%\build\cmake"

cmd /c "%ROOT_DIR%\scripts\cmake_build.bat"
if errorlevel 1 exit /b 1

if "%~1"=="" (
    ctest --test-dir "%CMAKE_BUILD_DIR%" --output-on-failure --no-tests=error
) else if "%~1"=="-R" (
    if "%~2"=="" (
        echo Usage: scripts\test.bat -R ^<regex^>
        exit /b 1
    )
    ctest --test-dir "%CMAKE_BUILD_DIR%" --output-on-failure --no-tests=error -R "%~2"
) else (
    ctest --test-dir "%CMAKE_BUILD_DIR%" --output-on-failure --no-tests=error -R "^%~1_"
)
if errorlevel 1 exit /b 1

echo.
echo Nodo tests completed successfully.

endlocal
