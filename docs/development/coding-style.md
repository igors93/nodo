# Coding Style

Nodo is C++20 and uses focused modules with explicit validation.

## Style Principles

- Prefer clear data types over implicit strings for protocol concepts.
- Keep validation close to the model it protects.
- Keep consensus separate from transport.
- Keep treasury economics separate from gossip.
- Keep governance evidence separate from CLI orchestration.
- Reject unknown schema versions and unexpected persisted fields.
- Use deterministic proofs and canonical ordering for auditable records.

## Comments

Code comments should be in English and explain security, audit, or maintenance reasoning. Avoid comments that restate obvious code.

## Tests

Tests should prove invariants:

- malformed data is rejected;
- valid records round-trip canonically;
- replay or duplicate evidence is rejected;
- rebuilt state matches stored state;
- production paths cannot bypass official validation.
