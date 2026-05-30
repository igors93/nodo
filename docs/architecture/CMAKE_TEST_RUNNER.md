# Nodo CMake Test Runner

Status: Cycle 2 Implementation  
Version: NODO-CMAKE-TEST-RUNNER-V1

## Purpose

This phase completes the move away from fragile manual test linker commands.

The legacy scripts now delegate to CMake/CTest.

## Main Commands

Linux/Fedora:

```bash
chmod +x scripts/*.sh
./scripts/build.sh
./scripts/test_all.sh
```

Windows/MSYS2:

```powershell
cmd /c scripts\build.bat
cmd /c scripts\test_all.bat
```

## Focused Test Commands

Protection/economics/core protection layer:

```bash
./scripts/test_economics.sh
```

Storage layer:

```bash
./scripts/test_storage.sh
```

## Why This Matters

Previous scripts manually listed many `.cpp` files per test.

That caused linker failures such as:

```text
undefined reference
```

CMake now builds a reusable `nodo_core` static library and links tests against it.

## Current Labels

CTest labels are assigned from test folder paths:

```text
tests/core      -> core;protection
tests/economics -> economics;protection
tests/storage   -> storage
tests/crypto     -> crypto
```
