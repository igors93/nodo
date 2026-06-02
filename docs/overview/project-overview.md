# Project Overview

Nodo is a security-first blockchain protocol project implemented in C++20. It is designed around verifiable protection, auditable economics, controlled treasury execution, governance evidence, and rebuildable state.

The current codebase is a pre-mainnet foundation. It has a working localnet path and testnet-candidate foundations, but it is not a production network.

## What Nodo Is

Nodo is a protocol and runtime foundation for a blockchain where safety checks are first-class protocol work:

- blocks are finalized through explicit validation and quorum records;
- runtime state can be reloaded and checked against persisted history;
- monetary reports and supply audit records make economic changes traceable;
- treasury execution requires policy validation and lifecycle-backed governance approval;
- governance decisions require verifiable vote evidence;
- penalties and slashing evidence are modeled as auditable, idempotent records.

## What Nodo Is Not Yet

Nodo is not yet:

- a production mainnet;
- a production wallet or custody system;
- a complete public governance application;
- a fully hardened production P2P network;
- an externally audited monetary system.

Mainnet remains blocked until those areas are reviewed and hardened.

## Design Goals

Nodo aims to be:

- simple: core rules should be understandable and reviewable;
- modular: security, economics, storage, P2P, and runtime responsibilities stay separated;
- verifiable: any honest node should be able to rebuild and audit state;
- deterministic: protocol records and proofs should reproduce from canonical inputs;
- secure before expansive: new features should strengthen protection or auditability before adding surface area.
