# Key Management

Nodo currently supports local development keys and production-safety gates.

## Current Local Keys

Localnet uses:

- Ed25519 user signatures through OpenSSL;
- BLS12-381 validator signatures through blst;
- strict local key metadata parsing;
- atomic key file writes.

Local development keys are not production custody. They should not be used to protect real funds or run a production validator.

## Official Network Safety

Official network profiles require stronger key policy. Testnet-candidate rejects insecure localnet-only key material. Mainnet remains blocked until audited custody and production key providers exist.

## Implemented

- encrypted durable key storage: `KeyStore` encrypts private key material at
  rest through `KeyEncryptionService` (PBKDF2-HMAC-SHA256 derived key), and
  plaintext key files are refused on official networks by
  `ProductionKeySafetyGate`.

## Required Future Work

- audited production signing providers;
- hardware or custody integration boundaries (HSM interface);
- key rotation and revocation procedures;
- operator documentation for testnet and mainnet.
