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

## RPC server

`--rpc-listen` accepts an IPv4 address, an IPv6 address in brackets (`[::1]:8545`), or a host name such as `localhost:8545`. The default is `127.0.0.1:8545`.

The RPC server runs on standalone Asio with a fixed pool of 4 worker threads; no thread is held per connection. Each connection carries one request (`Connection: close`) or upgrades `GET /events` to a WebSocket event stream.

| Limit | Default | Behaviour at the limit |
| --- | --- | --- |
| Concurrent connections | 128 | Further connections are closed unanswered |
| WebSocket subscribers | 32 (within the 128) | `503` |
| Time to receive a whole request | 10 s | Connection closed |
| Request size | 64 KiB, of which at most 16 KiB of headers | `413` / `431` |
| Requests per client IP | 3000 per 60 s window | `429` |

Malformed requests get `400`, and `Transfer-Encoding` is refused with `501`: bodies are framed by `Content-Length` only. A WebSocket subscriber that stops reading is dropped once 256 frames are queued, and an unmasked client frame closes the stream (RFC 6455).

## Required keys

`node run` requires validator and peer identity keys. Production networks must not rely on unsafe local defaults.

## Public operation warning

The runtime is suitable for development and testnet-candidate hardening. It should not be exposed as a production mainnet service until custody, monitoring, peer policy, runbooks, and audit requirements are complete.
