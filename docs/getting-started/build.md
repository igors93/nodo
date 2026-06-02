# Build

Nodo uses CMake and requires C++20, OpenSSL libcrypto, and external blst.

## Prerequisites

- CMake 3.20 or newer.
- A C++20 compiler.
- OpenSSL libcrypto development files.
- `blst.h` and `libblst` installed outside the Nodo repository.

The CMake dependency file searches:

- `-DBLST_ROOT=/path/to/blst`;
- the `BLST_ROOT` environment variable;
- `$HOME/.nodo/deps/blst`;
- `$HOME/.local/nodo/deps/blst`;
- system include/library paths;
- `pkg-config`, when available.

## Install blst on Unix-like Systems

The repository includes a helper:

```bash
./scripts/install_blst.sh
export BLST_ROOT="$HOME/.nodo/deps/blst"
```

On Windows/MSYS2, install or build blst outside the repository and point `BLST_ROOT` at that installation.

## Windows Build

```powershell
$env:BLST_ROOT="$env:USERPROFILE\.nodo\deps\blst"
.\scripts\cmake_build.bat
```

The executable is written to:

```text
build\nodo.exe
```

## Unix-like Build

```bash
export BLST_ROOT="$HOME/.nodo/deps/blst"
./scripts/cmake_build.sh
```

The executable is written to:

```text
build/nodo
```

## Direct CMake

```bash
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Debug -DBLST_ROOT="$BLST_ROOT"
cmake --build build/cmake --parallel
```

## Clean Build

Windows:

```powershell
.\scripts\clean.bat
.\scripts\cmake_build.bat
```

Unix-like:

```bash
./scripts/clean.sh
./scripts/cmake_build.sh
```

## Troubleshooting

| Symptom | Check |
| --- | --- |
| CMake cannot find blst. | Set `BLST_ROOT` or pass `-DBLST_ROOT=/path/to/blst`. |
| CMake cannot find OpenSSL. | Install OpenSSL development files or ensure `pkg-config` can find `libcrypto`. |
| Windows executable cannot find runtime DLLs. | Use the provided CMake build scripts; CMake copies common MinGW runtime DLLs for built targets. |
| Build scripts cannot find CMake. | Add CMake to `PATH` or install it through your toolchain package manager. |
