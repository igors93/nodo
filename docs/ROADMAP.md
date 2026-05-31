# Nodo Roadmap

## Near Term

- Add audited production provider validation around the current OpenSSL/blst suite.
- Add encrypted durable validator key management.
- Move from development text serialization to a stricter canonical binary format.
- Expand runtime reload validation to decode and audit quorum/finalization
  records, not only block payload integrity.
- Add a real networking runtime around the existing P2P message foundation.

## Runtime Hardening

- Keep rejecting malformed persistence data instead of skipping it.
- Keep file writes atomic and reject conflicting block/mempool artifacts.
- Add recovery handling for stale temporary files.
- Add migration/version handling for node data directory formats.

## Economics And Protection

- Connect useful protection work to production validator rewards.
- Add stake/penalty records once real identity and key management are available.
- Keep reward emission auditable through ledger records and rebuilders.
