# Cycle 3 implementation

This phase implements Cycle 3 in two fronts.

## Front A — validator vote records and quorum certificate

New components:

```text
ValidatorVoteRecord
QuorumCertificate
QuorumCertificateBuilder
```

This gives Nodo the first consensus voting primitive.

## Front B — mempool admission and transaction queue

New components:

```text
MempoolConfig
MempoolEntry
MempoolAdmissionResult
Mempool
```

This gives Nodo a deterministic transaction queue before block production.

Recommended commit:

```bash
git commit -m "Add validator votes quorum certificate and mempool"
```
