# Nodo Development Mode

Nodo currently runs localnet block production with an explicit temporary local
signature provider. The provider is deterministic and not production
cryptography.

Development mode exists so the runtime can be tested end to end:

```text
init -> keys create -> tx submit -> block produce -> node reload -> chain audit
```

## Boundaries

- Local signatures currently use the development algorithm behind
  `LocalSignatureProvider`.
- CLI commands with `demo` in the name are deprecated compatibility aliases.
- Runtime block votes are produced through `Signer`; no private key is derived
  from a public key inside runtime.
- The code has audited-provider interfaces, but no production Ed25519, ECDSA or
  post-quantum provider is bundled yet.

## Not Production Consensus

Current localnet block production is not P2P consensus. It is a deterministic local
pipeline used to harden runtime persistence, finalization records, mempool
cleanup and state reload.
