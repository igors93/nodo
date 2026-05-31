# Nodo CLI

The CLI is a local operator tool for the current Nodo Protocol foundation.

```bash
nodo help
nodo init [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo status [--data-dir PATH]
nodo inspect [--data-dir PATH]
nodo node reload [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo keys create [--data-dir PATH] [--type user|validator|both] [--key-id ID]
nodo keys list [--data-dir PATH]
nodo tx submit [--data-dir PATH] [--from KEY_ID] [--to ADDRESS] [--amount RAW_UNITS] [--fee RAW_UNITS] [--nonce VALUE]
nodo block produce [--data-dir PATH]
nodo chain audit [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo validator list [--data-dir PATH]
```

## Commands

- `init`: creates a local node data directory from the localnet genesis.
- `status`: prints manifest summary fields.
- `inspect`: prints the serialized manifest.
- `node reload`: rebuilds runtime from manifest, finalized blocks and
  persistent mempool, then reports loaded counts.
- `keys create`: creates localnet keys in `.nodo/keys`. Without `--type`, it
  creates both `local-user` Ed25519 and `local-validator` BLS12-381 keys.
- `keys list`: lists public key metadata without printing private material.
- `tx submit`: loads a key from `KeyStore`, builds a transaction through
  `TransactionBuilder`, signs it through `Signer`, validates it with
  `TransactionAdmissionValidator` and only then writes it to the persistent
  mempool. If `--nonce` is omitted, localnet uses the next nonce from the
  rebuilt account state. Duplicate transactions, duplicate sender/nonce,
  old nonces, unsupported future nonces, low fees and invalid signatures are
  rejected before persistence.
- `block produce`: reloads runtime, produces and finalizes one local block,
  persists it and removes finalized transactions from persistent mempool. It
  does not create transactions automatically. Produced blocks must pass
  account-state preview checks for balance, nonce and fee before votes or
  finalization.
- `chain audit`: reloads runtime and runs `ChainAuditor` over manifest,
  finalized block continuity, latest hash, `latestStateRoot`, crypto context,
  mempool and validator count consistency.
- `keys create`, `keys list`, `validator list`: protocol command names reserved
  for the key-store and validator registry boundary.

Compatibility commands remain available but are deprecated:

- `demo`
- `reload`
- `submit-demo-transaction`
- `produce-demo-block`

## Localnet Flow

```bash
build/nodo init --data-dir .nodo
build/nodo keys create --data-dir .nodo
build/nodo tx submit --data-dir .nodo
build/nodo block produce --data-dir .nodo
build/nodo node reload --data-dir .nodo
build/nodo chain audit --data-dir .nodo
build/nodo status --data-dir .nodo
```

Current localnet limits remain intentional: no production P2P, no mainnet
startup path, no slashing, unencrypted deterministic local keys and no
per-account future-nonce queue. These limits are audited explicitly instead of
being hidden behind demo-only code paths.
