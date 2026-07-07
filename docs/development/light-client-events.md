# Light client and event stream

Nodo exposes light-client data through the official JSON-RPC API and an event
stream through a WebSocket-compatible endpoint.

## Light-client JSON-RPC methods

- `light_getCheckpoint`: returns the latest finalized header, including the
  block header payload, block hash, state root, validator-set root, serialized
  finalization record and serialized QC.
- `light_getHeaders`: returns a contiguous finalized header range. Parameters:
  `fromHeight`, `maxHeaders`.
- `light_getAccountProof`: returns a Merkle proof for an account against the
  current account-state root.
- `light_getTransactionProof`: returns a Merkle proof that a ledger record
  matching a transaction id or source id is committed in a finalized block.

The light-client header includes enough data for a client to verify:

1. the block hash matches the canonical block header payload;
2. each returned header links to the previous header;
3. the header carries the QC/finalized record needed for finality verification;
4. account and transaction proofs reconstruct their committed Merkle roots.

## Event stream

`GET /events` with `Upgrade: websocket` opens a WebSocket-compatible stream of
canonical node events. Events are retained in a bounded in-memory ring buffer and
can also be queried with `nodo_getEvents`.

Event types include:

- `block_finalized`
- `tx_admitted`
- `light_client_checkpoint`
- `rpc_subscription`

The event stream is intentionally backed by a `NodeEventBus` module rather than
being embedded in the HTTP server. Consensus, RPC submission and future modules
can publish events without depending on WebSocket internals.

REST compatibility remains available for diagnostics, but wallets and explorers
should prefer JSON-RPC plus the event stream.
