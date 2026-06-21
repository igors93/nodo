# Local Testnet

This document explains how to run a local multi-node Nodo testnet for development and validation.

## Overview

The local testnet starts four independent Nodo nodes on the same machine, each with its own data directory. Every node initializes from the shared localnet genesis, produces blocks, reloads state from disk, and passes chain audit.

This is not a production testnet. It validates the local block production and audit pipeline across multiple isolated nodes. P2P block gossip between nodes is a later phase.

## Prerequisites

- CMake 3.20+
- OpenSSL (libcrypto)
- BLST library (`scripts/install_blst.sh`)
- A POSIX shell (bash)

Build the binary first:

```bash
./scripts/cmake_build.sh
```

The executable is placed at `build/nodo`.

## Quick Start

```bash
./scripts/testnet_local_multi_node.sh
```

This initializes 4 nodes, produces 3 blocks on each, then runs reload and chain audit. All output is printed to the terminal and also written to per-node log files under `testnet/local/`.

## Command Reference

```bash
# Fresh 4-node testnet
./scripts/testnet_local_multi_node.sh

# Resume an existing testnet (skip init/keys if data directories exist)
./scripts/testnet_local_multi_node.sh --resume

# Remove testnet directories and exit
./scripts/testnet_local_multi_node.sh --clean
```

## Configuration

Override defaults with environment variables before running the script:

| Variable            | Default               | Description                                 |
|---------------------|-----------------------|---------------------------------------------|
| `NODO_TESTNET_DIR`  | `<repo>/testnet/local`| Root directory for all node data            |
| `NODO_NODE_COUNT`   | `4`                   | Number of nodes to start                    |
| `NODO_BLOCKS`       | `3`                   | Blocks to produce per node                  |
| `NODO_BASE_PORT`    | `30330`               | First TCP peer port (increments per node)   |
| `NODO_BUILD_JOBS`   | `1`                   | Parallel build jobs                         |

Example — 6 nodes, 5 blocks each:

```bash
NODO_NODE_COUNT=6 NODO_BLOCKS=5 ./scripts/testnet_local_multi_node.sh
```

## Directory Layout

```
testnet/local/
├── node-0/                # Data directory for node 0
│   ├── manifest.nodo
│   ├── genesis.nodo
│   ├── storage_schema.nodo
│   ├── blocks/
│   │   ├── block_1.nodo
│   │   ├── block_2.nodo
│   │   └── block_3.nodo
│   ├── keys/
│   │   ├── local-user.nodo
│   │   └── local-validator.nodo
│   ├── mempool/
│   └── peers/
├── node-1/                # Same structure for node 1
├── node-2/                # Same structure for node 2
├── node-3/                # Same structure for node 3
├── node-0.log             # Log for node 0
├── node-1.log
├── node-2.log
└── node-3.log
```

## Manual Steps

The script executes these commands for each node. You can run them manually for inspection:

```bash
# 1. Initialize
build/nodo init --data-dir testnet/local/node-0 --peer-id node-0 --endpoint 127.0.0.1:30330

# 2. Create keys
build/nodo keys create --data-dir testnet/local/node-0

# 3. Submit a transaction and produce a block (repeat for more blocks)
build/nodo tx submit --data-dir testnet/local/node-0
build/nodo block produce --data-dir testnet/local/node-0

# 4. Reload state
build/nodo node reload --data-dir testnet/local/node-0 --peer-id node-0 --endpoint 127.0.0.1:30330

# 5. Audit
build/nodo chain audit --data-dir testnet/local/node-0 --peer-id node-0 --endpoint 127.0.0.1:30330

# 6. Status
build/nodo status --data-dir testnet/local/node-0
```

## Audit Verification

The `chain audit` command validates:

- Storage schema version
- Manifest completeness and integrity
- Genesis config compatibility
- Finalized block continuity (no gaps, no hash divergence)
- State root consistency (rebuilt state must match manifest)
- Mempool validity
- Validator count consistency
- Monetary policy invariants

A passing audit means the node's full history is verifiable and rebuildable from scratch.

## Log Files

Each node writes timestamped logs to `testnet/local/node-N.log`. To inspect a node's output:

```bash
cat testnet/local/node-0.log
```

Log entries have the format `[timestamp][node-N] message`.

## Running the Integration Test

The C++ integration test (`TestnetLocalFourNodeIntegrationTests`) validates the same 4-node scenario programmatically via the internal C++ API:

```bash
./scripts/test_testnet_local_multi_node.sh
```

Or run directly with CTest after building:

```bash
ctest --test-dir build/cmake -R node_TestnetLocalFourNodeIntegrationTests --output-on-failure
```

### What the test covers

1. All 4 nodes initialize from a shared genesis with the same genesis id and chain id.
2. Each node has a distinct data directory, peer endpoint, and validator key.
3. Each node produces 3 blocks independently using a solo-validator genesis.
4. Each node reloads its state from disk after block production.
5. Each node passes chain audit after reload.
6. All 4 nodes reach the same finalized height (3 blocks).
7. A node rejects loading a data directory initialized with a different genesis.
8. Fresh initialization leaves all 4 nodes at block height 0.

## Known Limitations

- **No live P2P block sync**: nodes produce blocks independently. Sharing blocks between nodes requires the P2P gossip and sync layer (next phase).
- **Shared localnet keys**: the CLI uses deterministic localnet keys (`local-user`, `local-validator`) — all nodes get the same keys. These are not suitable for production or public testnets.
- **Single-validator consensus**: block production uses a solo-validator genesis so each node can finalize blocks alone. A real multi-node consensus requires all validators to vote over the network.
- **No automatic peer discovery**: peer addresses must be configured manually.

## Next Steps

1. P2P block sync between nodes (persistent block state sync layer).
2. Multi-node validator voting over TCP transport.
3. Encrypted key storage with per-node distinct keys.
4. Automatic peer discovery and reconnection policy.
5. Public testnet node with bootnodes.
