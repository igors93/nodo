# Module Map

| Module | Main Paths | Responsibility |
| --- | --- | --- |
| App / CLI | `apps/cli`, `include/app`, `src/app` | Command parsing, local operator commands, network selection, and local runtime orchestration. |
| Config | `include/config`, `src/config` | Genesis config and network profile registry. |
| Core | `include/core`, `src/core` | Blocks, transactions, account state, state roots, validators, and state-transition preview. |
| Consensus | `include/consensus`, `src/consensus` | Round management, votes, quorum certificates, proposer scheduling, finalization, and slashing evidence foundations. |
| Crypto | `include/crypto`, `src/crypto` | Hashing, key storage, signature providers, address derivation, and production key safety boundaries. |
| Economics | `include/economics`, `src/economics` | Monetary policy, supply audit, rewards, treasury, governance, stake/slash application, and validator penalty economics. |
| Mempool | `include/mempool`, `src/mempool` | Transaction admission and pending transaction selection. |
| Node | `include/node`, `src/node` | Node runtime, finalized artifacts, data directory, reload, diagnostics, readiness, and chain audit. |
| P2P | `include/p2p`, `src/p2p` | Protocol messages, gossip mesh, loopback/TCP transport, encrypted channels, sync, and peer limiting. |
| Serialization | `include/serialization`, `src/serialization` | Canonical binary and strict key-value codecs. |
| Staking | `include/staking`, `src/staking` | Security-weight primitives (staking registry lives in `node`). |
| Storage | `include/storage`, `src/storage` | Atomic file operations and persistent stores. |
| Utils | `include/utils`, `src/utils` | Shared primitives such as `Amount` and time helpers. |
| Tests | `tests` | Module tests discovered by CMake and run by CTest. |

## Build Shape

CMake builds:

- `nodo_core`: static library containing repository sources;
- `nodo`: CLI executable;
- one test executable per `tests/**/*.cpp` file.
