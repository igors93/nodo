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
nodo node run [--network localnet|testnet-candidate] [--data-dir PATH] [--listen HOST:PORT] [--peer NAME@HOST:PORT]... [--validator-key ID]
nodo node reload [--network localnet|testnet-candidate] [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo keys create [--network localnet|testnet-candidate] [--data-dir PATH] [--type user|validator|both] [--key-id ID]
nodo keys list [--data-dir PATH]
nodo tx submit [--data-dir PATH] [--from KEY_ID] [--to ADDRESS] [--amount RAW_UNITS] [--fee RAW_UNITS] [--nonce VALUE]
nodo governance propose [--data-dir PATH] [--from KEY_ID] [--proposal-type parameter-change|treasury-spend|text] [--title TEXT] [--body TEXT] ...
nodo governance vote [--data-dir PATH] [--owner KEY_ID] --proposal-id ID --validator ADDRESS [--vote YES|NO|ABSTAIN] [--fee RAW_UNITS]
nodo governance status|list|show|audit [--data-dir PATH] [--proposal-id ID]
nodo stake lock|deposit|top-up|unlock|withdraw [--data-dir PATH] (--validator ADDRESS | --validator-key ID) --amount RAW_UNITS [--owner KEY_ID] [--fee RAW_UNITS]
nodo stake status [--data-dir PATH] (--validator ADDRESS | --validator-key ID)
nodo stake positions|audit [--data-dir PATH] [--validator ADDRESS] [--address ADDRESS]
nodo block produce [--data-dir PATH]
nodo chain audit [--data-dir PATH] [--peer-id ID] [--endpoint HOST:PORT]
nodo validator list [--data-dir PATH]
nodo testnet readiness [--network localnet|testnet-candidate] [--data-dir PATH] [--key-id ID]
nodo diagnostics [--network localnet|testnet-candidate] [--data-dir PATH] [--key-id ID]
```

For per-command behavior details see the [CLI Reference](../CLI.md).

## Network Profiles

- `localnet`: local development path.
- `testnet-candidate`: official pre-testnet profile with safety gates.
- `mainnet`: blocked until production readiness requirements are satisfied.
