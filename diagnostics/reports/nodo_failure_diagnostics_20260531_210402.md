# Nodo failing test diagnostics

Generated at: `2026-05-31T21:04:03.831564+00:00`
Repository root: `/home/igor/Documentos/Python/nodo`
Build directory: `/home/igor/Documentos/Python/nodo/build/cmake`

## Failed test focus

- `app_CommandLineLocalFlowTests`
- `app_CommandLinePersistentMempoolTests`
- `app_CommandLineRuntimeBlockTests`
- `node_FinalizedBlockStoreTests`
- `node_RuntimeStateLoaderTests`

## Source audit

- FinalizedBlockStore version: `NODO_FINALIZED_BLOCK_V18`
- RuntimeStateLoader version: `NODO_FINALIZED_BLOCK_V18`
- Persisted fields: `217`
- Loader allowed fields: `216`
- Loader canonical fields: `216`
- Governance fields present: `True`
- Protection reward fields present: `True`
- Cryptographic slashing fields present: `True`
- Fee economics fields present: `True`
- Monetary fields present: `True`

### Persisted fields not accepted by RuntimeStateLoader

- `record.`

### Persisted fields not recreated canonically by RuntimeStateLoader

- `record.`

## Artifact audit

- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `14` key `constexpr const char* FINALIZED_BLOCK_VERSION `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `219` key `    const NodeDataDirectoryReadResult existingManifest `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `229` key `    if (existingManifest.manifest().genesisConfigId() !`
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `237` key `    const std::filesystem::path path `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `243` key `    const std::string contents `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `252` key `            const std::string existingContents `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `262` key `            const NodeDataDirectoryReadResult snapshot `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `294` key `        const NodeDataDirectoryReadResult snapshot `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `379` key `    const auto& records `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `389` key `    const std::vector<RewardDistribution>& rewards `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `393` key `        const std::string prefix `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `404` key `    const std::vector<LockedStakePosition>& lockedStakePositions `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `408` key `        const std::string prefix `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `419` key `    const std::vector<SecurityScoreRecord>& securityScoreRecords `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `423` key `        const std::string prefix `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `437` key `    const std::vector<ValidatorSecurityCheckpoint>& securityCheckpoints `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `441` key `        const std::string prefix `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `454` key `    const std::vector<ValidatorRiskAssessment>& riskAssessments `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `458` key `        const std::string prefix `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `473` key `    const std::vector<ValidatorContainmentDecision>& containmentDecisions `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `477` key `        const std::string prefix `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `491` key `    const std::vector<ValidatorNetworkPolicy>& networkPolicies `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `495` key `        const std::string prefix `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `511` key `    const MonetaryFirewallAudit& monetaryAudit `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `526` key `    const GenesisTreasurySnapshot& treasurySnapshot `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `537` key `    const ProtectionRewardBudget& protectionBudget `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `549` key `    const std::vector<ProtectionRewardGrant>& protectionGrants `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `553` key `        const std::string prefix `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `564` key `    const std::vector<ProtectionWorkRecord>& protectionWorkRecords `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `568` key `        const std::string prefix `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `584` key `    const ProtectionRewardSummary& protectionSummary `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `595` key `    const std::vector<ProtectionRewardSettlement>& protectionSettlements `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `599` key `        const std::string prefix `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `614` key `    const InflationEpochSnapshot& inflationEpoch `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `628` key `    const MintAuthorizationRecord& mintAuthorization `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `642` key `    const SupplyExpansionRecord& supplyExpansion `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `653` key `    const FeeEconomicBalance& feeBalance `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `664` key `    const FeeBurnRecord& feeBurn `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `674` key `    const TreasuryFeeRecord& treasuryFee `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `683` key `    const std::vector<SlashingEvidenceRecord>& evidenceRecords `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `698` key `    const std::vector<SlashingPreparationRecord>& preparationRecords `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `714` key `    const SlashingEvidenceSummary& evidenceSummary `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `725` key `    const std::vector<CryptographicSlashingEvidenceRecord>& cryptoEvidenceRecords `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `742` key `    const std::vector<StakePenaltyRecord>& stakePenaltyRecords `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `757` key `    const CryptographicSlashingSummary& cryptoSlashingSummary `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `768` key `    const GovernancePolicySnapshot& governancePolicy `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `778` key `    const std::vector<GovernanceActionGuard>& governanceGuards `
- `empty_value` in `/home/igor/Documentos/Python/nodo/src/node/FinalizedBlockStore.cpp` line `793` key `    const GovernanceSummary& governanceSummary `

