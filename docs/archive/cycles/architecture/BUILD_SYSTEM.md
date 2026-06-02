> Archived document.
> This file is kept for historical context only and may not describe the current Nodo implementation.

# Nodo Modular Build Foundation

Status: Cycle 1 Foundation  
Version: NODO-BUILD-SYSTEM-V1

## Purpose

Nodo previously compiled many tests by manually listing `.cpp` files in shell and batch scripts.

That caused fragile linker failures whenever one new component depended on another component.

This phase adds a CMake foundation:

```text
nodo_core static library
nodo executable
CTest test discovery
```

## Why This Matters

Instead of every test manually linking dozens of files, tests can link against:

```text
nodo_core
```

That makes future implementation faster and safer.

## New Commands

Linux/Fedora:

```bash
chmod +x scripts/*.sh
./scripts/cmake_build.sh
./scripts/cmake_test_all.sh
```

Windows/MSYS2:

```powershell
cmd /c scripts\cmake_build.bat
cmd /c scripts\cmake_test_all.bat
```

## Current Design

```text
src/**/*.cpp + src/crypto/hash.c -> nodo_core
apps/cli/main.cpp               -> nodo executable
tests/**/*.cpp                  -> CTest test executables
```

## Future Direction

The next build improvements should split `nodo_core` into smaller libraries:

```text
nodo_crypto
nodo_serialization
nodo_core
nodo_economics
nodo_storage
nodo_privacy
```

The current foundation intentionally starts with one static library to reduce risk and stop linker errors first.
