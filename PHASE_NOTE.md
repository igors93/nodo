# Cycle 1 first implementation

This phase starts Cycle 1 with two parallel foundations.

## Front A — validator identity

New components:

```text
ValidatorRegistrationRecord
ValidatorRegistryEntry
ValidatorRegistryUpdateResult
ValidatorRegistry
```

This binds a validator address to a public key and rejects invalid address/key pairs.

## Front B — modular build foundation

New components:

```text
CMakeLists.txt
scripts/cmake_build.sh
scripts/cmake_test_all.sh
scripts/cmake_build.bat
scripts/cmake_test_all.bat
```

This creates a `nodo_core` static library and lets tests link against the library instead of manually listing every `.cpp` file.

Recommended commits:

```bash
git commit -m "Add CMake modular build foundation"
git commit -m "Add validator registry identity binding"
```

If committed together:

```bash
git commit -m "Add validator registry and modular build foundation"
```
