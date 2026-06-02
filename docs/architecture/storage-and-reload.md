# Storage and Reload

Nodo treats persisted state as untrusted until it has been parsed, rebuilt, and audited.

## Node Data Directory

The local data directory includes:

```text
.nodo/
  storage_schema.nodo
  manifest.nodo
  genesis.nodo
  blocks/
  mempool/
  peers/
  runtime/
  keys/
```

The storage schema must be recognized before the manifest is trusted.

## Reload Principle

Reload follows a rebuild-first model:

```text
storage schema -> manifest -> genesis runtime -> finalized blocks -> mempool -> audit
```

The runtime is accepted only when the rebuilt height, latest hash, state root, finalized artifacts, mempool, validator state, and protocol invariants match the persisted view.

## Audited Persistence

Important persistence paths include:

- finalized block artifacts;
- runtime snapshots;
- persistent mempool records;
- governance lifecycle records;
- treasury execution evidence;
- slashing evidence and validator penalty stores;
- monetary and treasury reports.

Unknown fields, missing required fields, unexpected schema versions, non-canonical files, and temporary write artifacts are rejected instead of being silently interpreted.
