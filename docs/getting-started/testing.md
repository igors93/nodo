# Testing

Nodo uses CTest. `CMakeLists.txt` discovers every `tests/**/*.cpp` file and builds one executable per test source.

## Run All Tests

Windows:

```powershell
.\scripts\cmake_test_all.bat
```

Unix-like:

```bash
./scripts/cmake_test_all.sh
```

Direct CTest:

```bash
ctest --test-dir build/cmake --output-on-failure
```

## Module Test Scripts

The repository also includes focused scripts:

- `scripts/test_crypto.bat` / `scripts/test_crypto.sh`
- `scripts/test_consensus.bat` / `scripts/test_consensus.sh`
- `scripts/test_mempool.bat` / `scripts/test_mempool.sh`
- `scripts/test_serialization.bat` / `scripts/test_serialization.sh`
- `scripts/test_storage.bat` / `scripts/test_storage.sh`
- `scripts/test_economics.bat` / `scripts/test_economics.sh`

## Testing Rules

- Do not disable tests to make a change pass.
- Do not weaken protocol validation to satisfy a stale assertion.
- Update tests only when the old expected behavior is genuinely wrong.
- Keep security, storage, treasury, governance, and consensus tests deterministic.
- Prefer small regression tests that prove one invariant clearly.
