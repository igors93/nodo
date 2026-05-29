# Nodo Economics and Protection Guides

Status: Project Guide Index  
Version: NODO-ECONOMICS-GUIDES-V5

This folder contains the main concept documents for Nodo's protection-based economic model.

Recommended reading order:

```text
1. PROOF_OF_PROTECTION.md
2. EMISSION_POLICY.md
3. VALIDATOR_SCORE.md
4. COIN_EXISTENCE.md
5. STAKE_AND_SECURITY.md
6. NETWORK_RISK_AND_ANTI_SYBIL.md
7. IMPLEMENTATION_START.md
8. LEDGER_RECORD_INTEGRATION.md
9. PROTECTION_STATE_REBUILDER.md
10. COIN_LOT_REGISTRY.md
```

Core idea:

```text
Nodo is a security-first blockchain where coins are born from verified protection work, every coin lot has provable existence, validator trust is recorded on-chain, and emission is dynamically controlled by transparent economic rules.
```

Implemented foundations so far:

```text
ValidationWorkRecord
ValidatorScoreRecord
EpochEmissionPolicy
ProtectionEpoch
GenesisRewardRecord
LedgerRecord integration for protection economics records
ProtectionEconomicsState
ProtectionEconomicsRebuilder
CoinLotVerificationResult
CoinLotRegistry
CoinLotRegistryRebuilder
```
