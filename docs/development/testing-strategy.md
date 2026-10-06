# Testing Strategy

CTest keeps a stable name for each source-level test. The default CMake build
links tests into one runner per module to reduce binary and link overhead;
`-DNODO_TESTS_GROUPED=OFF` restores separate executables for focused sanitizer
and coverage jobs. Shared integration fixtures and the `require` assertion
helper live in `tests/common`. Existing source-level `main` functions become
named entries in the module runner at compile time. Assertions remain active
in Release test binaries.

CI builds with warnings treated as errors and checks Linux GCC/Clang, Windows
MinGW, and macOS. Linux also runs ASan/UBSan, TSan concurrency tests, a
coverage-report subset, static analysis, and a short network-codec libFuzzer
smoke run. `scripts/check_markdown_links.py` checks local documentation links.
The cross-platform authenticated real-TCP handshake test runs on Windows too;
the longer process-isolated multi-validator scenarios use POSIX `fork` and run
on Linux and macOS.

Nodo tests should prove that the protocol rejects unsafe behavior and rebuilds valid behavior deterministically.

## Required test categories

- canonical serialization round trips;
- invalid serialization rejection;
- transaction admission;
- state-transition execution;
- state-root mismatch rejection;
- finalized block replay;
- quorum certificate verification;
- consensus recovery;
- storage schema validation;
- manifest corruption rejection;
- persistent mempool reload;
- governance vote evidence;
- treasury policy;
- slashing evidence;
- reward settlement;
- key safety gates;
- peer policy and rate limiting;
- sync and fast-sync behavior;
- readiness diagnostics.

## Regression rule

Every discovered bug should leave a regression test unless the failure is purely environmental.

## Integration rule

When two protocol domains interact, tests should verify the integration boundary. Examples:

- governance decision → treasury execution;
- staking registry → validator weight snapshot;
- slashing evidence → penalty ledger;
- transaction execution → coin-lot registry;
- finalized block → manifest state root.
