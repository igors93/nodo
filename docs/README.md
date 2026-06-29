# Nodo Documentation

This directory is the documentation entry point for Nodo. The canonical docs are organized by topic and are written for operators, contributors, auditors, and protocol reviewers.

Nodo is currently in development and pre-mainnet. Documentation should be read with that status in mind.

## Overview

- [Project Overview](overview/project-overview.md)
- [Proof of Protection](overview/proof-of-protection.md)
- [Glossary](overview/glossary.md)

## Getting Started

- [Quick Start](getting-started/quick-start.md)
- [Build](getting-started/build.md)
- [Testing](getting-started/testing.md)
- [CLI](getting-started/cli.md)

## Architecture

- [Architecture Overview](architecture/architecture-overview.md)
- [Module Map](architecture/module-map.md)
- [Storage and Reload](architecture/storage-and-reload.md)
- [Legacy Architecture Reference](ARCHITECTURE.md)

## Protocol

- [Blocks and Finalization](protocol/blocks-and-finalization.md)
- [Transactions](protocol/transactions.md)
- [Consensus](protocol/consensus.md)
- [Protocol Reference](PROTOCOL.md)
- [Consensus Rules Reference](CONSENSUS_RULES.md)
- [State Transition Reference](STATE_TRANSITION.md)

## Economics

- [Monetary Policy](economics/monetary-policy.md)
- [Supply and Reports](economics/supply-and-reports.md)
- [Rewards](economics/rewards.md)
- [Proof of Protection Economics Reference](economics/PROOF_OF_PROTECTION.md)
- [Coin Lot Registry](economics/COIN_LOT_REGISTRY.md)
- [Coin Lot Transaction Integration](economics/COIN_LOT_TRANSACTION_INTEGRATION.md)

## Treasury

- [Treasury Policy](treasury/treasury-policy.md)
- [Treasury Execution Evidence](treasury/treasury-execution-evidence.md)

## Governance

- [Governance Overview](governance/governance-overview.md)
- [Vote Evidence](governance/vote-evidence.md)
- [Lifecycle Audit](governance/lifecycle-audit.md)

## Security

- [Security Model](security/security-model.md)
- [Threat Model](security/threat-model.md)
- [Key Management](security/key-management.md)
- [Security Reference](SECURITY_MODEL.md)

## Node Operations

- [Networks](NETWORKS.md)
- [Node Data Directory](NODE_DATA_DIRECTORY.md)
- [Development Mode](DEVELOPMENT_MODE.md)
- [Testnet Node Runtime](TCP_TESTNET_NODE_RUNTIME.md)
- [Persistent Block State Sync](PERSISTENT_BLOCK_STATE_SYNC.md)

## Development

- [Contributing](development/contributing.md)
- [Coding Style](development/coding-style.md)
- [Testing Strategy](development/testing-strategy.md)
- [Canonical Serialization](serialization/CANONICAL_SERIALIZATION.md)

## Roadmap

- [Roadmap](ROADMAP.md)

## Archive

- [Archive Overview](archive/README.md)
- [Implementation Cycle Archive](archive/cycles/README.md)

## Documentation Rules

- Do not claim production readiness unless a release explicitly earns that status.
- Keep protocol, economics, governance, treasury, and security claims tied to implemented code.
- Archive historical notes instead of presenting them as current behavior.
- Keep every internal link pointing to an existing file.
