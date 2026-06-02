# Hardened Protocol Foundation

This package hardens Nodo's local protocol foundation without presenting the
project as production mainnet software.

## Implemented boundaries

- Transactional block finalization: finalization now validates and stages chain
  and registry mutations before committing them to runtime state.
- Safer atomic file replacement: canonical files are no longer deleted before
  replacement bytes are available.
- Temporary write recovery: stale atomic-write temporary files can be
  quarantined for operator inspection.
- Safe quorum math: quorum threshold calculation rejects invalid parameters and
  avoids silent uint64 overflow.
- Account-aware mempool selection: block production can select only nonce-valid
  sender sequences from the current account state.
- Runtime transaction admission now supports contiguous future nonce queues
  instead of rejecting every second transaction from the same sender.
- Protocol safety gate: plaintext deterministic local keys are explicitly
  allowed only for local development networks.
- Additional CTest coverage and a sanitizer build script.

## Apply

```bash
python3 tools/apply_hardened_protocol_foundation.py
```

Then run the normal build and test commands from the repository root.