## ctest results

### `/usr/bin/ctest -R ^app_CommandLineLocalFlowTests$ --output-on-failure -VV`

- Return code: `8`
- Duration: `0.055s`
- Classifications: `cli_flow_failure`

Failure windows:

```text
Checking test dependency graph...
Checking test dependency graph end
test 2
    Start 2: app_CommandLineLocalFlowTests

2: Test command: /home/igor/Documentos/Python/nodo/build/cmake/app_CommandLineLocalFlowTests
2: Working Directory: /home/igor/Documentos/Python/nodo/build/cmake
2: Test timeout computed to be: 10000000
2: Nodo command line local flow tests failed: Demo block should finalize the submitted transaction.
1/1 Test #2: app_CommandLineLocalFlowTests ....***Failed    0.03 sec
Nodo command line local flow tests failed: Demo block should finalize the submitted transaction.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.04 sec

```

```text
Checking test dependency graph end
test 2
    Start 2: app_CommandLineLocalFlowTests

2: Test command: /home/igor/Documentos/Python/nodo/build/cmake/app_CommandLineLocalFlowTests
2: Working Directory: /home/igor/Documentos/Python/nodo/build/cmake
2: Test timeout computed to be: 10000000
2: Nodo command line local flow tests failed: Demo block should finalize the submitted transaction.
1/1 Test #2: app_CommandLineLocalFlowTests ....***Failed    0.03 sec
Nodo command line local flow tests failed: Demo block should finalize the submitted transaction.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.04 sec

The following tests FAILED:
```

```text
test 2
    Start 2: app_CommandLineLocalFlowTests

2: Test command: /home/igor/Documentos/Python/nodo/build/cmake/app_CommandLineLocalFlowTests
2: Working Directory: /home/igor/Documentos/Python/nodo/build/cmake
2: Test timeout computed to be: 10000000
2: Nodo command line local flow tests failed: Demo block should finalize the submitted transaction.
1/1 Test #2: app_CommandLineLocalFlowTests ....***Failed    0.03 sec
Nodo command line local flow tests failed: Demo block should finalize the submitted transaction.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.04 sec

The following tests FAILED:
	  2 - app_CommandLineLocalFlowTests (Failed)
```

```text
1/1 Test #2: app_CommandLineLocalFlowTests ....***Failed    0.03 sec
Nodo command line local flow tests failed: Demo block should finalize the submitted transaction.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.04 sec

The following tests FAILED:
	  2 - app_CommandLineLocalFlowTests (Failed)

Errors while running CTest
```

```text
Nodo command line local flow tests failed: Demo block should finalize the submitted transaction.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.04 sec

The following tests FAILED:
	  2 - app_CommandLineLocalFlowTests (Failed)

Errors while running CTest
```

### `/usr/bin/ctest -R ^app_CommandLinePersistentMempoolTests$ --output-on-failure -VV`

- Return code: `8`
- Duration: `0.033s`
- Classifications: `cli_flow_failure`

Failure windows:

```text
Checking test dependency graph...
Checking test dependency graph end
test 3
    Start 3: app_CommandLinePersistentMempoolTests

3: Test command: /home/igor/Documentos/Python/nodo/build/cmake/app_CommandLinePersistentMempoolTests
3: Working Directory: /home/igor/Documentos/Python/nodo/build/cmake
3: Test timeout computed to be: 10000000
3: Nodo command line persistent mempool tests failed: Produce should finalize block from persistent mempool.
1/1 Test #3: app_CommandLinePersistentMempoolTests ...***Failed    0.02 sec
Nodo command line persistent mempool tests failed: Produce should finalize block from persistent mempool.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.02 sec

```

