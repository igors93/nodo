# Nodo Development Mode

Nodo currently runs localnet block production with real signature algorithms:
Ed25519 for user transactions and BLS12-381 for validator operations. Localnet
key material is deterministic and unencrypted, so the profile is still not
production key custody.

Development mode exists so the runtime can be tested end to end:

```text
init -> keys create -> tx submit -> block produce -> node reload -> chain audit
```

## Boundaries

- Local user signatures use OpenSSL Ed25519.
- Local validator signatures use blst BLS12-381.
- CLI commands with `demo` in the name are deprecated compatibility aliases.
- Runtime block votes are produced through `Signer`; no private key is derived
  from a public key inside runtime.
- The code has post-quantum provider interfaces, but no audited post-quantum
  provider is bundled yet.

## Not Production Consensus

Current localnet block production is not P2P consensus. It is a deterministic local
pipeline used to harden runtime persistence, finalization records, mempool
cleanup and state reload.
