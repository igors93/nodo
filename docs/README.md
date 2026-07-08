# Nodo Documentation

Nodo is an experimental C++20 blockchain protocol foundation focused on verifiable protection, auditable economics, controlled treasury execution, governance evidence, and rebuildable state.

This directory is the canonical documentation set for the project. It replaces the older mixed documentation layout that contained uppercase filenames, cycle notes, duplicated references, and partially overlapping guides.

> **Status:** pre-mainnet development. Nodo is not ready for production custody, public mainnet operation, or real treasury value.

## Reading path

Start here if you are new to the project:

1. [Project overview](overview/project-overview.md)
2. [Proof of Protection](overview/proof-of-protection.md)
3. [Current status](status.md)
4. [Architecture overview](architecture/architecture-overview.md)
5. [Protocol overview](protocol/protocol-overview.md)
6. [Security model](security/security-model.md)
7. [Roadmap](roadmap.md)

## Documentation map

### Overview

- [Project overview](overview/project-overview.md)
- [Proof of Protection](overview/proof-of-protection.md)
- [Glossary](overview/glossary.md)

### Getting started

- [Quick start](getting-started/quick-start.md)
- [Build](getting-started/build.md)
- [Testing](getting-started/testing.md)
- [CLI](getting-started/cli.md)

### Architecture

- [Architecture overview](architecture/architecture-overview.md)
- [Module map](architecture/module-map.md)
- [Storage and reload](architecture/storage-and-reload.md)

### Protocol

- [Protocol overview](protocol/protocol-overview.md)
- [Consensus](protocol/consensus.md)
- [Blocks and finalization](protocol/blocks-and-finalization.md)
- [Transactions and state](protocol/transactions-and-state.md)
- [Networking and sync](protocol/networking-and-sync.md)

### Economics

- [Economics overview](economics/economics-overview.md)
- [Monetary policy](economics/monetary-policy.md)
- [Coin lots and ledger](economics/coin-lots.md)
- [Staking, rewards and penalties](economics/staking-rewards-and-penalties.md)

### Governance and treasury

- [Governance overview](governance/governance-overview.md)
- [Vote evidence](governance/vote-evidence.md)
- [Lifecycle audit](governance/lifecycle-audit.md)
- [Treasury policy](treasury/treasury-policy.md)
- [Treasury execution evidence](treasury/treasury-execution-evidence.md)

### Security and cryptography

- [Security model](security/security-model.md)
- [Threat model](security/threat-model.md)
- [Key management](security/key-management.md)
- [Cryptography](crypto/cryptography.md)
- [Canonical serialization](serialization/canonical-serialization.md)

### Operations

- [Networks and data directory](operations/networks-and-data-directory.md)
- [Local testnet](operations/local-testnet.md)
- [TCP node runtime](operations/tcp-node-runtime.md)
- [Diagnostics and observability](operations/diagnostics-and-observability.md)

### Development

- [Contributing](development/contributing.md)
- [Coding style](development/coding-style.md)
- [Testing strategy](development/testing-strategy.md)
- [JSON-RPC public API](development/json-rpc-public-api.md)
- [Metrics, health and observability](development/metrics-health-observability.md)
- [Sync, pruning and snapshots](development/sync-pruning-snapshots.md)

## Documentation conventions

- `README.md` is the only uppercase Markdown filename kept by convention.
- All other Markdown files use lowercase kebab-case.
- Historical implementation-cycle notes are not part of the canonical docs.
- Concept documents must clearly distinguish implemented behavior from planned behavior.
- Protocol rules should be deterministic, replayable, and auditable.