```text
Checking test dependency graph end
test 3
    Start 3: app_CommandLinePersistentMempoolTests

3: Test command: /home/igor/Documentos/Python/nodo/build/cmake/app_CommandLinePersistentMempoolTests
3: Working Directory: /home/igor/Documentos/Python/nodo/build/cmake
3: Test timeout computed to be: 10000000
3: Nodo command line persistent mempool tests failed: Produce should finalize block from persistent mempool.
1/1 Test #3: app_CommandLinePersistentMempoolTests ...***Failed    0.02 sec
Nodo command line persistent mempool tests failed: Produce should finalize block from persistent mempool.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.02 sec

The following tests FAILED:
```

```text
test 3
    Start 3: app_CommandLinePersistentMempoolTests

3: Test command: /home/igor/Documentos/Python/nodo/build/cmake/app_CommandLinePersistentMempoolTests
3: Working Directory: /home/igor/Documentos/Python/nodo/build/cmake
3: Test timeout computed to be: 10000000
3: Nodo command line persistent mempool tests failed: Produce should finalize block from persistent mempool.
1/1 Test #3: app_CommandLinePersistentMempoolTests ...***Failed    0.02 sec
Nodo command line persistent mempool tests failed: Produce should finalize block from persistent mempool.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.02 sec

The following tests FAILED:
	  3 - app_CommandLinePersistentMempoolTests (Failed)
```

```text
1/1 Test #3: app_CommandLinePersistentMempoolTests ...***Failed    0.02 sec
Nodo command line persistent mempool tests failed: Produce should finalize block from persistent mempool.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.02 sec

The following tests FAILED:
	  3 - app_CommandLinePersistentMempoolTests (Failed)

Errors while running CTest
```

```text
Nodo command line persistent mempool tests failed: Produce should finalize block from persistent mempool.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.02 sec

The following tests FAILED:
	  3 - app_CommandLinePersistentMempoolTests (Failed)

Errors while running CTest
```

### `/usr/bin/ctest -R ^app_CommandLineRuntimeBlockTests$ --output-on-failure -VV`

- Return code: `8`
- Duration: `0.03s`
- Classifications: `cli_flow_failure`

Failure windows:

```text
Checking test dependency graph...
Checking test dependency graph end
test 4
    Start 4: app_CommandLineRuntimeBlockTests

4: Test command: /home/igor/Documentos/Python/nodo/build/cmake/app_CommandLineRuntimeBlockTests
4: Working Directory: /home/igor/Documentos/Python/nodo/build/cmake
4: Test timeout computed to be: 10000000
4: Nodo command line runtime block tests failed: block produce should finalize and persist block height 1.
1/1 Test #4: app_CommandLineRuntimeBlockTests ...***Failed    0.02 sec
Nodo command line runtime block tests failed: block produce should finalize and persist block height 1.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.02 sec

```

```text
Checking test dependency graph end
test 4
    Start 4: app_CommandLineRuntimeBlockTests

4: Test command: /home/igor/Documentos/Python/nodo/build/cmake/app_CommandLineRuntimeBlockTests
4: Working Directory: /home/igor/Documentos/Python/nodo/build/cmake
4: Test timeout computed to be: 10000000
4: Nodo command line runtime block tests failed: block produce should finalize and persist block height 1.
1/1 Test #4: app_CommandLineRuntimeBlockTests ...***Failed    0.02 sec
Nodo command line runtime block tests failed: block produce should finalize and persist block height 1.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.02 sec

The following tests FAILED:
```

```text
test 4
    Start 4: app_CommandLineRuntimeBlockTests

4: Test command: /home/igor/Documentos/Python/nodo/build/cmake/app_CommandLineRuntimeBlockTests
4: Working Directory: /home/igor/Documentos/Python/nodo/build/cmake
4: Test timeout computed to be: 10000000
4: Nodo command line runtime block tests failed: block produce should finalize and persist block height 1.
1/1 Test #4: app_CommandLineRuntimeBlockTests ...***Failed    0.02 sec
Nodo command line runtime block tests failed: block produce should finalize and persist block height 1.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.02 sec

The following tests FAILED:
	  4 - app_CommandLineRuntimeBlockTests (Failed)
```

