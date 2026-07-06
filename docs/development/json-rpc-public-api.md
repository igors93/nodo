# JSON-RPC Public API

Nodo exposes JSON-RPC 2.0 as the official public protocol API over the existing node HTTP socket. The endpoint is:

```text
POST /rpc
Content-Type: application/json
```

The older REST routes are intentionally kept as operational and compatibility endpoints for local status, diagnostics, integration tests and dashboards. Wallets, explorers, staking tooling and governance clients should prefer JSON-RPC.

## Design rules

- JSON-RPC and REST share the same `NodeRpcServer` socket and runtime mutex.
- Every JSON-RPC method is routed through the same canonical runtime handlers used by the existing operational routes, avoiding divergent business logic.
- Requests are bounded by `NodeRpcServer::MAX_REQUEST_LEN`.
- Per-source-IP rate limiting is applied before request dispatch.
- Runtime access is serialized with the daemon/orchestrator tick so RPC cannot observe torn state.
- `POST /rpc` is the public protocol path; `/status`, `/metrics`-style routes remain operational.

## Core methods

```text
rpc_methods
nodo_getStatus
nodo_getChainInfo
nodo_getBlockByHeight
nodo_getBlockByHash
nodo_getTransactionById
nodo_getAccountState
nodo_getAccountProof
nodo_getValidators
nodo_getPeers
nodo_getMempoolStats
nodo_estimateFee
nodo_sendTransaction
nodo_sendRawTransaction
```

## Governance methods

```text
governance_status
governance_proposals
governance_getProposal
governance_getVotes
governance_getTally
governance_getDecision
governance_getExecution
governance_submitProposal
governance_submitVote
governance_submitExecution
```

## Staking methods

```text
stake_status
stake_positions
stake_getPosition
stake_deposit
stake_topUp
stake_unlock
stake_withdraw
stake_pendingUnbonding
stake_validatorStake
stake_auditStatus
```

## Example

```json
{
  "jsonrpc": "2.0",
  "method": "nodo_getChainInfo",
  "params": {},
  "id": "1"
}
```

Response:

```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": {
    "networkName": "localnet",
    "chainId": "...",
    "height": 0
  }
}
```

## Transaction submission

`nodo_sendTransaction`, `nodo_sendRawTransaction`, governance submit methods and staking mutation methods expect a self-contained signed transaction submission envelope in the `tx` or `transaction` parameter, matching the schema accepted by the existing `/submit` route. The RPC server rejects raw `Transaction{...}` payloads that lack public key material.

## Compatibility

No REST file was deleted. REST remains available for local operations and existing tests, but new external integrations should treat JSON-RPC as the stable public protocol API.
