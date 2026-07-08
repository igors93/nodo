# Quick Start

This guide builds Nodo and runs the local development path.

## 1. Install dependencies

Required tools:

- CMake 3.20 or newer;
- a C++20 compiler;
- OpenSSL/libcrypto;
- BLST;
- Bash, PowerShell, or a compatible shell.

On Unix-like systems, BLST can be installed through the helper script:

```bash
./scripts/install_blst.sh
export BLST_ROOT="$HOME/.nodo/deps/blst"
```

## 2. Build

Unix-like systems:

```bash
./scripts/cmake_build.sh
```

Windows PowerShell:

```powershell
$env:BLST_ROOT="$env:USERPROFILE\.nodo\deps\blst"
.\scripts\cmake_build.bat
```

## 3. Run tests

Unix-like systems:

```bash
./scripts/cmake_test_all.sh
```

Windows PowerShell:

```powershell
.\scripts\cmake_test_all.bat
```

## 4. Inspect the CLI

```bash
./build/nodo help
```

On Windows, the executable may be available as:

```powershell
.\build\nodo.exe help
```

## 5. Initialize a local node

```bash
./build/nodo init --network localnet --data-dir .nodo --peer-id local-node --endpoint 127.0.0.1:9000
./build/nodo keys create --data-dir .nodo --type both
./build/nodo status --data-dir .nodo
```

## 6. Produce and audit local blocks

```bash
./build/nodo tx submit --data-dir .nodo
./build/nodo block produce --data-dir .nodo
./build/nodo node reload --data-dir .nodo
./build/nodo chain audit --data-dir .nodo
```

## Next reading

- [Build](build.md)
- [CLI](cli.md)
- [Local testnet](../operations/local-testnet.md)
- [Architecture overview](../architecture/architecture-overview.md)
