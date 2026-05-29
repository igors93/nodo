# Audited signature provider boundary phase

This phase adds the audited signature provider integration boundary.

In simple terms:

```text
No fake provider is marked as real.
Nodo now has the gate where a real audited Ed25519/ECDSA provider will plug in.
```

Recommended commit:

```bash
git commit -m "Add audited signature provider boundary"
```

Next phase:

```text
Connect real audited signature provider implementation
```
