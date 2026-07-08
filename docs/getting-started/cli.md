# CLI

The `nodo` binary exposes development, node, governance, staking, validator, audit, and diagnostic commands.

## Help

```bash
./build/nodo help
```

## Core commands

```text
nodo init
nodo status
nodo inspect
nodo node run
nodo node reload
nodo node prune
nodo node pruning-status
nodo keys create
nodo keys list
nodo tx submit
nodo block produce
nodo chain audit
nodo testnet readiness
nodo diagnostics
```

## Governance commands

```text
nodo governance propose
nodo governance vote
nodo governance execute
nodo governance status
nodo governance list
nodo governance show
nodo governance audit
```

## Validator and staking commands

```text
nodo validator list
nodo validator status
nodo validator exit
nodo validator unjail
nodo stake lock
nodo stake deposit
nodo stake top-up
nodo stake unlock
nodo stake withdraw
nodo stake status
nodo stake positions
nodo stake audit
nodo rewards status
nodo slashing evidence
```

## Common options

| Option | Meaning |
| --- | --- |
| `--data-dir PATH` | Node data directory. Default: `.nodo`. |
| `--network NAME` | Network profile. Common values: `localnet`, `localnet-soak`, `testnet-candidate`. Mainnet is blocked. |
| `--peer-id ID` | Local peer id for init/load. |
| `--endpoint HOST:PORT` | Local endpoint for init/load. |
| `--listen HOST:PORT` | Bind address for `node run`. |
| `--rpc-listen HOST:PORT` | JSON-RPC/HTTP bind address for `node run`. Default: `127.0.0.1:8545`. |
| `--peer NAME@HOST:PORT` | Static peer for `node run`. Repeatable. |
| `--validator-key ID` | Validator identity key. |
| `--identity-key ID` | Peer identity key. Default: `local-user`. |
| `--key-id ID` | Key id for creation or signing. |

## Development safety

Legacy demo commands are intentionally treated as development-only and should not be part of public-network documentation. Prefer the canonical commands listed above.
