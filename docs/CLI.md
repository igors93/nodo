# Nodo CLI

The CLI is a local operator tool for the current Nodo Protocol foundation.

```bash
nodo help
nodo init [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo status [--data-dir PATH]
nodo inspect [--data-dir PATH]
nodo node reload [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo node run [--network localnet|testnet-candidate] [--data-dir PATH] [--listen HOST:PORT] [--peer NAME@HOST:PORT]... [--validator-key ID]
nodo keys create [--data-dir PATH] [--type user|validator|both] [--key-id ID]
nodo keys list [--data-dir PATH]
nodo tx submit [--data-dir PATH] [--from KEY_ID] [--to ADDRESS] [--amount RAW_UNITS] [--fee RAW_UNITS] [--nonce VALUE]
nodo stake lock|deposit|top-up|unlock|withdraw [--data-dir PATH] (--validator ADDRESS | --validator-key ID) --amount RAW_UNITS [--owner KEY_ID] [--fee RAW_UNITS]
nodo stake status [--data-dir PATH] (--validator ADDRESS | --validator-key ID)
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
- `node run`: starts a long-running node daemon. Verifies genesis, checks
  data-dir compatibility, loads an optional validator key (via
  `--validator-key`), registers static peers (`--peer NAME@HOST:PORT`), then
  runs the `NodeDaemon` tick loop until SIGINT or SIGTERM. Rejected for
  `LOCKED_PRODUCTION` (mainnet) networks. Uses `ProductionKeySafetyGate` to
  refuse localnet-only keys on official networks. Each tick drains gossip
  inboxes: `TRANSACTION_GOSSIP` payloads are deduplicated via
  `SeenTransactionCache`, validated and relayed; `BLOCK_PROPOSAL` messages are
  applied via `BlockAnnounceHandler`; `FINALIZED_BLOCK_ARTIFACT` messages are
  verified against the local validator registry before being recorded.
- `keys create`: creates localnet keys in `.nodo/keys`. Without `--type`, it
  creates both `local-user` Ed25519 and `local-validator` BLS12-381 keys.
- `keys list`: lists public key metadata without printing private material.
- `tx submit`: loads a key from `KeyStore`, builds a transaction through
  `TransactionBuilder`, signs it through `Signer`, validates it with
  `TransactionAdmissionValidator` and only then writes it to the persistent
  mempool. If `--nonce` is omitted, localnet uses the next nonce from the
  account state derived from canonical protocol replay. Duplicate transactions,
  duplicate sender/nonce, old nonces, unsupported future nonces, low fees and
  invalid signatures are rejected before persistence.
- `stake lock`/`deposit`/`top-up`/`unlock`/`withdraw`: resolves the target from
  an explicit validator address or from validator-key metadata, signs with the
  independent owner key (`--owner`, default `local-user` on localnet), applies
  runtime stake admission rules, and persists only accepted transactions.
  When both target forms are supplied they must resolve to the same address.
- `stake status`: resolves the same validator identity without loading private
  key material and reports canonical registry, stake-account, and position
  state rebuilt from finalized blocks.
- `block produce`: DEVELOPMENT_LOCAL-only helper. It reloads runtime, produces
  and finalizes one local block with a single local PRECOMMIT-backed QC, persists
  it and removes finalized transactions from persistent mempool. It does not
  create transactions automatically and is rejected on testnet-candidate and
  production network classes; real networks must finalize through distributed
  PREVOTE/PRECOMMIT consensus. Produced blocks must pass authoritative protocol
  state-transition checks for balance, nonce, fee, authorization and domain
  commitments before finalization.
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

Current localnet limits remain intentional: the `block produce` shortcut is not
production P2P consensus, mainnet startup remains blocked, automatic production
stake-slashing is not active, and local keys are deterministic and unencrypted.
These limits are audited explicitly instead of being hidden behind demo-only code
paths.
