# Testing

Nodo has a broad C++ test suite and Python diagnostic scenarios. By default,
the C++ tests share one runner binary per module while retaining individual
CTest names. Set `-DNODO_TESTS_GROUPED=OFF` for a dedicated binary per test
when isolating sanitizer or coverage runs.

## Run all C++ tests

Unix-like systems:

```bash
./scripts/cmake_test_all.sh
```

Windows:

```powershell
.\scripts\cmake_test_all.bat
```

## Direct CTest

```bash
ctest --test-dir build/cmake --output-on-failure
```

## Filtered test runs

```bash
ctest --test-dir build/cmake -R consensus --output-on-failure
ctest --test-dir build/cmake -R storage --output-on-failure
ctest --test-dir build/cmake -R governance --output-on-failure
```

## Diagnostic scenarios

Python diagnostic tooling lives under `diagnostics/python`.

Typical use:

```bash
python diagnostics/python/run_scenarios.py
```

## Testing rules

Tests should prove protocol safety, not only implementation convenience. Important test categories include:

- deterministic serialization;
- state-transition rejection paths;
- block finalization and quorum certificates;
- storage/reload failure cases;
- governance lifecycle audit;
- treasury policy and execution evidence;
- validator penalties and slashing evidence;
- networking and sync hardening;
- key safety gates;
- readiness diagnostics.
