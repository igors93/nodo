# Metrics, health checks and observability

Nodo exposes live operational state through the official JSON-RPC API and the
operational REST surface.

## Endpoints

- `GET /health` returns a structured health report with status, reasons and the
  metrics snapshot used to make the decision.
- `GET /metrics` returns the raw metrics snapshot as JSON.
- `GET /metrics/prometheus` returns Prometheus text exposition format.
- JSON-RPC `nodo_getHealth` returns the same report as `/health`.
- JSON-RPC `nodo_getMetrics` returns the same JSON metrics payload as
  `/metrics`.
- JSON-RPC `nodo_getPrometheusMetrics` returns the Prometheus text payload as a
  JSON string for tooling that only speaks JSON-RPC.

## Health states

- `HEALTHY`: runtime is valid, RPC is up, sync is healthy and finality lag is
  within the safe threshold.
- `DEGRADED`: the node is still usable but has an operational warning, such as
  recent sync failures, RPC startup issues or finality lag above the degraded
  threshold.
- `UNHEALTHY`: the runtime is invalid/halted or finality lag exceeds the
  unhealthy threshold.

## Why this exists

Multi-node failures are hard to debug from long logs alone. The health and
metrics service gives tests, operators, dashboards and future testnet runbooks a
single place to inspect:

- canonical height and finalized height;
- finality lag;
- mempool size;
- peer and validator counts;
- governance proposal counters;
- event bus retention and latest sequence;
- sync status, counters and last failure reason;
- RPC and runtime state.

The metrics collector returns a primitive, immutable snapshot. It does not expose
mutable runtime objects to API handlers, keeping observability decoupled from
consensus and state-transition logic.
