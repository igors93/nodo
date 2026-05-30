# Nodo Development Mode

Nodo currently runs local block production with development-only signatures.

Development mode exists so the runtime can be tested end to end:

```text
init -> submit demo transaction -> produce/finalize block -> reload -> inspect
```

## Boundaries

- Development signatures use `DEVELOPMENT_FAKE_SIGNATURE`.
- CLI commands with `demo` in the name are local development commands.
- Runtime block votes are produced by a development signer until real validator
  key storage exists.
- The code has audited-provider interfaces, but no production Ed25519, ECDSA or
  post-quantum provider is bundled yet.

## Not Production Consensus

Current demo block production is not P2P consensus. It is a deterministic local
pipeline used to harden runtime persistence, finalization records, mempool
cleanup and state reload.
