# Coding Style

Nodo code should be clear, modular, and easy to audit.

## Style principles

- Prefer small, focused types and functions.
- Keep protocol validation explicit.
- Use clear names for safety boundaries.
- Avoid hidden side effects in validation code.
- Return structured errors when possible.
- Keep serialization deterministic.
- Avoid duplicated protocol logic.
- Keep domain rules close to the domain they protect.

## Comments

Use comments to explain protocol reasoning, safety boundaries, and non-obvious invariants. Avoid comments that merely repeat the code.

## Tests

Every protocol rule should have tests for both acceptance and rejection paths.
