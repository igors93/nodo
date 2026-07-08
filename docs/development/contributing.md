# Contributing

Nodo contributions should preserve protocol clarity, modularity, and auditability.

## Workflow

1. Create a focused branch.
2. Keep changes small and reviewable.
3. Update tests for protocol behavior.
4. Update documentation when behavior changes.
5. Run relevant tests before opening a pull request.

## Engineering principles

Implementation should follow solid software-engineering principles, including modularity, clarity, simplicity, security, low coupling, high cohesion, maintainability, testability, architectural consistency, robust error handling, and removal of dead or unnecessary legacy code.

## Protocol contribution rules

- Do not add hidden state shortcuts.
- Do not bypass canonical state transition.
- Do not silently accept malformed input.
- Do not make rewards, penalties, treasury execution, or governance decisions non-replayable.
- Do not introduce production claims without tests, docs, and review.

## Documentation rule

If a change alters protocol behavior, update the relevant documentation in this directory.
