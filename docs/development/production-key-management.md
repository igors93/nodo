# Production key management: key rotation and external signing

Nodo keeps user ownership keys and validator operation keys separated. This
module adds a protocol transaction for rotating validator operation keys and
wires validator proposal/vote signing through the `Signer` boundary so a node can
move from in-process signing to an external signer/HSM path without changing
consensus code.

## Validator key rotation

`VALIDATOR_KEY_ROTATE` is an owner-signed transaction. The transaction is signed
by the validator owner address and targets the old validator address. Its payload
contains the new validator public key, metadata hash, activation epoch and an
operator reason hash.

When executed, the protocol:

1. verifies that the sender is the registered owner of the old validator;
2. verifies that the new public key derives a fresh validator address;
3. moves the validator registry entry to the new validator address;
4. preserves owner, stake, jail status and consensus weight;
5. moves staking state and lifecycle records to the new validator address;
6. rejects rotation for exited, deactivated or exit-requested validators.

This keeps the validator identity/key binding deterministic while still allowing
operators to replace compromised or retiring validator operation keys before
mainnet.

## External signer / HSM boundary

Validator proposals and votes now use `Signer::signValidatorPayload()`. The
normal local path still signs with the local key pair. If an out-of-process
signer is attached, proposal and vote signatures are delegated through the
external signing boundary and protected by signer watermarks.

The signer boundary is intentionally domain-specific:

- `VALIDATOR_BLOCK_PROPOSAL` for block proposals;
- `VALIDATOR_VOTE` for consensus votes.

This prevents generic private-key access from leaking into consensus code and
keeps future HSM integration focused on exact protocol payloads.

## Operational rule

Local plaintext validator keys are still suitable only for localnet/development.
Public testnet and future mainnet operation should use encrypted keys and then
move toward an external signer/HSM process before any production claim.
