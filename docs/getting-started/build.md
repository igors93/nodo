# Build

Nodo uses CMake and C++20.

## Prerequisites

- CMake 3.20+
- C++20 compiler
- OpenSSL/libcrypto
- BLST

## Install BLST on Unix-like systems

```bash
./scripts/install_blst.sh
export BLST_ROOT="$HOME/.nodo/deps/blst"
```

The build scripts expect `BLST_ROOT` to point to the BLST installation.

## Unix-like build

```bash
./scripts/cmake_build.sh
```

## Windows build

```powershell
$env:BLST_ROOT="$env:USERPROFILE\.nodo\deps\blst"
.\scripts\cmake_build.bat
```

## Direct CMake build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Sanitized build

```bash
./scripts/cmake_build_sanitized.sh
```

## Clean build

Unix-like systems:

```bash
./scripts/clean.sh
./scripts/cmake_build.sh
```

Windows:

```powershell
.\scripts\clean.bat
.\scripts\cmake_build.bat
```

## Troubleshooting

### BLST not found

Confirm that `BLST_ROOT` is set and points to the directory containing BLST include/library artifacts.

### OpenSSL not found

Install the OpenSSL development package for your platform and rerun CMake.

### Old CMake cache

Remove the build directory and rebuild:

```bash
rm -rf build
./scripts/cmake_build.sh
```
