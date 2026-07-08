# Module Map

This map describes the intended responsibility of the main source modules.

| Module | Responsibility |
| --- | --- |
| `apps/cli` | CLI entry point. |
| `app` | Command parsing, command policy, CLI execution. |
| `config` | Genesis registry, network profiles, network parameters. |
| `core` | Accounts, transactions, blocks, state transition, state roots, ledger, governance/treasury domain records. |
| `consensus` | Voting, quorum certificates, proposer schedule, finalization, slashing evidence, consensus recovery. |
| `crypto` | Address derivation, key storage, signature providers, crypto policy. |
| `economics` | Emission, rewards, score, protection accounting, staking-related economic helpers. |
| `mempool` | Transaction admission and pending transaction storage. |
| `node` | Runtime services, data directory, finalized block store, daemon, RPC, sync, health, metrics. |
| `p2p` | Peer information, messages, gossip, transport, peer policy, rate limiting, discovery. |
| `serialization` | Canonical serialization and codecs. |
| `staking` | Stake lifecycle, active stake, validator weight projection. |
| `storage` | Atomic writes and persistent storage helpers. |
| `tests` | Protocol, runtime, and regression tests. |
| `diagnostics` | Python scenarios and operator diagnostics. |

## Dependency direction

Preferred direction:

```text
app/node → consensus/core/p2p/crypto/config/storage
core → crypto/serialization
consensus → core/crypto/storage
node → p2p/consensus/core/storage
```

Avoid circular dependencies. When a domain needs data from another domain, prefer narrow value types or explicit service boundaries.
