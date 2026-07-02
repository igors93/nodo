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

## Filtered Test Runs

`scripts/test.sh` (Unix-like) and `scripts/test.bat` (Windows) build the
project and run a subset of the suite:

```bash
./scripts/test.sh                  # all tests
./scripts/test.sh consensus        # one module (tests/ subdirectory prefix)
./scripts/test.sh -R "p2p_Encrypted"   # any ctest name regex
```

Valid module names are the `tests/` subdirectories: `app`, `config`,
`consensus`, `core`, `crypto`, `economics`, `mempool`, `node`, `p2p`,
`serialization`, `staking`, `storage`, `utils`.

Test executables are named `<subdir>_<FileName>` (for example
`consensus_VotePoolTests`), so any grouping can be selected with `-R`.

## Testing Rules

- Do not disable tests to make a change pass.
- Do not weaken protocol validation to satisfy a stale assertion.
- Update tests only when the old expected behavior is genuinely wrong.
- Keep security, storage, treasury, governance, and consensus tests deterministic.
- Prefer small regression tests that prove one invariant clearly.
