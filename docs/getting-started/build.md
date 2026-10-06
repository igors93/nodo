# Build

Nodo uses CMake and C++20.

## Prerequisites

- CMake 3.20+
- GCC or Clang with C++20 and `unsigned __int128` support (including MinGW and AppleClang); MSVC is not currently supported
- OpenSSL/libcrypto
- BLST
- Network access on the first configure unless pinned source trees are provided locally

## Install BLST on Unix-like systems

```bash
./scripts/install_blst.sh
export BLST_ROOT="$HOME/.nodo/deps/blst"
```

The build scripts expect `BLST_ROOT` to point to the BLST installation.
The installer checks out the pinned blst v0.3.11 commit. To install without
network access, set `BLST_SOURCE_DIR` to a checkout at that commit.

For an offline CMake configure, provide the pinned Asio and nlohmann/json
source trees with `-DNODO_ASIO_SOURCE_DIR=...` and
`-DNODO_JSON_SOURCE_DIR=...`, and set
`-DFETCHCONTENT_FULLY_DISCONNECTED=ON`. CMake checks the blst v0.3.11
headers at configure time; the library must come from that release too.

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
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build/cmake
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
rm -rf build/cmake
./scripts/cmake_build.sh
```
