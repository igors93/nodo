# Local Development Networks

The legacy `testnet_local_multi_node.sh` script initializes separate local
chains and produces blocks independently. It is an isolated-node smoke test,
not a networked consensus test.

The four-validator devnet test launches four independent processes, connects
them over authenticated loopback TCP, finalizes a common block, compares the
finalized hash on every node, and audits each node's persisted chain. Run it
after building:

```bash
./scripts/devnet_real_four_node.sh
```

The same test is registered as `node_FourValidatorDevnetTests` in CTest and
runs on Linux and macOS CI. The process harness currently uses POSIX `fork`;
Windows CI does not run this test.

## Purpose

The isolated-node script validates:

- independent data directories;
- local genesis initialization;
- key creation;
- local transaction and block production;
- reload;
- chain audit;
- per-node logs.

Neither local harness replaces a public multi-operator testnet.

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
