# JSON-RPC Public API

Nodo exposes JSON-RPC 2.0 as the preferred public protocol API.

## Endpoint

```text
POST /rpc
Content-Type: application/json
```

REST routes may remain for operations, diagnostics, compatibility, and tests. Wallets, explorers, staking tools, and governance clients should prefer JSON-RPC.

## Design rules

- JSON-RPC and REST share the same node runtime where possible.
- Public methods must not bypass canonical validation.
- Requests must be size-bounded.
- Rate limiting should apply before dispatch.
- Runtime access should avoid observing torn state.
- Protocol methods should return deterministic data shapes.

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

## Transaction submission

Transaction-submission methods should receive self-contained signed transaction envelopes. Raw payloads without required public-key/signature material must be rejected.
