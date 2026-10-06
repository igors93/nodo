# Diagnostics and Observability

Library diagnostics are emitted as one JSON object per line on the process's
standard error stream, with `time_unix_ms`, `level`, `component`, and `message`
fields. Set `NODO_LOG_LEVEL` to `TRACE`, `DEBUG`, `INFO`, `WARN` (default), or
`ERROR` before starting a node. Library code does not write operational
messages to standard output, so CLI command output remains parseable.

Nodo exposes diagnostics through CLI, Python scenarios, REST operational endpoints, and JSON-RPC methods.

## CLI diagnostics

```bash
./build/nodo testnet readiness --network testnet-candidate --data-dir .nodo
./build/nodo diagnostics --network testnet-candidate --data-dir .nodo
```

## Health endpoints

Operational HTTP endpoints include:

```text
GET /health
GET /metrics
GET /metrics/prometheus
```

JSON-RPC equivalents may include:

```text
nodo_getHealth
nodo_getMetrics
nodo_getPrometheusMetrics
```

## Health states

| State | Meaning |
| --- | --- |
| `HEALTHY` | Runtime is valid, RPC is up, sync is healthy, and finality lag is within threshold. |
| `DEGRADED` | Node is usable but has warnings such as sync failures or elevated finality lag. |
| `UNHEALTHY` | Runtime is invalid, halted, or finality lag exceeds the unsafe threshold. |

## Metrics should cover

- chain height;
- finalized height;
- finality lag;
- mempool size;
- peer count;
- validator count;
- governance proposal counters;
- event bus status;
- sync status;
- RPC status;
- latest failure reason.

## Purpose

Observability should help operators detect unsafe state before it becomes a network incident.