```text
1/1 Test #4: app_CommandLineRuntimeBlockTests ...***Failed    0.02 sec
Nodo command line runtime block tests failed: block produce should finalize and persist block height 1.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.02 sec

The following tests FAILED:
	  4 - app_CommandLineRuntimeBlockTests (Failed)

Errors while running CTest
```

```text
Nodo command line runtime block tests failed: block produce should finalize and persist block height 1.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.02 sec

The following tests FAILED:
	  4 - app_CommandLineRuntimeBlockTests (Failed)

Errors while running CTest
```

### `/usr/bin/ctest -R ^node_FinalizedBlockStoreTests$ --output-on-failure -VV`

- Return code: `0`
- Duration: `0.066s`
- Classifications: `none`

### `/usr/bin/ctest -R ^node_RuntimeStateLoaderTests$ --output-on-failure -VV`

- Return code: `8`
- Duration: `0.058s`
- Classifications: `none`

Failure windows:

```text
Checking test dependency graph...
Checking test dependency graph end
test 54
    Start 54: node_RuntimeStateLoaderTests

54: Test command: /home/igor/Documentos/Python/nodo/build/cmake/node_RuntimeStateLoaderTests
54: Working Directory: /home/igor/Documentos/Python/nodo/build/cmake
54: Test timeout computed to be: 10000000
54: Nodo runtime state loader tests failed: Runtime should load from persisted data directory.
1/1 Test #54: node_RuntimeStateLoaderTests .....***Failed    0.04 sec
Nodo runtime state loader tests failed: Runtime should load from persisted data directory.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.04 sec

```

```text
Checking test dependency graph end
test 54
    Start 54: node_RuntimeStateLoaderTests

54: Test command: /home/igor/Documentos/Python/nodo/build/cmake/node_RuntimeStateLoaderTests
54: Working Directory: /home/igor/Documentos/Python/nodo/build/cmake
54: Test timeout computed to be: 10000000
54: Nodo runtime state loader tests failed: Runtime should load from persisted data directory.
1/1 Test #54: node_RuntimeStateLoaderTests .....***Failed    0.04 sec
Nodo runtime state loader tests failed: Runtime should load from persisted data directory.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.04 sec

The following tests FAILED:
```

```text
test 54
    Start 54: node_RuntimeStateLoaderTests

54: Test command: /home/igor/Documentos/Python/nodo/build/cmake/node_RuntimeStateLoaderTests
54: Working Directory: /home/igor/Documentos/Python/nodo/build/cmake
54: Test timeout computed to be: 10000000
54: Nodo runtime state loader tests failed: Runtime should load from persisted data directory.
1/1 Test #54: node_RuntimeStateLoaderTests .....***Failed    0.04 sec
Nodo runtime state loader tests failed: Runtime should load from persisted data directory.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.04 sec

The following tests FAILED:
	 54 - node_RuntimeStateLoaderTests (Failed)
```

```text
1/1 Test #54: node_RuntimeStateLoaderTests .....***Failed    0.04 sec
Nodo runtime state loader tests failed: Runtime should load from persisted data directory.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.04 sec

The following tests FAILED:
	 54 - node_RuntimeStateLoaderTests (Failed)

Errors while running CTest
```

```text
Nodo runtime state loader tests failed: Runtime should load from persisted data directory.


0% tests passed, 1 tests failed out of 1

Total Test time (real) =   0.04 sec

The following tests FAILED:
	 54 - node_RuntimeStateLoaderTests (Failed)

Errors while running CTest
```

## Recommended next checks

1. If a field appears in `persisted_not_allowed`, add it to RuntimeStateLoader allowed fields.
2. If a field appears in `persisted_not_canonical`, add it to RuntimeStateLoader canonical reconstruction.
3. If the failing output says `should persist`, compare FinalizedBlockStore fields with the test expectation.
4. If CLI tests fail after node tests, fix node persistence/reload first; CLI failures are often downstream.
