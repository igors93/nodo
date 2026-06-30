# Nodo Architecture

Nodo is organized by runtime responsibility rather than implementation phase.

## Modules

- `app`: command-line interface and local development commands.
- `core`: blocks, blockchain, transactions, ledger records, state, validators
  and coin lot rules.
- `consensus`: votes, quorum certificates, finalization and fork choice.
- `mempool`: volatile transaction admission and selection.
- `node`: local runtime, data directory, runtime loader, runtime verifier,
  finalized block artifact codec/store, persistent mempool, local block
  pipeline, `NodeOrchestrator` (transport + peer discovery + sync + consensus
  event loop + RPC) and `NodeDaemon` (wraps `NodeOrchestrator` to add static
  peer registration, transaction gossip processing, block proposal relaying and
  finalized artifact verification).
- `storage`: generic chain/block storage helpers and atomic file IO.
- `serialization`: strict canonical binary codecs for protocol boundaries plus
  deterministic text codecs for legacy development persistence.
- `crypto`: hash, key, signature-provider and development-signature boundaries.
- `economics`: protection work, emissions, rewards, validator scores and
  penalties.
- `privacy`: experimental private accounting records and nullifier tracking.
- `p2p`: peer messages, sync planning, gossip mesh, loopback/TCP transports and
  encrypted peer-channel framing for testnet development.

## Persistence Flow

The local node directory stores an explicit storage schema version, a manifest,
finalized block files, a runtime snapshot and pending mempool transactions.
Runtime reload is rebuild-first:

```text
storage schema -> manifest -> genesis runtime -> finalized blocks -> persistent mempool -> audit
```

The manifest is accepted only after the storage schema is recognized and the
rebuilt latest block height, hash and `latestStateRoot` match it. Finalized
block files carry a `postStateRoot`, quorum certificate and finalized record;
reload verifies all three before the runtime is considered auditable.

Reload responsibilities are intentionally split, but replay itself is unified.
`RuntimeStateLoader` coordinates durable reads while `ProtocolStateTransition`
replays each finalized block into one `ProtocolReplayState` containing accounts
and all protocol domains. `FinalizedBlockArtifactCodec` owns finalized artifact
parsing/serialization shape. `FinalizedArtifactValidator` coordinates domain
validators for finality, authoritative state-transition execution, economic
records, monetary and treasury records, slashing evidence, governance records and
validator lifecycle accounting. `RuntimeStateVerifier` centralizes
manifest-to-runtime checks by recalculating `latestStateRoot` from the same
canonical replay state; it no longer accepts an account-only root as a protocol
commitment. `ProtocolInvariantChecker` performs heavy runtime invariants after
genesis and reload. `ChainAuditor` adds the final operational checks for crypto
context, mempool admission policy and validator registry consistency.

## Daemon and Gossip Flow

`NodeDaemon` wraps `NodeOrchestrator` and adds a tick-driven gossip processing
layer:

```text
NodeDaemon.tick()
  ├── NodeOrchestrator.tick()          — transport I/O, peer heartbeats, block sync
  ├── processTransactionGossip()       — drain TRANSACTION_GOSSIP inbox
  │     ├── SeenTransactionCache       — LRU+TTL dedup by payloadHash
  │     ├── PersistentMempoolStore::deserializeGossip()          — decode + Ed25519 verify
  │     ├── TransactionAdmissionValidator                       — account + domain admission
  │     └── gossipBroadcast()          — relay if newly admitted
  └── processFinalizedArtifacts()      — drain FINALIZED_BLOCK_ARTIFACT inbox
        ├── FinalizedBlockRecord::deserialize()
        ├── record.verify()            — QC check vs. local validator registry
        └── FinalizationRegistry::registerFinalizedBlock()
```

`ConsensusEventLoop` runs in a background thread inside `NodeOrchestrator` and
owns proposal admission. A validated `BLOCK_PROPOSAL` is retained as a
round-scoped candidate, never appended as canonical state. The loop handles
`VALIDATOR_VOTE` / `VOTE_ANNOUNCE` accumulation, casts PREVOTE before PRECOMMIT
and assembles a `QuorumCertificate` only from PRECOMMIT votes; only
`BlockFinalizer` appends the candidate after quorum. When vote admission rejects
a same-validator conflict, the same tick converts the accepted vote plus rejected
vote into verified slashing evidence, persists it and gossips it. The in-process
localnet production helper is not available to staging or production network
classes.

## Build

CMake builds one `nodo_core` static library, one `nodo` CLI executable and one
test executable per `tests/<module>/*.cpp` file. CTest labels tests by module.


Consensus recovery is part of the voting boundary. `ConsensusEventLoop` builds a
signed PREVOTE/PRECOMMIT, persists that exact record through
`ConsensusRecoveryStore`, then submits and broadcasts it. If the process restarts
after persistence but before successful broadcast, the loaded vote is resubmitted
to the local vote pool and rebroadcast once the matching proposal is active.
