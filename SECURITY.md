# Security Policy

Nodo is a pre-mainnet, security-focused blockchain protocol project. The current repository is intended for development, review, and testnet-candidate preparation. It is not production mainnet software.

## Supported Versions

No production release line is currently supported. Security fixes should target the active `main` branch unless a future release policy says otherwise.

## Reporting a Vulnerability

If you discover a vulnerability, do not publish exploit details before maintainers have time to review and respond.

Preferred reporting path:

1. Open a private GitHub security advisory if repository permissions allow it.
2. If private advisory reporting is not available, contact the repository owner through GitHub and provide a minimal, responsible summary first.
3. Include reproduction steps, affected commit, expected impact, and whether private keys, funds, or persisted state could be affected.

## Scope

Important areas include:

- consensus safety;
- storage/reload canonical validation;
- transaction signature and admission validation;
- treasury execution evidence;
- governance vote evidence and lifecycle audit;
- slashing evidence and penalty idempotency;
- P2P message validation and rate limiting;
- key storage and production key safety gates.

## Current Security Status

- Mainnet is blocked.
- Localnet keys are not production custody.
- Testnet-candidate is still being hardened.
- No external audit is claimed.

See [Security Model](docs/security/security-model.md) and [Threat Model](docs/security/threat-model.md).
