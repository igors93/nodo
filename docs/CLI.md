# Nodo CLI

The CLI is a local operator and development tool.

```bash
nodo help
nodo demo
nodo init [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo status [--data-dir PATH]
nodo inspect [--data-dir PATH]
nodo reload [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo submit-demo-transaction [--data-dir PATH]
nodo produce-demo-block [--data-dir PATH]
```

## Commands

- `init`: creates a local node data directory from the development genesis.
- `status`: prints manifest summary fields.
- `inspect`: prints the serialized manifest.
- `reload`: rebuilds runtime from manifest, finalized blocks and persistent
  mempool, then reports loaded counts.
- `submit-demo-transaction`: writes one development transaction into the
  persistent mempool.
- `produce-demo-block`: reloads runtime, produces and finalizes one local demo
  block, persists it and removes finalized transactions from persistent mempool.

## Development Flow

```bash
build/nodo init --data-dir .nodo
build/nodo submit-demo-transaction --data-dir .nodo
build/nodo produce-demo-block --data-dir .nodo
build/nodo reload --data-dir .nodo
build/nodo status --data-dir .nodo
```
