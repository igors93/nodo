# Module Map

This map describes the intended responsibility of the main source modules.

| Module | Responsibility |
| --- | --- |
| `apps/cli` | CLI entry point. |
| `app` | Command parsing, command policy, CLI execution. |
| `archive` | Proof of Archival (ADR 0014): archive segment commitments, provider registry, replica assignment, challenges, proofs, scoring, replication. |
| `config` | Genesis registry, network profiles, network parameters. |
| `core` | Accounts, transactions, blocks, state transition, state roots, ledger, governance/treasury domain records. |
| `consensus` | Runtime-independent voting, quorum certificates, proposer schedule, finalization records, slashing evidence, and recovery data. |
| `crypto` | Address derivation, key storage, signature providers, crypto policy. |
| `economics` | Emission, rewards, score, protection accounting, staking-related economic helpers. |
| `mempool` | Transaction admission and pending transaction storage. |
| `node` | Runtime services, consensus orchestration (`node/consensus`), bounded history (`node/history`: checkpoints, full state snapshots, bootstrap verification, retention, crash-safe pruning, history sync messages, storage migration), data directory, finalized block store, daemon, RPC, sync, health, metrics. |
| `p2p` | Peer information, messages, gossip, transport, peer policy, rate limiting, discovery. |
| `serialization` | Canonical serialization and codecs. |
| `staking` | Security-weight helper. Stake lifecycle and `StakingRegistry` currently live in `node`. |
| `storage` | Atomic writes and persistent storage helpers. |
| `tests` | Protocol, runtime, and regression tests. |
| `diagnostics` | Python scenarios and operator diagnostics. |

Shared scalar validation policies live in `utils/SafeScalar.hpp`. Legacy
C-string hashing and binary hashing are explicit alternatives in
`utils/HashString.hpp`, so callers preserve their existing commitments while
using one implementation. `economics::MonetaryPolicy` describes a network's
monetary settings; `node::MonetaryFirewallRule` is the narrower inflation-cap
rule used by the firewall. `node::ValidatorRiskEvidenceRecord` records risk
signals, while `consensus::SlashingEvidenceRecord` records cryptographic
equivocation evidence. Their serialized tags are unchanged for compatibility.

The RPC transport and route handlers live in separate translation units, as
do CLI parsing, staking commands and other command execution; finalized
artifact state and its codec; and runtime block execution and result validation.

## Dependency direction

Dependency direction after the Phase 0 orchestration move:

```text
app/node → consensus/core/p2p/crypto/config/storage
core → crypto/serialization
consensus → core/crypto/storage
node → p2p/consensus/core/storage/archive
archive → config/core/crypto/serialization
economics → config/core
```

Avoid circular dependencies. When a domain needs data from another domain, prefer narrow value types or explicit service boundaries.
