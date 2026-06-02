# CLI

The Nodo CLI is a local operator and development tool. It exposes the current localnet and testnet-candidate foundations.

## Help

```bash
build/nodo help
```

On Windows:

```powershell
.\build\nodo.exe help
```

## Current Commands

```text
nodo init [--network localnet|testnet-candidate] [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo status [--network localnet|testnet-candidate] [--data-dir PATH]
nodo inspect [--network localnet|testnet-candidate] [--data-dir PATH]
nodo node reload [--network localnet|testnet-candidate] [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo keys create [--network localnet|testnet-candidate] [--data-dir PATH] [--type user|validator|both] [--key-id ID]
nodo keys list [--data-dir PATH]
nodo tx submit [--data-dir PATH] [--from KEY_ID] [--to ADDRESS] [--amount RAW_UNITS] [--fee RAW_UNITS] [--nonce VALUE]
nodo block produce [--data-dir PATH]
nodo chain audit [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo testnet readiness [--network localnet|testnet-candidate] [--data-dir PATH] [--key-id ID]
nodo diagnostics [--network localnet|testnet-candidate] [--data-dir PATH] [--key-id ID]
```

## Network Profiles

- `localnet`: local development path.
- `testnet-candidate`: official pre-testnet profile with safety gates.
- `mainnet`: blocked until production readiness requirements are satisfied.

## Compatibility Aliases

Compatibility commands such as `submit-demo-transaction`, `produce-demo-block`, and `reload` may still exist for migration. Prefer the current protocol command names in new documentation and scripts.
