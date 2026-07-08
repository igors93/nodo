# Metrics, Health and Observability

Observability exists so operators can understand whether a node is safe, synced, and participating correctly.

## Endpoints

```text
GET /health
GET /metrics
GET /metrics/prometheus
```

JSON-RPC methods may mirror these operational endpoints:

```text
nodo_getHealth
nodo_getMetrics
nodo_getPrometheusMetrics
```

## Health states

- `HEALTHY`: runtime is valid and operating within thresholds.
- `DEGRADED`: node is usable but has warnings.
- `UNHEALTHY`: node is unsafe, halted, or too far behind.

## Metrics snapshot

A metrics snapshot should be immutable and should not expose mutable runtime objects to handlers.

Useful metrics:

- latest height;
- finalized height;
- finality lag;
- peer count;
- validator count;
- mempool size;
- sync counters;
- RPC status;
- governance counters;
- event bus sequence;
- latest failure reason.
