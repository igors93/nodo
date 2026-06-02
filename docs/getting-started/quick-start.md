# Quick Start

This guide builds Nodo and runs a small localnet flow. Nodo is pre-mainnet software; use this only for development and review.

## Windows PowerShell

```powershell
$env:BLST_ROOT="$env:USERPROFILE\.nodo\deps\blst"
.\scripts\cmake_build.bat
.\scripts\cmake_test_all.bat
.\build\nodo.exe help
```

Run a localnet flow:

```powershell
.\build\nodo.exe init --network localnet --data-dir .nodo
.\build\nodo.exe keys create --network localnet --data-dir .nodo
.\build\nodo.exe tx submit --data-dir .nodo
.\build\nodo.exe block produce --data-dir .nodo
.\build\nodo.exe node reload --network localnet --data-dir .nodo
.\build\nodo.exe chain audit --data-dir .nodo
.\build\nodo.exe diagnostics --network localnet --data-dir .nodo
```

## Linux, macOS, Git Bash, or MSYS2

```bash
export BLST_ROOT="$HOME/.nodo/deps/blst"
./scripts/cmake_build.sh
./scripts/cmake_test_all.sh
./build/nodo help
```

Run a localnet flow:

```bash
./build/nodo init --network localnet --data-dir .nodo
./build/nodo keys create --network localnet --data-dir .nodo
./build/nodo tx submit --data-dir .nodo
./build/nodo block produce --data-dir .nodo
./build/nodo node reload --network localnet --data-dir .nodo
./build/nodo chain audit --data-dir .nodo
./build/nodo diagnostics --network localnet --data-dir .nodo
```

## Next Reading

- [Build](build.md)
- [Testing](testing.md)
- [CLI](cli.md)
- [Storage and Reload](../architecture/storage-and-reload.md)
