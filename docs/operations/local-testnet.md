# Local Testnet

The local testnet script starts multiple independent Nodo nodes on the same machine for development validation.

## Purpose

The local testnet validates:

- independent data directories;
- local genesis initialization;
- key creation;
- local transaction and block production;
- reload;
- chain audit;
- per-node logs.

This is not a public testnet and does not replace live multi-operator validation.

## Prerequisites

Build the binary first:

```bash
./scripts/cmake_build.sh
```

## Run

```bash
./scripts/testnet_local_multi_node.sh
```

## Useful options

```bash
# Resume existing data directories
./scripts/testnet_local_multi_node.sh --resume

# Clean local testnet data
./scripts/testnet_local_multi_node.sh --clean
```

## Environment variables

| Variable | Default | Meaning |
| --- | --- | --- |
| `NODO_TESTNET_DIR` | `<repo>/testnet/local` | Root directory for local nodes. |
| `NODO_NODE_COUNT` | `4` | Number of local nodes. |
| `NODO_BLOCKS` | `3` | Blocks produced per node. |
| `NODO_BASE_PORT` | `30330` | First TCP port. |
| `NODO_BUILD_JOBS` | `1` | Parallel build jobs. |

Example:

```bash
NODO_NODE_COUNT=6 NODO_BLOCKS=5 ./scripts/testnet_local_multi_node.sh
```

## Audit

The script should end by confirming reload and chain audit. Any mismatch indicates a protocol, storage, or local environment problem that should be investigated before continuing.
