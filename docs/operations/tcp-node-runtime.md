# TCP Node Runtime

`nodo node run` starts the long-running TCP/RPC node runtime.

## Example

```bash
./build/nodo node run \
  --network localnet \
  --data-dir .nodo \
  --listen 127.0.0.1:30330 \
  --rpc-listen 127.0.0.1:8545 \
  --validator-key local-validator \
  --identity-key local-user
```

Static peers can be added with repeated `--peer` options:

```bash
--peer node-1@127.0.0.1:30331 --peer node-2@127.0.0.1:30332
```

## Runtime responsibilities

The daemon coordinates:

- peer networking;
- transaction gossip;
- block proposal relay;
- consensus voting;
- finalized artifact handling;
- sync;
- JSON-RPC;
- health and metrics;
- recovery after restart.

## Required keys

`node run` requires validator and peer identity keys. Production networks must not rely on unsafe local defaults.

## Public operation warning

The runtime is suitable for development and testnet-candidate hardening. It should not be exposed as a production mainnet service until custody, monitoring, peer policy, runbooks, and audit requirements are complete.
