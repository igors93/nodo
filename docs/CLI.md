# Nodo CLI

The CLI is a local operator tool for the current Nodo Protocol foundation.

```bash
nodo help
nodo init [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo status [--data-dir PATH]
nodo inspect [--data-dir PATH]
nodo node reload [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo tx submit [--data-dir PATH]
nodo block produce [--data-dir PATH]
nodo chain audit [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo keys create
nodo keys list
nodo validator list
```

## Commands

- `init`: creates a local node data directory from the localnet genesis.
- `status`: prints manifest summary fields.
- `inspect`: prints the serialized manifest.
- `node reload`: rebuilds runtime from manifest, finalized blocks and
  persistent mempool, then reports loaded counts.
- `tx submit`: writes one local transaction into the persistent mempool.
- `block produce`: reloads runtime, produces and finalizes one local block,
  persists it and removes finalized transactions from persistent mempool.
- `chain audit`: reloads runtime and verifies manifest, finalized block
  continuity, latest hash, mempool and validator count consistency.
- `keys create`, `keys list`, `validator list`: protocol command names reserved
  for the key-store and validator registry boundary.

Compatibility commands remain available:

- `demo`
- `reload`
- `submit-demo-transaction`
- `produce-demo-block`

## Localnet Flow

```bash
build/nodo init --data-dir .nodo
build/nodo tx submit --data-dir .nodo
build/nodo block produce --data-dir .nodo
build/nodo node reload --data-dir .nodo
build/nodo chain audit --data-dir .nodo
build/nodo status --data-dir .nodo
```
