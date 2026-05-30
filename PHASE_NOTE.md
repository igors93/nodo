# Cycle 6 implementation

This phase implements Cycle 6 in two fronts.

## Front A — genesis config and network parameters

New components:

```text
NetworkParameters
BootstrapValidatorConfig
GenesisConfig
GenesisBuilder
```

Nodo now has deterministic configuration for creating the initial chain and
bootstrap validator registry.

## Front B — node runtime skeleton with local peer manager

New components:

```text
LocalPeerManager
NodeRuntimeConfig
NodeRuntime
NodeRuntimeFactory
```

Nodo now has a local runtime object that binds chain state, validator registry,
mempool, finalization registry and P2P sync planning.

Recommended commit:

```bash
git commit -m "Add genesis config and node runtime skeleton"
```
