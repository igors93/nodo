#include "node/FinalizedBlockArtifactCodec.hpp"

#include "consensus/BlockFinalizer.hpp"
#include "consensus/QuorumCertificate.hpp"
#include "node/FinalizedArtifactSchema.hpp"
#include "node/FinalizedMonetarySectionCodec.hpp"
#include "serialization/BlockCodec.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"

#include <cstdint>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::string readTextFile(
    const std::filesystem::path& path
) {
    return storage::AtomicFile::readTextFile(path);
}

std::uint64_t parseU64Strict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }

    for (const char current : value) {
        if (current < '0' || current > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }

    std::size_t parsedSize = 0;
    const std::uint64_t parsed =
        static_cast<std::uint64_t>(
            std::stoull(
                value,
                &parsedSize
            )
        );

    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }

    return parsed;
}

std::uint16_t parseU16Strict(
    const std::string& value,
    const std::string& fieldName
) {
    const std::uint64_t parsed =
        parseU64Strict(
            value,
            fieldName
        );

    if (parsed > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument("Numeric field exceeds uint16 range: " + fieldName);
    }

    return static_cast<std::uint16_t>(parsed);
}



std::uint32_t parseU32Strict(
    const std::string& value,
    const std::string& fieldName
) {
    const std::uint64_t parsed =
        parseU64Strict(
            value,
            fieldName
        );

    if (parsed > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("Numeric field exceeds uint32 range: " + fieldName);
    }

    return static_cast<std::uint32_t>(parsed);
}

std::int64_t parseI64Strict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value.empty()) {
        throw std::invalid_argument("Empty numeric field: " + fieldName);
    }

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char current =
            value[index];

        if (current == '-' && index == 0 && value.size() > 1) {
            continue;
        }

        if (current < '0' || current > '9') {
            throw std::invalid_argument("Malformed numeric field: " + fieldName);
        }
    }

    std::size_t parsedSize = 0;
    const std::int64_t parsed =
        static_cast<std::int64_t>(
            std::stoll(
                value,
                &parsedSize
            )
        );

    if (parsedSize != value.size()) {
        throw std::invalid_argument("Malformed numeric field: " + fieldName);
    }

    return parsed;
}

utils::Amount parseAmountStrict(
    const std::string& value,
    const std::string& fieldName
) {
    return utils::Amount::fromRawUnits(
        parseI64Strict(
            value,
            fieldName
        )
    );
}

bool parseBoolStrict(
    const std::string& value,
    const std::string& fieldName
) {
    if (value == "true") {
        return true;
    }

    if (value == "false") {
        return false;
    }

    throw std::invalid_argument("Malformed boolean field: " + fieldName);
}

RewardDistribution parseRewardDistribution(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "reward." + std::to_string(index) + ".";

    RewardDistribution distribution(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        parseAmountStrict(
            document.requireField(prefix + "totalRewardRawUnits"),
            prefix + "totalRewardRawUnits"
        ),
        parseAmountStrict(
            document.requireField(prefix + "liquidRewardRawUnits"),
            prefix + "liquidRewardRawUnits"
        ),
        parseAmountStrict(
            document.requireField(prefix + "lockedRewardRawUnits"),
            prefix + "lockedRewardRawUnits"
        ),
        document.requireField(prefix + "reason")
    );

    if (!distribution.isValid()) {
        throw std::invalid_argument("Finalized block reward distribution is invalid.");
    }

    return distribution;
}

LockedStakePosition parseLockedStakePosition(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "lockedStake." + std::to_string(index) + ".";

    LockedStakePosition position(
        document.requireField(prefix + "ownerAddress"),
        parseAmountStrict(
            document.requireField(prefix + "amountRawUnits"),
            prefix + "amountRawUnits"
        ),
        parseU64Strict(
            document.requireField(prefix + "createdAtHeight"),
            prefix + "createdAtHeight"
        ),
        parseU64Strict(
            document.requireField(prefix + "unlockAtHeight"),
            prefix + "unlockAtHeight"
        ),
        parseBoolStrict(
            document.requireField(prefix + "slashable"),
            prefix + "slashable"
        ),
        document.requireField(prefix + "sourceRewardId")
    );

    if (!position.isValid()) {
        throw std::invalid_argument("Finalized block locked stake position is invalid.");
    }

    return position;
}

SecurityScoreRecord parseSecurityScoreRecord(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "securityScore." + std::to_string(index) + ".";

    SecurityScoreRecord record(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        parseU16Strict(
            document.requireField(prefix + "score"),
            prefix + "score"
        ),
        parseU16Strict(
            document.requireField(prefix + "lockedStakeScore"),
            prefix + "lockedStakeScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "participationScore"),
            prefix + "participationScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "maturityScore"),
            prefix + "maturityScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "penaltyScore"),
            prefix + "penaltyScore"
        ),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceLockedStakeId")
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Finalized block security score record is invalid.");
    }

    return record;
}
ValidatorSecurityCheckpoint parseSecurityCheckpoint(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "securityCheckpoint." + std::to_string(index) + ".";

    ValidatorSecurityCheckpoint checkpoint(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        parseU16Strict(
            document.requireField(prefix + "score"),
            prefix + "score"
        ),
        document.requireField(prefix + "band"),
        parseAmountStrict(
            document.requireField(prefix + "lockedStakeRawUnits"),
            prefix + "lockedStakeRawUnits"
        ),
        parseU64Strict(
            document.requireField(prefix + "securityScoreRecordCount"),
            prefix + "securityScoreRecordCount"
        ),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceDigest")
    );

    if (!checkpoint.isValid()) {
        throw std::invalid_argument("Finalized block security checkpoint is invalid.");
    }

    return checkpoint;
}
ValidatorRiskAssessment parseValidatorRiskAssessment(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "validatorRisk." + std::to_string(index) + ".";

    ValidatorRiskAssessment assessment(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        parseU16Strict(
            document.requireField(prefix + "score"),
            prefix + "score"
        ),
        document.requireField(prefix + "band"),
        parseAmountStrict(
            document.requireField(prefix + "lockedStakeRawUnits"),
            prefix + "lockedStakeRawUnits"
        ),
        parseU16Strict(
            document.requireField(prefix + "riskScore"),
            prefix + "riskScore"
        ),
        document.requireField(prefix + "riskLevel"),
        document.requireField(prefix + "recommendedAction"),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "checkpointDigest")
    );

    if (!assessment.isValid()) {
        throw std::invalid_argument("Finalized block validator risk assessment is invalid.");
    }

    return assessment;
}
ValidatorContainmentDecision parseValidatorContainmentDecision(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "validatorContainment." + std::to_string(index) + ".";

    ValidatorContainmentDecision decision(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        document.requireField(prefix + "riskLevel"),
        document.requireField(prefix + "recommendedAction"),
        document.requireField(prefix + "containmentMode"),
        document.requireField(prefix + "peerTrustState"),
        document.requireField(prefix + "networkAdmissionState"),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceRiskDigest")
    );

    if (!decision.isValid()) {
        throw std::invalid_argument("Finalized block validator containment decision is invalid.");
    }

    return decision;
}
ValidatorNetworkPolicy parseValidatorNetworkPolicy(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "validatorNetworkPolicy." + std::to_string(index) + ".";

    ValidatorNetworkPolicy policy(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        document.requireField(prefix + "containmentMode"),
        document.requireField(prefix + "peerTrustState"),
        document.requireField(prefix + "networkAdmissionState"),
        document.requireField(prefix + "connectionPolicy"),
        document.requireField(prefix + "messagePolicy"),
        document.requireField(prefix + "consensusPolicy"),
        parseBoolStrict(
            document.requireField(prefix + "requiresManualReview"),
            prefix + "requiresManualReview"
        ),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceContainmentDigest")
    );

    if (!policy.isValid()) {
        throw std::invalid_argument("Finalized block validator network policy is invalid.");
    }

    return policy;
}


MonetaryFirewallAudit parseMonetaryFirewallAudit(
    const serialization::KeyValueFileDocument& document
) {
    const std::string status =
        document.requireField("monetaryFirewallStatus");

    const SupplyLedgerSnapshot ledger(
        parseU64Strict(
            document.requireField("monetary.blockHeight"),
            "monetary.blockHeight"
        ),
        parseAmountStrict(
            document.requireField("monetary.supplyBeforeRawUnits"),
            "monetary.supplyBeforeRawUnits"
        ),
        parseAmountStrict(
            document.requireField("monetary.mintedRawUnits"),
            "monetary.mintedRawUnits"
        ),
        parseAmountStrict(
            document.requireField("monetary.burnedRawUnits"),
            "monetary.burnedRawUnits"
        ),
        parseAmountStrict(
            document.requireField("monetary.treasuryDeltaRawUnits"),
            "monetary.treasuryDeltaRawUnits"
        ),
        parseAmountStrict(
            document.requireField("monetary.supplyAfterRawUnits"),
            "monetary.supplyAfterRawUnits"
        )
    );

    MonetaryFirewallAudit audit(
        status,
        ledger,
        parseAmountStrict(
            document.requireField("monetary.annualMintLimitRawUnits"),
            "monetary.annualMintLimitRawUnits"
        ),
        parseAmountStrict(
            document.requireField("monetary.annualMintUsedBeforeRawUnits"),
            "monetary.annualMintUsedBeforeRawUnits"
        ),
        parseAmountStrict(
            document.requireField("monetary.annualMintUsedAfterRawUnits"),
            "monetary.annualMintUsedAfterRawUnits"
        ),
        document.requireField("monetary.policyId"),
        document.requireField("monetary.reason")
    );

    if (!audit.isValid()) {
        throw std::invalid_argument("Finalized block monetary firewall audit is invalid.");
    }

    return audit;
}


GenesisTreasurySnapshot parseGenesisTreasurySnapshot(
    const serialization::KeyValueFileDocument& document
) {
    GenesisTreasurySnapshot snapshot(
        document.requireField("genesisTreasuryStatus"),
        document.requireField("treasury.treasuryAddress"),
        parseU64Strict(
            document.requireField("treasury.blockHeight"),
            "treasury.blockHeight"
        ),
        parseAmountStrict(
            document.requireField("treasury.genesisTreasuryBalanceRawUnits"),
            "treasury.genesisTreasuryBalanceRawUnits"
        ),
        parseAmountStrict(
            document.requireField("treasury.protectedReserveRawUnits"),
            "treasury.protectedReserveRawUnits"
        ),
        parseAmountStrict(
            document.requireField("treasury.protectionBudgetRawUnits"),
            "treasury.protectionBudgetRawUnits"
        ),
        parseAmountStrict(
            document.requireField("treasury.availableBalanceRawUnits"),
            "treasury.availableBalanceRawUnits"
        ),
        document.requireField("treasury.reason")
    );

    if (!snapshot.isValid()) {
        throw std::invalid_argument("Finalized block genesis treasury snapshot is invalid.");
    }

    return snapshot;
}

ProtectionRewardBudget parseProtectionRewardBudget(
    const serialization::KeyValueFileDocument& document
) {
    ProtectionRewardBudget budget(
        document.requireField("protectionRewardBudgetStatus"),
        parseU64Strict(
            document.requireField("protectionBudget.blockHeight"),
            "protectionBudget.blockHeight"
        ),
        document.requireField("protectionBudget.treasuryAddress"),
        parseAmountStrict(
            document.requireField("protectionBudget.availableBudgetRawUnits"),
            "protectionBudget.availableBudgetRawUnits"
        ),
        parseAmountStrict(
            document.requireField("protectionBudget.plannedTotalRawUnits"),
            "protectionBudget.plannedTotalRawUnits"
        ),
        parseAmountStrict(
            document.requireField("protectionBudget.remainingBudgetRawUnits"),
            "protectionBudget.remainingBudgetRawUnits"
        ),
        parseU64Strict(
            document.requireField("protectionBudget.beneficiaryCount"),
            "protectionBudget.beneficiaryCount"
        ),
        document.requireField("protectionBudget.reason"),
        document.requireField("protectionBudget.sourceTreasuryDigest")
    );

    if (!budget.isValid()) {
        throw std::invalid_argument("Finalized block protection reward budget is invalid.");
    }

    return budget;
}

ProtectionRewardGrant parseProtectionRewardGrant(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "protectionGrant." + std::to_string(index) + ".";

    ProtectionRewardGrant grant(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        parseAmountStrict(
            document.requireField(prefix + "plannedRewardRawUnits"),
            prefix + "plannedRewardRawUnits"
        ),
        parseU16Strict(
            document.requireField(prefix + "securityScore"),
            prefix + "securityScore"
        ),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceBudgetDigest")
    );

    if (!grant.isValid()) {
        throw std::invalid_argument("Finalized block protection reward grant is invalid.");
    }

    return grant;
}


InflationEpochSnapshot parseInflationEpochSnapshot(
    const serialization::KeyValueFileDocument& document
) {
    InflationEpochSnapshot snapshot(
        document.requireField("inflationEpochStatus"),
        parseU64Strict(
            document.requireField("inflationEpoch.blockHeight"),
            "inflationEpoch.blockHeight"
        ),
        parseU64Strict(
            document.requireField("inflationEpoch.epochStartBlock"),
            "inflationEpoch.epochStartBlock"
        ),
        parseU64Strict(
            document.requireField("inflationEpoch.epochEndBlock"),
            "inflationEpoch.epochEndBlock"
        ),
        parseU32Strict(
            document.requireField("inflationEpoch.maxAnnualInflationBasisPoints"),
            "inflationEpoch.maxAnnualInflationBasisPoints"
        ),
        parseAmountStrict(
            document.requireField("inflationEpoch.baseSupplyRawUnits"),
            "inflationEpoch.baseSupplyRawUnits"
        ),
        parseAmountStrict(
            document.requireField("inflationEpoch.annualMintLimitRawUnits"),
            "inflationEpoch.annualMintLimitRawUnits"
        ),
        parseAmountStrict(
            document.requireField("inflationEpoch.mintedThisEpochRawUnits"),
            "inflationEpoch.mintedThisEpochRawUnits"
        ),
        parseAmountStrict(
            document.requireField("inflationEpoch.remainingMintCapacityRawUnits"),
            "inflationEpoch.remainingMintCapacityRawUnits"
        ),
        document.requireField("inflationEpoch.policyId"),
        document.requireField("inflationEpoch.reason")
    );

    if (!snapshot.isValid()) {
        throw std::invalid_argument("Finalized block inflation epoch snapshot is invalid.");
    }

    return snapshot;
}

MintAuthorizationRecord parseMintAuthorizationRecord(
    const serialization::KeyValueFileDocument& document
) {
    MintAuthorizationRecord authorization(
        document.requireField("mintAuthorizationStatus"),
        parseU64Strict(
            document.requireField("mintAuthorization.blockHeight"),
            "mintAuthorization.blockHeight"
        ),
        document.requireField("mintAuthorization.authorizationId"),
        parseAmountStrict(
            document.requireField("mintAuthorization.authorizedAmountRawUnits"),
            "mintAuthorization.authorizedAmountRawUnits"
        ),
        parseU64Strict(
            document.requireField("mintAuthorization.activationBlock"),
            "mintAuthorization.activationBlock"
        ),
        parseU64Strict(
            document.requireField("mintAuthorization.expirationBlock"),
            "mintAuthorization.expirationBlock"
        ),
        parseU32Strict(
            document.requireField("mintAuthorization.requiredApprovalBasisPoints"),
            "mintAuthorization.requiredApprovalBasisPoints"
        ),
        parseU64Strict(
            document.requireField("mintAuthorization.timelockBlocks"),
            "mintAuthorization.timelockBlocks"
        ),
        document.requireField("mintAuthorization.governanceDigest"),
        document.requireField("mintAuthorization.reason"),
        document.requireField("mintAuthorization.sourceEpochDigest")
    );

    if (!authorization.isValid()) {
        throw std::invalid_argument("Finalized block mint authorization record is invalid.");
    }

    return authorization;
}

SupplyExpansionRecord parseSupplyExpansionRecord(
    const serialization::KeyValueFileDocument& document
) {
    SupplyExpansionRecord expansion(
        document.requireField("supplyExpansionStatus"),
        parseU64Strict(
            document.requireField("supplyExpansion.blockHeight"),
            "supplyExpansion.blockHeight"
        ),
        parseAmountStrict(
            document.requireField("supplyExpansion.mintedAmountRawUnits"),
            "supplyExpansion.mintedAmountRawUnits"
        ),
        document.requireField("supplyExpansion.recipientAddress"),
        document.requireField("supplyExpansion.authorizationId"),
        document.requireField("supplyExpansion.policyId"),
        document.requireField("supplyExpansion.reason"),
        document.requireField("supplyExpansion.sourceAuthorizationDigest")
    );

    if (!expansion.isValid()) {
        throw std::invalid_argument("Finalized block supply expansion record is invalid.");
    }

    return expansion;
}


FeeEconomicBalance parseFeeEconomicBalance(
    const serialization::KeyValueFileDocument& document
) {
    FeeEconomicBalance balance(
        document.requireField("feeEconomicBalanceStatus"),
        parseU64Strict(
            document.requireField("feeBalance.blockHeight"),
            "feeBalance.blockHeight"
        ),
        parseAmountStrict(
            document.requireField("feeBalance.totalFeeRawUnits"),
            "feeBalance.totalFeeRawUnits"
        ),
        parseAmountStrict(
            document.requireField("feeBalance.validatorRewardRawUnits"),
            "feeBalance.validatorRewardRawUnits"
        ),
        parseAmountStrict(
            document.requireField("feeBalance.treasuryRawUnits"),
            "feeBalance.treasuryRawUnits"
        ),
        parseAmountStrict(
            document.requireField("feeBalance.burnRawUnits"),
            "feeBalance.burnRawUnits"
        ),
        document.requireField("feeBalance.policyId"),
        document.requireField("feeBalance.reason")
    );

    if (!balance.isValid()) {
        throw std::invalid_argument("Finalized block fee economic balance is invalid.");
    }

    return balance;
}

FeeBurnRecord parseFeeBurnRecord(
    const serialization::KeyValueFileDocument& document
) {
    FeeBurnRecord burn(
        document.requireField("feeBurnStatus"),
        parseU64Strict(
            document.requireField("feeBurn.blockHeight"),
            "feeBurn.blockHeight"
        ),
        parseAmountStrict(
            document.requireField("feeBurn.burnAmountRawUnits"),
            "feeBurn.burnAmountRawUnits"
        ),
        parseAmountStrict(
            document.requireField("feeBurn.supplyBeforeRawUnits"),
            "feeBurn.supplyBeforeRawUnits"
        ),
        parseAmountStrict(
            document.requireField("feeBurn.supplyAfterRawUnits"),
            "feeBurn.supplyAfterRawUnits"
        ),
        document.requireField("feeBurn.reason"),
        document.requireField("feeBurn.sourceFeeBalanceDigest")
    );

    if (!burn.isValid()) {
        throw std::invalid_argument("Finalized block fee burn record is invalid.");
    }

    return burn;
}

TreasuryFeeRecord parseTreasuryFeeRecord(
    const serialization::KeyValueFileDocument& document
) {
    TreasuryFeeRecord treasuryFee(
        document.requireField("treasuryFeeStatus"),
        parseU64Strict(
            document.requireField("treasuryFee.blockHeight"),
            "treasuryFee.blockHeight"
        ),
        document.requireField("treasuryFee.treasuryAddress"),
        parseAmountStrict(
            document.requireField("treasuryFee.treasuryAmountRawUnits"),
            "treasuryFee.treasuryAmountRawUnits"
        ),
        document.requireField("treasuryFee.reason"),
        document.requireField("treasuryFee.sourceFeeBalanceDigest")
    );

    if (!treasuryFee.isValid()) {
        throw std::invalid_argument("Finalized block treasury fee record is invalid.");
    }

    return treasuryFee;
}


ProtectionWorkRecord parseProtectionWorkRecord(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "protectionWork." + std::to_string(index) + ".";

    ProtectionWorkRecord record(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        parseU16Strict(
            document.requireField(prefix + "uptimeScore"),
            prefix + "uptimeScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "correctVoteScore"),
            prefix + "correctVoteScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "attackDetectionScore"),
            prefix + "attackDetectionScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "auditContributionScore"),
            prefix + "auditContributionScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "securityScore"),
            prefix + "securityScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "riskPenaltyScore"),
            prefix + "riskPenaltyScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "totalWorkScore"),
            prefix + "totalWorkScore"
        ),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceSecurityDigest")
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Finalized block protection work record is invalid.");
    }

    return record;
}

ProtectionRewardSummary parseProtectionRewardSummary(
    const serialization::KeyValueFileDocument& document
) {
    ProtectionRewardSummary summary(
        document.requireField("protectionRewardSummaryStatus"),
        parseU64Strict(
            document.requireField("protectionSummary.blockHeight"),
            "protectionSummary.blockHeight"
        ),
        parseAmountStrict(
            document.requireField("protectionSummary.plannedTotalRawUnits"),
            "protectionSummary.plannedTotalRawUnits"
        ),
        parseAmountStrict(
            document.requireField("protectionSummary.earnedTotalRawUnits"),
            "protectionSummary.earnedTotalRawUnits"
        ),
        parseAmountStrict(
            document.requireField("protectionSummary.deferredTotalRawUnits"),
            "protectionSummary.deferredTotalRawUnits"
        ),
        parseU64Strict(
            document.requireField("protectionSummary.beneficiaryCount"),
            "protectionSummary.beneficiaryCount"
        ),
        document.requireField("protectionSummary.reason"),
        document.requireField("protectionSummary.sourceBudgetDigest")
    );

    if (!summary.isValid()) {
        throw std::invalid_argument("Finalized block protection reward summary is invalid.");
    }

    return summary;
}

ProtectionRewardSettlement parseProtectionRewardSettlement(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix =
        "protectionSettlement." + std::to_string(index) + ".";

    ProtectionRewardSettlement settlement(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(
            document.requireField(prefix + "blockHeight"),
            prefix + "blockHeight"
        ),
        parseAmountStrict(
            document.requireField(prefix + "plannedRewardRawUnits"),
            prefix + "plannedRewardRawUnits"
        ),
        parseAmountStrict(
            document.requireField(prefix + "earnedRewardRawUnits"),
            prefix + "earnedRewardRawUnits"
        ),
        parseAmountStrict(
            document.requireField(prefix + "deferredRewardRawUnits"),
            prefix + "deferredRewardRawUnits"
        ),
        parseU16Strict(
            document.requireField(prefix + "workScore"),
            prefix + "workScore"
        ),
        parseU16Strict(
            document.requireField(prefix + "securityScore"),
            prefix + "securityScore"
        ),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceGrantDigest"),
        document.requireField(prefix + "sourceWorkDigest")
    );

    if (!settlement.isValid()) {
        throw std::invalid_argument("Finalized block protection reward settlement is invalid.");
    }

    return settlement;
}


SlashingEvidenceRecord parseSlashingEvidenceRecord(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix = "slashingEvidence." + std::to_string(index) + ".";

    SlashingEvidenceRecord record(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(document.requireField(prefix + "blockHeight"), prefix + "blockHeight"),
        document.requireField(prefix + "evidenceType"),
        parseU16Strict(document.requireField(prefix + "severityScore"), prefix + "severityScore"),
        parseBoolStrict(document.requireField(prefix + "slashable"), prefix + "slashable"),
        document.requireField(prefix + "recommendedAction"),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceSecurityDigest")
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Finalized block slashing evidence record is invalid.");
    }

    return record;
}

SlashingPreparationRecord parseSlashingPreparationRecord(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix = "slashingPreparation." + std::to_string(index) + ".";

    SlashingPreparationRecord record(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(document.requireField(prefix + "blockHeight"), prefix + "blockHeight"),
        parseU64Strict(document.requireField(prefix + "evidenceCount"), prefix + "evidenceCount"),
        parseU64Strict(document.requireField(prefix + "slashableEvidenceCount"), prefix + "slashableEvidenceCount"),
        parseU16Strict(document.requireField(prefix + "maxSeverityScore"), prefix + "maxSeverityScore"),
        parseAmountStrict(document.requireField(prefix + "preparedPenaltyRawUnits"), prefix + "preparedPenaltyRawUnits"),
        document.requireField(prefix + "enforcementAction"),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceEvidenceDigest")
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Finalized block slashing preparation record is invalid.");
    }

    return record;
}

SlashingEvidenceSummary parseSlashingEvidenceSummary(
    const serialization::KeyValueFileDocument& document
) {
    SlashingEvidenceSummary summary(
        document.requireField("slashingEvidenceSummaryStatus"),
        parseU64Strict(document.requireField("slashingSummary.blockHeight"), "slashingSummary.blockHeight"),
        parseU64Strict(document.requireField("slashingSummary.evidenceCount"), "slashingSummary.evidenceCount"),
        parseU64Strict(document.requireField("slashingSummary.slashableEvidenceCount"), "slashingSummary.slashableEvidenceCount"),
        parseU16Strict(document.requireField("slashingSummary.maxSeverityScore"), "slashingSummary.maxSeverityScore"),
        parseAmountStrict(document.requireField("slashingSummary.preparedPenaltyTotalRawUnits"), "slashingSummary.preparedPenaltyTotalRawUnits"),
        document.requireField("slashingSummary.reason"),
        document.requireField("slashingSummary.sourcePreparationDigest")
    );

    if (!summary.isValid()) {
        throw std::invalid_argument("Finalized block slashing evidence summary is invalid.");
    }

    return summary;
}

CryptographicSlashingEvidenceRecord parseCryptographicSlashingEvidenceRecord(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix = "cryptographicSlashingEvidence." + std::to_string(index) + ".";

    CryptographicSlashingEvidenceRecord record(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(document.requireField(prefix + "blockHeight"), prefix + "blockHeight"),
        parseU64Strict(document.requireField(prefix + "round"), prefix + "round"),
        document.requireField(prefix + "evidenceType"),
        parseU16Strict(document.requireField(prefix + "severityScore"), prefix + "severityScore"),
        parseU32Strict(document.requireField(prefix + "penaltyBasisPoints"), prefix + "penaltyBasisPoints"),
        document.requireField(prefix + "firstVoteDigest"),
        document.requireField(prefix + "secondVoteDigest"),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceEvidenceDigest")
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Finalized block cryptographic slashing evidence is invalid.");
    }

    return record;
}

StakePenaltyRecord parseStakePenaltyRecord(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix = "stakePenalty." + std::to_string(index) + ".";

    StakePenaltyRecord record(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(document.requireField(prefix + "blockHeight"), prefix + "blockHeight"),
        parseAmountStrict(document.requireField(prefix + "lockedStakeBeforeRawUnits"), prefix + "lockedStakeBeforeRawUnits"),
        parseAmountStrict(document.requireField(prefix + "penaltyAmountRawUnits"), prefix + "penaltyAmountRawUnits"),
        parseAmountStrict(document.requireField(prefix + "lockedStakeAfterRawUnits"), prefix + "lockedStakeAfterRawUnits"),
        parseU64Strict(document.requireField(prefix + "evidenceCount"), prefix + "evidenceCount"),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceEvidenceDigest")
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Finalized block stake penalty record is invalid.");
    }

    return record;
}

CryptographicSlashingSummary parseCryptographicSlashingSummary(
    const serialization::KeyValueFileDocument& document
) {
    CryptographicSlashingSummary summary(
        document.requireField("cryptographicSlashingSummaryStatus"),
        parseU64Strict(document.requireField("cryptographicSlashingSummary.blockHeight"), "cryptographicSlashingSummary.blockHeight"),
        parseU64Strict(document.requireField("cryptographicSlashingSummary.evidenceCount"), "cryptographicSlashingSummary.evidenceCount"),
        parseU64Strict(document.requireField("cryptographicSlashingSummary.slashableEvidenceCount"), "cryptographicSlashingSummary.slashableEvidenceCount"),
        parseU16Strict(document.requireField("cryptographicSlashingSummary.maxSeverityScore"), "cryptographicSlashingSummary.maxSeverityScore"),
        parseAmountStrict(document.requireField("cryptographicSlashingSummary.penaltyTotalRawUnits"), "cryptographicSlashingSummary.penaltyTotalRawUnits"),
        document.requireField("cryptographicSlashingSummary.reason"),
        document.requireField("cryptographicSlashingSummary.sourcePenaltyDigest")
    );

    if (!summary.isValid()) {
        throw std::invalid_argument("Finalized block cryptographic slashing summary is invalid.");
    }

    return summary;
}


GovernancePolicySnapshot parseGovernancePolicySnapshot(
    const serialization::KeyValueFileDocument& document
) {
    GovernancePolicySnapshot policy(
        document.requireField("governancePolicyStatus"),
        parseU64Strict(document.requireField("governancePolicy.blockHeight"), "governancePolicy.blockHeight"),
        parseU32Strict(document.requireField("governancePolicy.requiredApprovalBasisPoints"), "governancePolicy.requiredApprovalBasisPoints"),
        parseU64Strict(document.requireField("governancePolicy.timelockBlocks"), "governancePolicy.timelockBlocks"),
        parseU64Strict(document.requireField("governancePolicy.activationDelayBlocks"), "governancePolicy.activationDelayBlocks"),
        document.requireField("governancePolicy.policyId"),
        document.requireField("governancePolicy.reason")
    );

    if (!policy.isValid()) {
        throw std::invalid_argument("Finalized block governance policy is invalid.");
    }

    return policy;
}

GovernanceActionGuard parseGovernanceActionGuard(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix = "governanceGuard." + std::to_string(index) + ".";

    GovernanceActionGuard guard(
        document.requireField(prefix + "actionType"),
        document.requireField(prefix + "status"),
        parseU64Strict(document.requireField(prefix + "blockHeight"), prefix + "blockHeight"),
        document.requireField(prefix + "protectedResource"),
        parseU32Strict(document.requireField(prefix + "requiredApprovalBasisPoints"), prefix + "requiredApprovalBasisPoints"),
        parseU64Strict(document.requireField(prefix + "timelockBlocks"), prefix + "timelockBlocks"),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourcePolicyDigest")
    );

    if (!guard.isValid()) {
        throw std::invalid_argument("Finalized block governance action guard is invalid.");
    }

    return guard;
}

GovernanceSummary parseGovernanceSummary(
    const serialization::KeyValueFileDocument& document
) {
    GovernanceSummary summary(
        document.requireField("governanceSummaryStatus"),
        parseU64Strict(document.requireField("governanceSummary.blockHeight"), "governanceSummary.blockHeight"),
        parseU64Strict(document.requireField("governanceSummary.guardCount"), "governanceSummary.guardCount"),
        parseU64Strict(document.requireField("governanceSummary.activeProposalCount"), "governanceSummary.activeProposalCount"),
        parseU64Strict(document.requireField("governanceSummary.approvedProposalCount"), "governanceSummary.approvedProposalCount"),
        parseU64Strict(document.requireField("governanceSummary.executableProposalCount"), "governanceSummary.executableProposalCount"),
        parseU64Strict(document.requireField("governanceSummary.executedProposalCount"), "governanceSummary.executedProposalCount"),
        document.requireField("governanceSummary.reason"),
        document.requireField("governanceSummary.sourceGuardDigest")
    );

    if (!summary.isValid()) {
        throw std::invalid_argument("Finalized block governance summary is invalid.");
    }

    return summary;
}

ValidatorLifecycleRecord parseValidatorLifecycleRecord(
    const serialization::KeyValueFileDocument& document,
    std::size_t index
) {
    const std::string prefix = "validatorLifecycle." + std::to_string(index) + ".";

    ValidatorLifecycleRecord record(
        document.requireField(prefix + "validatorAddress"),
        parseU64Strict(document.requireField(prefix + "blockHeight"), prefix + "blockHeight"),
        parseU64Strict(document.requireField(prefix + "epochIndex"), prefix + "epochIndex"),
        document.requireField(prefix + "lifecycleStatus"),
        parseAmountStrict(document.requireField(prefix + "lockedStakeRawUnits"), prefix + "lockedStakeRawUnits"),
        parseAmountStrict(document.requireField(prefix + "earnedRewardRawUnits"), prefix + "earnedRewardRawUnits"),
        parseAmountStrict(document.requireField(prefix + "slashingPenaltyRawUnits"), prefix + "slashingPenaltyRawUnits"),
        parseU16Strict(document.requireField(prefix + "securityScore"), prefix + "securityScore"),
        document.requireField(prefix + "reason"),
        document.requireField(prefix + "sourceDigest")
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Finalized block validator lifecycle record is invalid.");
    }

    return record;
}

EpochAccountingRecord parseEpochAccountingRecord(
    const serialization::KeyValueFileDocument& document
) {
    EpochAccountingRecord record(
        document.requireField("epochAccountingStatus"),
        parseU64Strict(document.requireField("epochAccounting.blockHeight"), "epochAccounting.blockHeight"),
        parseU64Strict(document.requireField("epochAccounting.epochIndex"), "epochAccounting.epochIndex"),
        parseU64Strict(document.requireField("epochAccounting.epochStartBlock"), "epochAccounting.epochStartBlock"),
        parseU64Strict(document.requireField("epochAccounting.epochEndBlock"), "epochAccounting.epochEndBlock"),
        parseU64Strict(document.requireField("epochAccounting.validatorCount"), "epochAccounting.validatorCount"),
        parseU64Strict(document.requireField("epochAccounting.activeValidatorCount"), "epochAccounting.activeValidatorCount"),
        parseAmountStrict(document.requireField("epochAccounting.totalLockedStakeRawUnits"), "epochAccounting.totalLockedStakeRawUnits"),
        parseAmountStrict(document.requireField("epochAccounting.totalEarnedRewardsRawUnits"), "epochAccounting.totalEarnedRewardsRawUnits"),
        parseAmountStrict(document.requireField("epochAccounting.totalSlashingPenaltiesRawUnits"), "epochAccounting.totalSlashingPenaltiesRawUnits"),
        document.requireField("epochAccounting.reason"),
        document.requireField("epochAccounting.sourceLifecycleDigest")
    );

    if (!record.isValid()) {
        throw std::invalid_argument("Finalized block epoch accounting record is invalid.");
    }

    return record;
}

ValidatorLifecycleSummary parseValidatorLifecycleSummary(
    const serialization::KeyValueFileDocument& document
) {
    ValidatorLifecycleSummary summary(
        document.requireField("validatorLifecycleSummaryStatus"),
        parseU64Strict(document.requireField("validatorLifecycleSummary.blockHeight"), "validatorLifecycleSummary.blockHeight"),
        parseU64Strict(document.requireField("validatorLifecycleSummary.epochIndex"), "validatorLifecycleSummary.epochIndex"),
        parseU64Strict(document.requireField("validatorLifecycleSummary.activeValidatorCount"), "validatorLifecycleSummary.activeValidatorCount"),
        parseU64Strict(document.requireField("validatorLifecycleSummary.jailedValidatorCount"), "validatorLifecycleSummary.jailedValidatorCount"),
        parseU64Strict(document.requireField("validatorLifecycleSummary.slashedValidatorCount"), "validatorLifecycleSummary.slashedValidatorCount"),
        parseAmountStrict(document.requireField("validatorLifecycleSummary.totalLockedStakeRawUnits"), "validatorLifecycleSummary.totalLockedStakeRawUnits"),
        parseAmountStrict(document.requireField("validatorLifecycleSummary.totalEarnedRewardsRawUnits"), "validatorLifecycleSummary.totalEarnedRewardsRawUnits"),
        parseAmountStrict(document.requireField("validatorLifecycleSummary.totalSlashingPenaltiesRawUnits"), "validatorLifecycleSummary.totalSlashingPenaltiesRawUnits"),
        document.requireField("validatorLifecycleSummary.reason"),
        document.requireField("validatorLifecycleSummary.sourceEpochDigest")
    );

    if (!summary.isValid()) {
        throw std::invalid_argument("Finalized block validator lifecycle summary is invalid.");
    }

    return summary;
}

} // namespace

FinalizedBlockArtifact::FinalizedBlockArtifact()
    : m_block(std::nullopt),
      m_postStateRoot(""),
      m_totalFee(),
      m_rewardDistributions(),
      m_lockedStakePositions(),
      m_securityScoreRecords(),
      m_securityCheckpoints(),
      m_validatorRiskAssessments(),
      m_validatorContainmentDecisions(),
      m_validatorNetworkPolicies(),
      m_monetaryFirewallAudit(MonetaryFirewallAudit::notEvaluated()),
      m_genesisTreasurySnapshot(GenesisTreasurySnapshot::notEvaluated()),
      m_protectionRewardBudget(ProtectionRewardBudget::notEvaluated()),
      m_protectionRewardGrants(),
      m_protectionWorkRecords(),
      m_protectionRewardSummary(ProtectionRewardSummary::notEvaluated()),
      m_protectionRewardSettlements(),
      m_inflationEpochSnapshot(InflationEpochSnapshot::notEvaluated()),
      m_mintAuthorizationRecord(),
      m_supplyExpansionRecord(),
      m_feeEconomicBalance(FeeEconomicBalance::notEvaluated()),
      m_feeBurnRecord(FeeBurnRecord::notEvaluated()),
      m_treasuryFeeRecord(TreasuryFeeRecord::notEvaluated()),
      m_slashingEvidenceRecords(),
      m_slashingPreparationRecords(),
      m_slashingEvidenceSummary(SlashingEvidenceSummary::notEvaluated()),
      m_cryptographicSlashingEvidenceRecords(),
      m_stakePenaltyRecords(),
      m_cryptographicSlashingSummary(CryptographicSlashingSummary::notEvaluated()),
      m_governancePolicySnapshot(GovernancePolicySnapshot::notEvaluated()),
      m_governanceActionGuards(),
      m_governanceSummary(GovernanceSummary::notEvaluated()),
      m_validatorLifecycleRecords(),
      m_epochAccountingRecord(EpochAccountingRecord::notEvaluated()),
      m_validatorLifecycleSummary(ValidatorLifecycleSummary::notEvaluated()),
      m_quorumCertificate(),
      m_finalizedRecord() {}

FinalizedBlockArtifact::FinalizedBlockArtifact(
    core::Block block,
    std::string postStateRoot,
    utils::Amount totalFee,
    std::vector<RewardDistribution> rewardDistributions,
    std::vector<LockedStakePosition> lockedStakePositions,
    std::vector<SecurityScoreRecord> securityScoreRecords,
    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints,
    std::vector<ValidatorRiskAssessment> validatorRiskAssessments,
    std::vector<ValidatorContainmentDecision> validatorContainmentDecisions,
    std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies,
    MonetaryFirewallAudit monetaryFirewallAudit,
    GenesisTreasurySnapshot genesisTreasurySnapshot,
    ProtectionRewardBudget protectionRewardBudget,
    std::vector<ProtectionRewardGrant> protectionRewardGrants,
    std::vector<ProtectionWorkRecord> protectionWorkRecords,
    ProtectionRewardSummary protectionRewardSummary,
    std::vector<ProtectionRewardSettlement> protectionRewardSettlements,
    InflationEpochSnapshot inflationEpochSnapshot,
    MintAuthorizationRecord mintAuthorizationRecord,
    SupplyExpansionRecord supplyExpansionRecord,
    FeeEconomicBalance feeEconomicBalance,
    FeeBurnRecord feeBurnRecord,
    TreasuryFeeRecord treasuryFeeRecord,
    std::vector<SlashingEvidenceRecord> slashingEvidenceRecords,
    std::vector<SlashingPreparationRecord> slashingPreparationRecords,
    SlashingEvidenceSummary slashingEvidenceSummary,
    std::vector<CryptographicSlashingEvidenceRecord> cryptographicSlashingEvidenceRecords,
    std::vector<StakePenaltyRecord> stakePenaltyRecords,
    CryptographicSlashingSummary cryptographicSlashingSummary,
    GovernancePolicySnapshot governancePolicySnapshot,
    std::vector<GovernanceActionGuard> governanceActionGuards,
    GovernanceSummary governanceSummary,
    std::vector<ValidatorLifecycleRecord> validatorLifecycleRecords,
    EpochAccountingRecord epochAccountingRecord,
    ValidatorLifecycleSummary validatorLifecycleSummary,
    consensus::QuorumCertificate quorumCertificate,
    consensus::FinalizedBlockRecord finalizedRecord
)
    : m_block(std::move(block)),
      m_postStateRoot(std::move(postStateRoot)),
      m_totalFee(totalFee),
      m_rewardDistributions(std::move(rewardDistributions)),
      m_lockedStakePositions(std::move(lockedStakePositions)),
      m_securityScoreRecords(std::move(securityScoreRecords)),
      m_securityCheckpoints(std::move(securityCheckpoints)),
      m_validatorRiskAssessments(std::move(validatorRiskAssessments)),
      m_validatorContainmentDecisions(std::move(validatorContainmentDecisions)),
      m_validatorNetworkPolicies(std::move(validatorNetworkPolicies)),
      m_monetaryFirewallAudit(std::move(monetaryFirewallAudit)),
      m_genesisTreasurySnapshot(std::move(genesisTreasurySnapshot)),
      m_protectionRewardBudget(std::move(protectionRewardBudget)),
      m_protectionRewardGrants(std::move(protectionRewardGrants)),
      m_protectionWorkRecords(std::move(protectionWorkRecords)),
      m_protectionRewardSummary(std::move(protectionRewardSummary)),
      m_protectionRewardSettlements(std::move(protectionRewardSettlements)),
      m_inflationEpochSnapshot(std::move(inflationEpochSnapshot)),
      m_mintAuthorizationRecord(std::move(mintAuthorizationRecord)),
      m_supplyExpansionRecord(std::move(supplyExpansionRecord)),
      m_feeEconomicBalance(std::move(feeEconomicBalance)),
      m_feeBurnRecord(std::move(feeBurnRecord)),
      m_treasuryFeeRecord(std::move(treasuryFeeRecord)),
      m_slashingEvidenceRecords(std::move(slashingEvidenceRecords)),
      m_slashingPreparationRecords(std::move(slashingPreparationRecords)),
      m_slashingEvidenceSummary(std::move(slashingEvidenceSummary)),
      m_cryptographicSlashingEvidenceRecords(std::move(cryptographicSlashingEvidenceRecords)),
      m_stakePenaltyRecords(std::move(stakePenaltyRecords)),
      m_cryptographicSlashingSummary(std::move(cryptographicSlashingSummary)),
      m_governancePolicySnapshot(std::move(governancePolicySnapshot)),
      m_governanceActionGuards(std::move(governanceActionGuards)),
      m_governanceSummary(std::move(governanceSummary)),
      m_validatorLifecycleRecords(std::move(validatorLifecycleRecords)),
      m_epochAccountingRecord(std::move(epochAccountingRecord)),
      m_validatorLifecycleSummary(std::move(validatorLifecycleSummary)),
      m_quorumCertificate(std::move(quorumCertificate)),
      m_finalizedRecord(std::move(finalizedRecord)) {}

const core::Block& FinalizedBlockArtifact::block() const {
    if (!m_block.has_value()) {
        throw std::logic_error("FinalizedBlockArtifact has no block.");
    }

    return m_block.value();
}

const std::string& FinalizedBlockArtifact::postStateRoot() const {
    return m_postStateRoot;
}

utils::Amount FinalizedBlockArtifact::totalFee() const {
    return m_totalFee;
}

const std::vector<RewardDistribution>& FinalizedBlockArtifact::rewardDistributions() const {
    return m_rewardDistributions;
}

const std::vector<LockedStakePosition>& FinalizedBlockArtifact::lockedStakePositions() const {
    return m_lockedStakePositions;
}

const std::vector<SecurityScoreRecord>& FinalizedBlockArtifact::securityScoreRecords() const {
    return m_securityScoreRecords;
}

const std::vector<ValidatorSecurityCheckpoint>& FinalizedBlockArtifact::securityCheckpoints() const {
    return m_securityCheckpoints;
}

const std::vector<ValidatorRiskAssessment>& FinalizedBlockArtifact::validatorRiskAssessments() const {
    return m_validatorRiskAssessments;
}

const std::vector<ValidatorContainmentDecision>& FinalizedBlockArtifact::validatorContainmentDecisions() const {
    return m_validatorContainmentDecisions;
}

const std::vector<ValidatorNetworkPolicy>& FinalizedBlockArtifact::validatorNetworkPolicies() const {
    return m_validatorNetworkPolicies;
}

const MonetaryFirewallAudit& FinalizedBlockArtifact::monetaryFirewallAudit() const {
    return m_monetaryFirewallAudit;
}

const GenesisTreasurySnapshot& FinalizedBlockArtifact::genesisTreasurySnapshot() const {
    return m_genesisTreasurySnapshot;
}

const ProtectionRewardBudget& FinalizedBlockArtifact::protectionRewardBudget() const {
    return m_protectionRewardBudget;
}

const std::vector<ProtectionRewardGrant>& FinalizedBlockArtifact::protectionRewardGrants() const {
    return m_protectionRewardGrants;
}

const std::vector<ProtectionWorkRecord>& FinalizedBlockArtifact::protectionWorkRecords() const {
    return m_protectionWorkRecords;
}

const ProtectionRewardSummary& FinalizedBlockArtifact::protectionRewardSummary() const {
    return m_protectionRewardSummary;
}

const std::vector<ProtectionRewardSettlement>& FinalizedBlockArtifact::protectionRewardSettlements() const {
    return m_protectionRewardSettlements;
}

const InflationEpochSnapshot& FinalizedBlockArtifact::inflationEpochSnapshot() const {
    return m_inflationEpochSnapshot;
}

const MintAuthorizationRecord& FinalizedBlockArtifact::mintAuthorizationRecord() const {
    return m_mintAuthorizationRecord;
}

const SupplyExpansionRecord& FinalizedBlockArtifact::supplyExpansionRecord() const {
    return m_supplyExpansionRecord;
}

const FeeEconomicBalance& FinalizedBlockArtifact::feeEconomicBalance() const {
    return m_feeEconomicBalance;
}

const FeeBurnRecord& FinalizedBlockArtifact::feeBurnRecord() const {
    return m_feeBurnRecord;
}

const TreasuryFeeRecord& FinalizedBlockArtifact::treasuryFeeRecord() const {
    return m_treasuryFeeRecord;
}

const std::vector<SlashingEvidenceRecord>& FinalizedBlockArtifact::slashingEvidenceRecords() const {
    return m_slashingEvidenceRecords;
}

const std::vector<SlashingPreparationRecord>& FinalizedBlockArtifact::slashingPreparationRecords() const {
    return m_slashingPreparationRecords;
}

const SlashingEvidenceSummary& FinalizedBlockArtifact::slashingEvidenceSummary() const {
    return m_slashingEvidenceSummary;
}

const std::vector<CryptographicSlashingEvidenceRecord>& FinalizedBlockArtifact::cryptographicSlashingEvidenceRecords() const {
    return m_cryptographicSlashingEvidenceRecords;
}

const std::vector<StakePenaltyRecord>& FinalizedBlockArtifact::stakePenaltyRecords() const {
    return m_stakePenaltyRecords;
}

const CryptographicSlashingSummary& FinalizedBlockArtifact::cryptographicSlashingSummary() const {
    return m_cryptographicSlashingSummary;
}

const GovernancePolicySnapshot& FinalizedBlockArtifact::governancePolicySnapshot() const {
    return m_governancePolicySnapshot;
}

const std::vector<GovernanceActionGuard>& FinalizedBlockArtifact::governanceActionGuards() const {
    return m_governanceActionGuards;
}

const GovernanceSummary& FinalizedBlockArtifact::governanceSummary() const {
    return m_governanceSummary;
}

const std::vector<ValidatorLifecycleRecord>& FinalizedBlockArtifact::validatorLifecycleRecords() const {
    return m_validatorLifecycleRecords;
}

const EpochAccountingRecord& FinalizedBlockArtifact::epochAccountingRecord() const {
    return m_epochAccountingRecord;
}

const ValidatorLifecycleSummary& FinalizedBlockArtifact::validatorLifecycleSummary() const {
    return m_validatorLifecycleSummary;
}

const consensus::QuorumCertificate& FinalizedBlockArtifact::quorumCertificate() const {
    return m_quorumCertificate;
}

const consensus::FinalizedBlockRecord& FinalizedBlockArtifact::finalizedRecord() const {
    return m_finalizedRecord;
}

const economics::SupplyDelta& FinalizedBlockArtifact::supplyDelta() const {
    return m_supplyDelta;
}

void FinalizedBlockArtifact::setSupplyDelta(economics::SupplyDelta delta) {
    m_supplyDelta = std::move(delta);
}

bool FinalizedBlockArtifact::isValid() const {
    if (!m_block.has_value() ||
        !m_block->isValid() ||
        m_postStateRoot.empty() ||
        m_totalFee.isNegative() ||
        !m_quorumCertificate.isStructurallyValid() ||
        !m_finalizedRecord.isStructurallyValid()) {
        return false;
    }

    if (!m_supplyDelta.isValid() ||
        m_supplyDelta.blockHeight() != m_block->index() ||
        m_supplyDelta.blockHash() != m_block->hash()) {
        return false;
    }

    try {
        if (m_totalFee.isZero()) {
            return m_rewardDistributions.empty() &&
                   m_lockedStakePositions.empty() &&
                   m_securityScoreRecords.empty() &&
                   m_securityCheckpoints.empty() &&
                   m_validatorRiskAssessments.empty() &&
                   m_validatorContainmentDecisions.empty() &&
                   m_validatorNetworkPolicies.empty() &&
                   m_monetaryFirewallAudit.passed() &&
                   m_genesisTreasurySnapshot.active() &&
                   m_protectionRewardBudget.active() &&
                   m_protectionRewardGrants.empty() &&
                   m_protectionWorkRecords.empty() &&
                   m_protectionRewardSummary.active() &&
                   m_protectionRewardSettlements.empty() &&
                   m_inflationEpochSnapshot.active() &&
                   m_mintAuthorizationRecord.isValid() &&
                   m_supplyExpansionRecord.isValid() &&
                   FeeEconomics::sameBalance(
                       FeeEconomics::buildFeeEconomicBalance(
                           m_feeEconomicBalance.blockHeight(),
                           m_totalFee
                       ),
                       m_feeEconomicBalance
                   ) &&
                   FeeEconomics::sameBurn(
                       FeeEconomics::buildFeeBurnRecord(
                           m_feeEconomicBalance,
                           m_feeBurnRecord.supplyBefore()
                       ),
                       m_feeBurnRecord
                   ) &&
                   FeeEconomics::sameTreasuryFee(
                       FeeEconomics::buildTreasuryFeeRecord(m_feeEconomicBalance),
                       m_treasuryFeeRecord
                   ) &&
                   Governance::samePolicy(
                       Governance::buildPolicySnapshot(m_block->index()),
                       m_governancePolicySnapshot
                   ) &&
                   Governance::sameActionGuards(
                       Governance::buildActionGuards(m_governancePolicySnapshot),
                       m_governanceActionGuards
                   ) &&
                   Governance::sameSummary(
                       Governance::buildSummary(m_block->index(), m_governanceActionGuards),
                       m_governanceSummary
                   ) &&
                   ValidatorLifecycle::sameLifecycleRecords(
                       ValidatorLifecycle::buildLifecycleRecords(
                           m_block->index(),
                           m_rewardDistributions,
                           m_lockedStakePositions,
                           m_securityScoreRecords,
                           m_protectionRewardSettlements,
                           m_stakePenaltyRecords
                       ),
                       m_validatorLifecycleRecords
                   ) &&
                   ValidatorLifecycle::sameEpochAccounting(
                       ValidatorLifecycle::buildEpochAccountingRecord(m_block->index(), m_validatorLifecycleRecords),
                       m_epochAccountingRecord
                   ) &&
                   ValidatorLifecycle::sameSummary(
                       ValidatorLifecycle::buildSummary(m_block->index(), m_validatorLifecycleRecords, m_epochAccountingRecord),
                       m_validatorLifecycleSummary
                   );
        }

        return FeeEconomics::sameBalance(
                   FeeEconomics::buildFeeEconomicBalance(
                       m_feeEconomicBalance.blockHeight(),
                       m_totalFee
                   ),
                   m_feeEconomicBalance
               ) &&
               RewardDistributionCalculator::totalReward(m_rewardDistributions) == m_feeEconomicBalance.validatorRewardAmount() &&
               LockedStakePositionBuilder::samePositions(
                   LockedStakePositionBuilder::buildFromRewardDistributions(m_rewardDistributions),
                   m_lockedStakePositions
               ) &&
               SecurityScoreCalculator::sameRecords(
                   SecurityScoreCalculator::buildFromLockedStakePositions(m_lockedStakePositions, m_block->index()),
                   m_securityScoreRecords
               ) &&
               ValidatorSecurityCheckpointBuilder::sameCheckpoints(
                   ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(m_securityScoreRecords, m_lockedStakePositions, m_block->index()),
                   m_securityCheckpoints
               ) &&
               ValidatorRiskAssessmentBuilder::sameAssessments(
                   ValidatorRiskAssessmentBuilder::buildFromCheckpoints(m_securityCheckpoints),
                   m_validatorRiskAssessments
               ) &&
               ValidatorContainmentDecisionBuilder::sameDecisions(
                   ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(m_validatorRiskAssessments),
                   m_validatorContainmentDecisions
               ) &&
               ValidatorNetworkPolicyBuilder::samePolicies(
                   ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(m_validatorContainmentDecisions),
                   m_validatorNetworkPolicies
               ) &&
               m_monetaryFirewallAudit.passed() &&
               m_genesisTreasurySnapshot.active() &&
               ProtectionTreasury::sameBudget(
                   ProtectionTreasury::buildProtectionRewardBudget(
                       m_genesisTreasurySnapshot,
                       m_rewardDistributions
                   ),
                   m_protectionRewardBudget
               ) &&
               ProtectionTreasury::sameGrants(
                   ProtectionTreasury::buildProtectionRewardGrants(
                       m_protectionRewardBudget,
                       m_rewardDistributions,
                       m_securityScoreRecords
                   ),
                   m_protectionRewardGrants
               ) &&
               ProtectionRewards::sameWorkRecords(
                   ProtectionRewards::buildWorkRecords(
                       m_protectionRewardGrants,
                       m_securityScoreRecords,
                       m_validatorRiskAssessments,
                       m_validatorNetworkPolicies
                   ),
                   m_protectionWorkRecords
               ) &&
               ProtectionRewards::sameSettlements(
                   ProtectionRewards::buildSettlements(
                       m_protectionRewardGrants,
                       m_protectionWorkRecords
                   ),
                   m_protectionRewardSettlements
               ) &&
               ProtectionRewards::sameSummary(
                   ProtectionRewards::buildSummary(
                       m_protectionRewardBudget,
                       m_protectionRewardSettlements
                   ),
                   m_protectionRewardSummary
               ) &&
               m_inflationEpochSnapshot.active() &&
               ControlledIssuance::sameAuthorization(
                   ControlledIssuance::buildNoMintAuthorization(m_inflationEpochSnapshot),
                   m_mintAuthorizationRecord
               ) &&
               ControlledIssuance::sameExpansion(
                   ControlledIssuance::buildNoSupplyExpansion(m_mintAuthorizationRecord, m_inflationEpochSnapshot),
                   m_supplyExpansionRecord
               ) &&
               m_feeEconomicBalance.active() &&
               FeeEconomics::sameBalance(
                   FeeEconomics::buildFeeEconomicBalance(
                       m_feeEconomicBalance.blockHeight(),
                       m_totalFee
                   ),
                   m_feeEconomicBalance
               ) &&
               FeeEconomics::sameBurn(
                   FeeEconomics::buildFeeBurnRecord(
                       m_feeEconomicBalance,
                       m_feeBurnRecord.supplyBefore()
                   ),
                   m_feeBurnRecord
               ) &&
               FeeEconomics::sameTreasuryFee(
                   FeeEconomics::buildTreasuryFeeRecord(m_feeEconomicBalance),
                   m_treasuryFeeRecord
               ) &&
               SlashingEvidence::sameEvidenceRecords(
                   SlashingEvidence::buildEvidenceRecords(
                       m_validatorRiskAssessments,
                       m_validatorNetworkPolicies,
                       m_protectionWorkRecords
                   ),
                   m_slashingEvidenceRecords
               ) &&
               SlashingEvidence::samePreparationRecords(
                   SlashingEvidence::buildPreparationRecords(
                       m_slashingEvidenceRecords,
                       m_lockedStakePositions
                   ),
                   m_slashingPreparationRecords
               ) &&
               SlashingEvidence::sameSummary(
                   SlashingEvidence::buildSummary(
                       m_block->index(),
                       m_slashingEvidenceRecords,
                       m_slashingPreparationRecords
                   ),
                   m_slashingEvidenceSummary
               ) &&
               CryptographicSlashing::sameEvidenceRecords(
                   CryptographicSlashing::buildEvidenceRecordsFromCertifiedVotes(
                       m_quorumCertificate.votes()
                   ),
                   m_cryptographicSlashingEvidenceRecords
               ) &&
               CryptographicSlashing::sameStakePenaltyRecords(
                   CryptographicSlashing::buildStakePenaltyRecords(
                       m_cryptographicSlashingEvidenceRecords,
                       m_lockedStakePositions
                   ),
                   m_stakePenaltyRecords
               ) &&
               CryptographicSlashing::sameSummary(
                   CryptographicSlashing::buildSummary(
                       m_block->index(),
                       m_cryptographicSlashingEvidenceRecords,
                       m_stakePenaltyRecords
                   ),
                   m_cryptographicSlashingSummary
               ) &&
               Governance::samePolicy(
                   Governance::buildPolicySnapshot(m_block->index()),
                   m_governancePolicySnapshot
               ) &&
               Governance::sameActionGuards(
                   Governance::buildActionGuards(m_governancePolicySnapshot),
                   m_governanceActionGuards
               ) &&
               Governance::sameSummary(
                   Governance::buildSummary(m_block->index(), m_governanceActionGuards),
                   m_governanceSummary
               ) &&
               ValidatorLifecycle::sameLifecycleRecords(
                   ValidatorLifecycle::buildLifecycleRecords(
                       m_block->index(),
                       m_rewardDistributions,
                       m_lockedStakePositions,
                       m_securityScoreRecords,
                       m_protectionRewardSettlements,
                       m_stakePenaltyRecords
                   ),
                   m_validatorLifecycleRecords
               ) &&
               ValidatorLifecycle::sameEpochAccounting(
                   ValidatorLifecycle::buildEpochAccountingRecord(m_block->index(), m_validatorLifecycleRecords),
                   m_epochAccountingRecord
               ) &&
               ValidatorLifecycle::sameSummary(
                   ValidatorLifecycle::buildSummary(m_block->index(), m_validatorLifecycleRecords, m_epochAccountingRecord),
                   m_validatorLifecycleSummary
               );
    } catch (const std::exception&) {
        return false;
    }
}

std::string FinalizedBlockArtifact::serialize() const {
    std::ostringstream oss;

    oss << "FinalizedBlockArtifact{"
        << "blockHash=" << (m_block.has_value() && m_block->isValid() ? m_block->hash() : "INVALID")
        << ";postStateRoot=" << m_postStateRoot
        << ";totalFeeRawUnits=" << m_totalFee.rawUnits()
        << ";rewardDistributionCount=" << m_rewardDistributions.size()
        << ";lockedStakePositionCount=" << m_lockedStakePositions.size()
        << ";securityScoreRecordCount=" << m_securityScoreRecords.size()
        << ";securityCheckpointCount=" << m_securityCheckpoints.size()
        << ";validatorRiskAssessmentCount=" << m_validatorRiskAssessments.size()
        << ";validatorContainmentDecisionCount=" << m_validatorContainmentDecisions.size()
        << ";validatorNetworkPolicyCount=" << m_validatorNetworkPolicies.size()
        << ";monetaryFirewallStatus=" << m_monetaryFirewallAudit.status()
        << ";genesisTreasuryStatus=" << m_genesisTreasurySnapshot.status()
        << ";protectionRewardBudgetStatus=" << m_protectionRewardBudget.status()
        << ";protectionRewardGrantCount=" << m_protectionRewardGrants.size()
        << ";protectionWorkRecordCount=" << m_protectionWorkRecords.size()
        << ";protectionRewardSummaryStatus=" << m_protectionRewardSummary.status()
        << ";protectionRewardSettlementCount=" << m_protectionRewardSettlements.size()
        << ";inflationEpochStatus=" << m_inflationEpochSnapshot.status()
        << ";mintAuthorizationStatus=" << m_mintAuthorizationRecord.status()
        << ";supplyExpansionStatus=" << m_supplyExpansionRecord.status()
        << ";feeEconomicBalanceStatus=" << m_feeEconomicBalance.status()
        << ";feeBurnStatus=" << m_feeBurnRecord.status()
        << ";treasuryFeeStatus=" << m_treasuryFeeRecord.status()
        << ";slashingEvidenceRecordCount=" << m_slashingEvidenceRecords.size()
        << ";slashingPreparationRecordCount=" << m_slashingPreparationRecords.size()
        << ";slashingEvidenceSummaryStatus=" << m_slashingEvidenceSummary.status()
        << ";cryptographicSlashingEvidenceCount=" << m_cryptographicSlashingEvidenceRecords.size()
        << ";stakePenaltyRecordCount=" << m_stakePenaltyRecords.size()
        << ";cryptographicSlashingSummaryStatus=" << m_cryptographicSlashingSummary.status()
        << ";governancePolicyStatus=" << m_governancePolicySnapshot.status()
        << ";governanceActionGuardCount=" << m_governanceActionGuards.size()
        << ";governanceSummaryStatus=" << m_governanceSummary.status()
        << ";validatorLifecycleRecordCount=" << m_validatorLifecycleRecords.size()
        << ";epochAccountingStatus=" << m_epochAccountingRecord.status()
        << ";validatorLifecycleSummaryStatus=" << m_validatorLifecycleSummary.status()
        << "}";

    return oss.str();
}

core::Block FinalizedBlockArtifactCodec::readBlockFile(
    const std::filesystem::path& path
) {
    return readBlockArtifactFile(path).block();
}

core::Block FinalizedBlockArtifactCodec::decodeBlockFileContents(
    const std::string& contents
) {
    return decodeBlockArtifactFileContents(contents).block();
}

FinalizedBlockArtifact FinalizedBlockArtifactCodec::readBlockArtifactFile(
    const std::filesystem::path& path
) {
    return decodeBlockArtifactFileContents(
        readTextFile(path)
    );
}

FinalizedBlockArtifact FinalizedBlockArtifactCodec::decodeBlockArtifactFileContents(
    const std::string& contents
) {
    const serialization::KeyValueFileDocument document =
        serialization::KeyValueFileCodec::parse(
            contents,
            FinalizedArtifactSchema::currentSchemaId()
        );

    const std::size_t recordCount = static_cast<std::size_t>(parseU64Strict(document.requireField("recordCount"), "recordCount"));
    const std::size_t rewardDistributionCount = static_cast<std::size_t>(parseU64Strict(document.requireField("rewardDistributionCount"), "rewardDistributionCount"));
    const std::size_t lockedStakePositionCount = static_cast<std::size_t>(parseU64Strict(document.requireField("lockedStakePositionCount"), "lockedStakePositionCount"));
    const std::size_t securityScoreRecordCount = static_cast<std::size_t>(parseU64Strict(document.requireField("securityScoreRecordCount"), "securityScoreRecordCount"));
    const std::size_t securityCheckpointCount = static_cast<std::size_t>(parseU64Strict(document.requireField("securityCheckpointCount"), "securityCheckpointCount"));
    const std::size_t validatorRiskAssessmentCount = static_cast<std::size_t>(parseU64Strict(document.requireField("validatorRiskAssessmentCount"), "validatorRiskAssessmentCount"));
    const std::size_t validatorContainmentDecisionCount = static_cast<std::size_t>(parseU64Strict(document.requireField("validatorContainmentDecisionCount"), "validatorContainmentDecisionCount"));
    const std::size_t validatorNetworkPolicyCount = static_cast<std::size_t>(parseU64Strict(document.requireField("validatorNetworkPolicyCount"), "validatorNetworkPolicyCount"));
    const std::size_t protectionRewardGrantCount = static_cast<std::size_t>(parseU64Strict(document.requireField("protectionRewardGrantCount"), "protectionRewardGrantCount"));
    const std::size_t protectionWorkRecordCount = static_cast<std::size_t>(parseU64Strict(document.requireField("protectionWorkRecordCount"), "protectionWorkRecordCount"));
    const std::size_t protectionRewardSettlementCount = static_cast<std::size_t>(parseU64Strict(document.requireField("protectionRewardSettlementCount"), "protectionRewardSettlementCount"));
    const std::size_t slashingEvidenceRecordCount = static_cast<std::size_t>(parseU64Strict(document.requireField("slashingEvidenceRecordCount"), "slashingEvidenceRecordCount"));
    const std::size_t slashingPreparationRecordCount = static_cast<std::size_t>(parseU64Strict(document.requireField("slashingPreparationRecordCount"), "slashingPreparationRecordCount"));
    const std::size_t cryptographicSlashingEvidenceCount = static_cast<std::size_t>(parseU64Strict(document.requireField("cryptographicSlashingEvidenceCount"), "cryptographicSlashingEvidenceCount"));
    const std::size_t stakePenaltyRecordCount = static_cast<std::size_t>(parseU64Strict(document.requireField("stakePenaltyRecordCount"), "stakePenaltyRecordCount"));
    const std::size_t governanceActionGuardCount = static_cast<std::size_t>(parseU64Strict(document.requireField("governanceActionGuardCount"), "governanceActionGuardCount"));
    const std::size_t validatorLifecycleRecordCount = static_cast<std::size_t>(parseU64Strict(document.requireField("validatorLifecycleRecordCount"), "validatorLifecycleRecordCount"));
    const std::size_t supplyDeltaMintRecordCount =
        FinalizedMonetarySectionCodec::mintRecordCount(document);
    const std::size_t supplyDeltaBurnRecordCount =
        FinalizedMonetarySectionCodec::burnRecordCount(document);

    std::set<std::string> allowedFields = {
        "blockIndex",
        "blockHash",
        "previousHash",
        "postStateRoot",
        "totalFeeRawUnits",
        "rewardDistributionCount",
        "lockedStakePositionCount",
        "securityScoreRecordCount",
        "securityCheckpointCount",
        "validatorRiskAssessmentCount",
        "validatorContainmentDecisionCount",
        "validatorNetworkPolicyCount",
        "monetaryFirewallStatus",
        "genesisTreasuryStatus",
        "protectionRewardBudgetStatus",
        "protectionRewardGrantCount",
        "protectionWorkRecordCount",
        "protectionRewardSummaryStatus",
        "protectionRewardSettlementCount",
        "inflationEpochStatus",
        "mintAuthorizationStatus",
        "supplyExpansionStatus",
        "feeEconomicBalanceStatus",
        "feeBurnStatus",
        "treasuryFeeStatus",
        "slashingEvidenceRecordCount",
        "slashingPreparationRecordCount",
        "slashingEvidenceSummaryStatus",
        "cryptographicSlashingEvidenceCount",
        "stakePenaltyRecordCount",
        "cryptographicSlashingSummaryStatus",
        "governancePolicyStatus",
        "governanceActionGuardCount",
        "governanceSummaryStatus",
        "validatorLifecycleRecordCount",
        "epochAccountingStatus",
        "validatorLifecycleSummaryStatus",
        "monetary.blockHeight",
        "monetary.supplyBeforeRawUnits",
        "monetary.mintedRawUnits",
        "monetary.burnedRawUnits",
        "monetary.treasuryDeltaRawUnits",
        "monetary.supplyAfterRawUnits",
        "monetary.annualMintLimitRawUnits",
        "monetary.annualMintUsedBeforeRawUnits",
        "monetary.annualMintUsedAfterRawUnits",
        "monetary.policyId",
        "monetary.reason",
        "treasury.treasuryAddress",
        "treasury.blockHeight",
        "treasury.genesisTreasuryBalanceRawUnits",
        "treasury.protectedReserveRawUnits",
        "treasury.protectionBudgetRawUnits",
        "treasury.availableBalanceRawUnits",
        "treasury.reason",
        "protectionBudget.blockHeight",
        "protectionBudget.treasuryAddress",
        "protectionBudget.availableBudgetRawUnits",
        "protectionBudget.plannedTotalRawUnits",
        "protectionBudget.remainingBudgetRawUnits",
        "protectionBudget.beneficiaryCount",
        "protectionBudget.reason",
        "protectionBudget.sourceTreasuryDigest",
        "protectionSummary.blockHeight",
        "protectionSummary.plannedTotalRawUnits",
        "protectionSummary.earnedTotalRawUnits",
        "protectionSummary.deferredTotalRawUnits",
        "protectionSummary.beneficiaryCount",
        "protectionSummary.reason",
        "protectionSummary.sourceBudgetDigest",
        "inflationEpoch.blockHeight",
        "inflationEpoch.epochStartBlock",
        "inflationEpoch.epochEndBlock",
        "inflationEpoch.maxAnnualInflationBasisPoints",
        "inflationEpoch.baseSupplyRawUnits",
        "inflationEpoch.annualMintLimitRawUnits",
        "inflationEpoch.mintedThisEpochRawUnits",
        "inflationEpoch.remainingMintCapacityRawUnits",
        "inflationEpoch.policyId",
        "inflationEpoch.reason",
        "mintAuthorization.blockHeight",
        "mintAuthorization.authorizationId",
        "mintAuthorization.authorizedAmountRawUnits",
        "mintAuthorization.activationBlock",
        "mintAuthorization.expirationBlock",
        "mintAuthorization.requiredApprovalBasisPoints",
        "mintAuthorization.timelockBlocks",
        "mintAuthorization.governanceDigest",
        "mintAuthorization.reason",
        "mintAuthorization.sourceEpochDigest",
        "supplyExpansion.blockHeight",
        "supplyExpansion.mintedAmountRawUnits",
        "supplyExpansion.recipientAddress",
        "supplyExpansion.authorizationId",
        "supplyExpansion.policyId",
        "supplyExpansion.reason",
        "supplyExpansion.sourceAuthorizationDigest",
        "feeBalance.blockHeight",
        "feeBalance.totalFeeRawUnits",
        "feeBalance.validatorRewardRawUnits",
        "feeBalance.treasuryRawUnits",
        "feeBalance.burnRawUnits",
        "feeBalance.policyId",
        "feeBalance.reason",
        "feeBurn.blockHeight",
        "feeBurn.burnAmountRawUnits",
        "feeBurn.supplyBeforeRawUnits",
        "feeBurn.supplyAfterRawUnits",
        "feeBurn.reason",
        "feeBurn.sourceFeeBalanceDigest",
        "treasuryFee.blockHeight",
        "treasuryFee.treasuryAddress",
        "treasuryFee.treasuryAmountRawUnits",
        "treasuryFee.reason",
        "treasuryFee.sourceFeeBalanceDigest",
        "slashingSummary.blockHeight",
        "slashingSummary.evidenceCount",
        "slashingSummary.slashableEvidenceCount",
        "slashingSummary.maxSeverityScore",
        "slashingSummary.preparedPenaltyTotalRawUnits",
        "slashingSummary.reason",
        "slashingSummary.sourcePreparationDigest",
        "cryptographicSlashingSummary.blockHeight",
        "cryptographicSlashingSummary.evidenceCount",
        "cryptographicSlashingSummary.slashableEvidenceCount",
        "cryptographicSlashingSummary.maxSeverityScore",
        "cryptographicSlashingSummary.penaltyTotalRawUnits",
        "cryptographicSlashingSummary.reason",
        "cryptographicSlashingSummary.sourcePenaltyDigest",
        "governancePolicy.blockHeight",
        "governancePolicy.requiredApprovalBasisPoints",
        "governancePolicy.timelockBlocks",
        "governancePolicy.activationDelayBlocks",
        "governancePolicy.policyId",
        "governancePolicy.reason",
        "governanceSummary.blockHeight",
        "governanceSummary.guardCount",
        "governanceSummary.activeProposalCount",
        "governanceSummary.approvedProposalCount",
        "governanceSummary.executableProposalCount",
        "governanceSummary.executedProposalCount",
        "governanceSummary.reason",
        "governanceSummary.sourceGuardDigest",
        "epochAccounting.blockHeight",
        "epochAccounting.epochIndex",
        "epochAccounting.epochStartBlock",
        "epochAccounting.epochEndBlock",
        "epochAccounting.validatorCount",
        "epochAccounting.activeValidatorCount",
        "epochAccounting.totalLockedStakeRawUnits",
        "epochAccounting.totalEarnedRewardsRawUnits",
        "epochAccounting.totalSlashingPenaltiesRawUnits",
        "epochAccounting.reason",
        "epochAccounting.sourceLifecycleDigest",
        "validatorLifecycleSummary.blockHeight",
        "validatorLifecycleSummary.epochIndex",
        "validatorLifecycleSummary.activeValidatorCount",
        "validatorLifecycleSummary.jailedValidatorCount",
        "validatorLifecycleSummary.slashedValidatorCount",
        "validatorLifecycleSummary.totalLockedStakeRawUnits",
        "validatorLifecycleSummary.totalEarnedRewardsRawUnits",
        "validatorLifecycleSummary.totalSlashingPenaltiesRawUnits",
        "validatorLifecycleSummary.reason",
        "validatorLifecycleSummary.sourceEpochDigest",
        "timestamp",
        "recordCount",
        "block",
        "quorumCertificate",
        "finalizedRecord"
    };

    FinalizedMonetarySectionCodec::addAllowedFields(
        allowedFields,
        supplyDeltaMintRecordCount,
        supplyDeltaBurnRecordCount
    );

    for (std::size_t index = 0; index < recordCount; ++index) {
        allowedFields.insert("record." + std::to_string(index));
    }

    for (std::size_t index = 0; index < rewardDistributionCount; ++index) {
        const std::string prefix = "reward." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "totalRewardRawUnits");
        allowedFields.insert(prefix + "liquidRewardRawUnits");
        allowedFields.insert(prefix + "lockedRewardRawUnits");
        allowedFields.insert(prefix + "reason");
    }

    for (std::size_t index = 0; index < lockedStakePositionCount; ++index) {
        const std::string prefix = "lockedStake." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "ownerAddress");
        allowedFields.insert(prefix + "amountRawUnits");
        allowedFields.insert(prefix + "createdAtHeight");
        allowedFields.insert(prefix + "unlockAtHeight");
        allowedFields.insert(prefix + "slashable");
        allowedFields.insert(prefix + "sourceRewardId");
    }

    for (std::size_t index = 0; index < securityScoreRecordCount; ++index) {
        const std::string prefix = "securityScore." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "score");
        allowedFields.insert(prefix + "lockedStakeScore");
        allowedFields.insert(prefix + "participationScore");
        allowedFields.insert(prefix + "maturityScore");
        allowedFields.insert(prefix + "penaltyScore");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceLockedStakeId");
    }

    for (std::size_t index = 0; index < securityCheckpointCount; ++index) {
        const std::string prefix = "securityCheckpoint." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "score");
        allowedFields.insert(prefix + "band");
        allowedFields.insert(prefix + "lockedStakeRawUnits");
        allowedFields.insert(prefix + "securityScoreRecordCount");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceDigest");
    }

    for (std::size_t index = 0; index < validatorRiskAssessmentCount; ++index) {
        const std::string prefix = "validatorRisk." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "score");
        allowedFields.insert(prefix + "band");
        allowedFields.insert(prefix + "lockedStakeRawUnits");
        allowedFields.insert(prefix + "riskScore");
        allowedFields.insert(prefix + "riskLevel");
        allowedFields.insert(prefix + "recommendedAction");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "checkpointDigest");
    }

    for (std::size_t index = 0; index < validatorContainmentDecisionCount; ++index) {
        const std::string prefix = "validatorContainment." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "riskLevel");
        allowedFields.insert(prefix + "recommendedAction");
        allowedFields.insert(prefix + "containmentMode");
        allowedFields.insert(prefix + "peerTrustState");
        allowedFields.insert(prefix + "networkAdmissionState");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceRiskDigest");
    }

    for (std::size_t index = 0; index < validatorNetworkPolicyCount; ++index) {
        const std::string prefix = "validatorNetworkPolicy." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "containmentMode");
        allowedFields.insert(prefix + "peerTrustState");
        allowedFields.insert(prefix + "networkAdmissionState");
        allowedFields.insert(prefix + "connectionPolicy");
        allowedFields.insert(prefix + "messagePolicy");
        allowedFields.insert(prefix + "consensusPolicy");
        allowedFields.insert(prefix + "requiresManualReview");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceContainmentDigest");
    }

    for (std::size_t index = 0; index < protectionRewardGrantCount; ++index) {
        const std::string prefix = "protectionGrant." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "plannedRewardRawUnits");
        allowedFields.insert(prefix + "securityScore");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceBudgetDigest");
    }

    for (std::size_t index = 0; index < protectionWorkRecordCount; ++index) {
        const std::string prefix = "protectionWork." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "uptimeScore");
        allowedFields.insert(prefix + "correctVoteScore");
        allowedFields.insert(prefix + "attackDetectionScore");
        allowedFields.insert(prefix + "auditContributionScore");
        allowedFields.insert(prefix + "securityScore");
        allowedFields.insert(prefix + "riskPenaltyScore");
        allowedFields.insert(prefix + "totalWorkScore");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceSecurityDigest");
    }

    for (std::size_t index = 0; index < protectionRewardSettlementCount; ++index) {
        const std::string prefix = "protectionSettlement." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "plannedRewardRawUnits");
        allowedFields.insert(prefix + "earnedRewardRawUnits");
        allowedFields.insert(prefix + "deferredRewardRawUnits");
        allowedFields.insert(prefix + "workScore");
        allowedFields.insert(prefix + "securityScore");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceGrantDigest");
        allowedFields.insert(prefix + "sourceWorkDigest");
    }

    for (std::size_t index = 0; index < slashingEvidenceRecordCount; ++index) {
        const std::string prefix = "slashingEvidence." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "evidenceType");
        allowedFields.insert(prefix + "severityScore");
        allowedFields.insert(prefix + "slashable");
        allowedFields.insert(prefix + "recommendedAction");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceSecurityDigest");
    }

    for (std::size_t index = 0; index < slashingPreparationRecordCount; ++index) {
        const std::string prefix = "slashingPreparation." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "evidenceCount");
        allowedFields.insert(prefix + "slashableEvidenceCount");
        allowedFields.insert(prefix + "maxSeverityScore");
        allowedFields.insert(prefix + "preparedPenaltyRawUnits");
        allowedFields.insert(prefix + "enforcementAction");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceEvidenceDigest");
    }

    for (std::size_t index = 0; index < cryptographicSlashingEvidenceCount; ++index) {
        const std::string prefix = "cryptographicSlashingEvidence." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "round");
        allowedFields.insert(prefix + "evidenceType");
        allowedFields.insert(prefix + "severityScore");
        allowedFields.insert(prefix + "penaltyBasisPoints");
        allowedFields.insert(prefix + "firstVoteDigest");
        allowedFields.insert(prefix + "secondVoteDigest");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceEvidenceDigest");
    }

    for (std::size_t index = 0; index < stakePenaltyRecordCount; ++index) {
        const std::string prefix = "stakePenalty." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "lockedStakeBeforeRawUnits");
        allowedFields.insert(prefix + "penaltyAmountRawUnits");
        allowedFields.insert(prefix + "lockedStakeAfterRawUnits");
        allowedFields.insert(prefix + "evidenceCount");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceEvidenceDigest");
    }

    for (std::size_t index = 0; index < governanceActionGuardCount; ++index) {
        const std::string prefix = "governanceGuard." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "actionType");
        allowedFields.insert(prefix + "status");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "protectedResource");
        allowedFields.insert(prefix + "requiredApprovalBasisPoints");
        allowedFields.insert(prefix + "timelockBlocks");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourcePolicyDigest");
    }

    for (std::size_t index = 0; index < validatorLifecycleRecordCount; ++index) {
        const std::string prefix = "validatorLifecycle." + std::to_string(index) + ".";
        allowedFields.insert(prefix + "validatorAddress");
        allowedFields.insert(prefix + "blockHeight");
        allowedFields.insert(prefix + "epochIndex");
        allowedFields.insert(prefix + "lifecycleStatus");
        allowedFields.insert(prefix + "lockedStakeRawUnits");
        allowedFields.insert(prefix + "earnedRewardRawUnits");
        allowedFields.insert(prefix + "slashingPenaltyRawUnits");
        allowedFields.insert(prefix + "securityScore");
        allowedFields.insert(prefix + "reason");
        allowedFields.insert(prefix + "sourceDigest");
    }

    document.requireOnlyFields(allowedFields);

    /*
     * The finalized block artifact stores explicit block fields, fee accounting,
     * locked stake, security score records, checkpoints, risk assessments,
     * containment decisions, network policies, monetary firewall audit,
     * protection rewards, controlled issuance, governance, slashing evidence and
     * validator lifecycle accounting. The canonical block serialization remains
     * the integrity anchor for the block payload itself.
     */
    const std::string serializedBlock = document.requireField("block");
    core::Block block = serialization::BlockCodec::deserialize(serializedBlock);

    const std::uint64_t index = static_cast<std::uint64_t>(parseU64Strict(document.requireField("blockIndex"), "blockIndex"));
    const std::string blockHash = document.requireField("blockHash");
    const std::string previousHash = document.requireField("previousHash");
    const std::string postStateRoot = document.requireField("postStateRoot");
    const utils::Amount totalFee = parseAmountStrict(document.requireField("totalFeeRawUnits"), "totalFeeRawUnits");

    std::vector<RewardDistribution> rewardDistributions;
    rewardDistributions.reserve(rewardDistributionCount);
    for (std::size_t rewardIndex = 0; rewardIndex < rewardDistributionCount; ++rewardIndex) {
        rewardDistributions.push_back(parseRewardDistribution(document, rewardIndex));
    }

    std::vector<LockedStakePosition> lockedStakePositions;
    lockedStakePositions.reserve(lockedStakePositionCount);
    for (std::size_t positionIndex = 0; positionIndex < lockedStakePositionCount; ++positionIndex) {
        lockedStakePositions.push_back(parseLockedStakePosition(document, positionIndex));
    }

    std::vector<SecurityScoreRecord> securityScoreRecords;
    securityScoreRecords.reserve(securityScoreRecordCount);
    for (std::size_t scoreIndex = 0; scoreIndex < securityScoreRecordCount; ++scoreIndex) {
        securityScoreRecords.push_back(parseSecurityScoreRecord(document, scoreIndex));
    }

    std::vector<ValidatorSecurityCheckpoint> securityCheckpoints;
    securityCheckpoints.reserve(securityCheckpointCount);
    for (std::size_t checkpointIndex = 0; checkpointIndex < securityCheckpointCount; ++checkpointIndex) {
        securityCheckpoints.push_back(parseSecurityCheckpoint(document, checkpointIndex));
    }

    std::vector<ValidatorRiskAssessment> validatorRiskAssessments;
    validatorRiskAssessments.reserve(validatorRiskAssessmentCount);
    for (std::size_t assessmentIndex = 0; assessmentIndex < validatorRiskAssessmentCount; ++assessmentIndex) {
        validatorRiskAssessments.push_back(parseValidatorRiskAssessment(document, assessmentIndex));
    }

    std::vector<ValidatorContainmentDecision> validatorContainmentDecisions;
    validatorContainmentDecisions.reserve(validatorContainmentDecisionCount);
    for (std::size_t decisionIndex = 0; decisionIndex < validatorContainmentDecisionCount; ++decisionIndex) {
        validatorContainmentDecisions.push_back(parseValidatorContainmentDecision(document, decisionIndex));
    }

    std::vector<ValidatorNetworkPolicy> validatorNetworkPolicies;
    validatorNetworkPolicies.reserve(validatorNetworkPolicyCount);
    for (std::size_t policyIndex = 0; policyIndex < validatorNetworkPolicyCount; ++policyIndex) {
        validatorNetworkPolicies.push_back(parseValidatorNetworkPolicy(document, policyIndex));
    }

    const MonetaryFirewallAudit monetaryFirewallAudit =
        parseMonetaryFirewallAudit(document);

    const GenesisTreasurySnapshot genesisTreasurySnapshot =
        parseGenesisTreasurySnapshot(document);

    const ProtectionRewardBudget protectionRewardBudget =
        parseProtectionRewardBudget(document);

    std::vector<ProtectionRewardGrant> protectionRewardGrants;
    protectionRewardGrants.reserve(protectionRewardGrantCount);
    for (std::size_t grantIndex = 0; grantIndex < protectionRewardGrantCount; ++grantIndex) {
        protectionRewardGrants.push_back(parseProtectionRewardGrant(document, grantIndex));
    }

    std::vector<ProtectionWorkRecord> protectionWorkRecords;
    protectionWorkRecords.reserve(protectionWorkRecordCount);
    for (std::size_t workIndex = 0; workIndex < protectionWorkRecordCount; ++workIndex) {
        protectionWorkRecords.push_back(parseProtectionWorkRecord(document, workIndex));
    }

    const ProtectionRewardSummary protectionRewardSummary =
        parseProtectionRewardSummary(document);

    std::vector<ProtectionRewardSettlement> protectionRewardSettlements;
    protectionRewardSettlements.reserve(protectionRewardSettlementCount);
    for (std::size_t settlementIndex = 0; settlementIndex < protectionRewardSettlementCount; ++settlementIndex) {
        protectionRewardSettlements.push_back(parseProtectionRewardSettlement(document, settlementIndex));
    }

    const InflationEpochSnapshot inflationEpochSnapshot =
        parseInflationEpochSnapshot(document);

    const MintAuthorizationRecord mintAuthorizationRecord =
        parseMintAuthorizationRecord(document);

    const SupplyExpansionRecord supplyExpansionRecord =
        parseSupplyExpansionRecord(document);

    const FeeEconomicBalance feeEconomicBalance =
        parseFeeEconomicBalance(document);

    const FeeBurnRecord feeBurnRecord =
        parseFeeBurnRecord(document);

    const TreasuryFeeRecord treasuryFeeRecord =
        parseTreasuryFeeRecord(document);

    std::vector<SlashingEvidenceRecord> slashingEvidenceRecords;
    slashingEvidenceRecords.reserve(slashingEvidenceRecordCount);
    for (std::size_t evidenceIndex = 0; evidenceIndex < slashingEvidenceRecordCount; ++evidenceIndex) {
        slashingEvidenceRecords.push_back(parseSlashingEvidenceRecord(document, evidenceIndex));
    }

    std::vector<SlashingPreparationRecord> slashingPreparationRecords;
    slashingPreparationRecords.reserve(slashingPreparationRecordCount);
    for (std::size_t preparationIndex = 0; preparationIndex < slashingPreparationRecordCount; ++preparationIndex) {
        slashingPreparationRecords.push_back(parseSlashingPreparationRecord(document, preparationIndex));
    }

    const SlashingEvidenceSummary slashingEvidenceSummary =
        parseSlashingEvidenceSummary(document);

    std::vector<CryptographicSlashingEvidenceRecord> cryptographicSlashingEvidenceRecords;
    cryptographicSlashingEvidenceRecords.reserve(cryptographicSlashingEvidenceCount);
    for (std::size_t evidenceIndex = 0; evidenceIndex < cryptographicSlashingEvidenceCount; ++evidenceIndex) {
        cryptographicSlashingEvidenceRecords.push_back(parseCryptographicSlashingEvidenceRecord(document, evidenceIndex));
    }

    std::vector<StakePenaltyRecord> stakePenaltyRecords;
    stakePenaltyRecords.reserve(stakePenaltyRecordCount);
    for (std::size_t penaltyIndex = 0; penaltyIndex < stakePenaltyRecordCount; ++penaltyIndex) {
        stakePenaltyRecords.push_back(parseStakePenaltyRecord(document, penaltyIndex));
    }

    const CryptographicSlashingSummary cryptographicSlashingSummary =
        parseCryptographicSlashingSummary(document);

    const GovernancePolicySnapshot governancePolicySnapshot =
        parseGovernancePolicySnapshot(document);

    std::vector<GovernanceActionGuard> governanceActionGuards;
    governanceActionGuards.reserve(governanceActionGuardCount);
    for (std::size_t guardIndex = 0; guardIndex < governanceActionGuardCount; ++guardIndex) {
        governanceActionGuards.push_back(parseGovernanceActionGuard(document, guardIndex));
    }

    const GovernanceSummary governanceSummary =
        parseGovernanceSummary(document);

    std::vector<ValidatorLifecycleRecord> validatorLifecycleRecords;
    validatorLifecycleRecords.reserve(validatorLifecycleRecordCount);
    for (std::size_t lifecycleIndex = 0; lifecycleIndex < validatorLifecycleRecordCount; ++lifecycleIndex) {
        validatorLifecycleRecords.push_back(parseValidatorLifecycleRecord(document, lifecycleIndex));
    }

    const EpochAccountingRecord epochAccountingRecord =
        parseEpochAccountingRecord(document);

    const ValidatorLifecycleSummary validatorLifecycleSummary =
        parseValidatorLifecycleSummary(document);

    const std::int64_t timestamp = parseI64Strict(document.requireField("timestamp"), "timestamp");

    const consensus::QuorumCertificate quorumCertificate =
        consensus::QuorumCertificate::deserialize(document.requireField("quorumCertificate"));

    const consensus::FinalizedBlockRecord finalizedRecord =
        consensus::FinalizedBlockRecord::deserialize(document.requireField("finalizedRecord"));

    const economics::SupplyDelta parsedSupplyDelta =
        FinalizedMonetarySectionCodec::decodeSupplyDelta(
            document,
            block.index(),
            block.hash()
        );

    if (postStateRoot.empty()) {
        throw std::invalid_argument("Finalized block file postStateRoot is empty.");
    }

    if (block.index() != index ||
        block.hash() != blockHash ||
        block.previousHash() != previousHash ||
        block.timestamp() != timestamp ||
        block.records().size() != recordCount) {
        throw std::invalid_argument("Finalized block file header does not match block payload.");
    }

    for (std::size_t recordIndex = 0; recordIndex < recordCount; ++recordIndex) {
        const std::string key = "record." + std::to_string(recordIndex);
        if (block.records()[recordIndex].serialize() != document.requireField(key)) {
            throw std::invalid_argument("Finalized block record line does not match block payload.");
        }
    }

    if (!consensus::BlockFinalizer::certificateMatchesBlock(block, quorumCertificate)) {
        throw std::invalid_argument("Finalized block quorum certificate does not match block payload.");
    }

    if (!finalizedRecord.matchesBlock(block) ||
        finalizedRecord.quorumCertificate().serialize() != quorumCertificate.serialize()) {
        throw std::invalid_argument("Finalized block record does not match block or quorum certificate.");
    }

    if (RewardDistributionCalculator::totalReward(rewardDistributions) != feeEconomicBalance.validatorRewardAmount()) {
        throw std::invalid_argument("Finalized block reward distributions do not match validator fee allocation.");
    }

    if (!LockedStakePositionBuilder::samePositions(
            LockedStakePositionBuilder::buildFromRewardDistributions(rewardDistributions),
            lockedStakePositions)) {
        throw std::invalid_argument("Finalized block locked stake positions do not match reward distributions.");
    }

    if (!SecurityScoreCalculator::sameRecords(
            SecurityScoreCalculator::buildFromLockedStakePositions(lockedStakePositions, block.index()),
            securityScoreRecords)) {
        throw std::invalid_argument("Finalized block security score records do not match locked stake positions.");
    }

    if (!ValidatorSecurityCheckpointBuilder::sameCheckpoints(
            ValidatorSecurityCheckpointBuilder::buildFromSecurityScores(securityScoreRecords, lockedStakePositions, block.index()),
            securityCheckpoints)) {
        throw std::invalid_argument("Finalized block security checkpoints do not match security score records.");
    }

    if (!ValidatorRiskAssessmentBuilder::sameAssessments(
            ValidatorRiskAssessmentBuilder::buildFromCheckpoints(securityCheckpoints),
            validatorRiskAssessments)) {
        throw std::invalid_argument("Finalized block validator risk assessments do not match security checkpoints.");
    }

    if (!ValidatorContainmentDecisionBuilder::sameDecisions(
            ValidatorContainmentDecisionBuilder::buildFromRiskAssessments(validatorRiskAssessments),
            validatorContainmentDecisions)) {
        throw std::invalid_argument("Finalized block validator containment decisions do not match risk assessments.");
    }

    if (!ValidatorNetworkPolicyBuilder::samePolicies(
            ValidatorNetworkPolicyBuilder::buildFromContainmentDecisions(validatorContainmentDecisions),
            validatorNetworkPolicies)) {
        throw std::invalid_argument("Finalized block validator network policies do not match containment decisions.");
    }

    if (!monetaryFirewallAudit.passed()) {
        throw std::invalid_argument("Finalized block monetary firewall audit did not pass.");
    }

    if (!genesisTreasurySnapshot.active() ||
        !protectionRewardBudget.active()) {
        throw std::invalid_argument("Finalized block protection treasury plan is inactive.");
    }

    if (!ProtectionTreasury::sameBudget(
            ProtectionTreasury::buildProtectionRewardBudget(
                genesisTreasurySnapshot,
                rewardDistributions
            ),
            protectionRewardBudget)) {
        throw std::invalid_argument("Finalized block protection reward budget does not match treasury snapshot.");
    }

    if (!ProtectionTreasury::sameGrants(
            ProtectionTreasury::buildProtectionRewardGrants(
                protectionRewardBudget,
                rewardDistributions,
                securityScoreRecords
            ),
            protectionRewardGrants)) {
        throw std::invalid_argument("Finalized block protection reward grants do not match protection budget.");
    }

    if (!ProtectionRewards::sameWorkRecords(
            ProtectionRewards::buildWorkRecords(
                protectionRewardGrants,
                securityScoreRecords,
                validatorRiskAssessments,
                validatorNetworkPolicies
            ),
            protectionWorkRecords)) {
        throw std::invalid_argument("Finalized block protection work records do not match security context.");
    }

    if (!ProtectionRewards::sameSettlements(
            ProtectionRewards::buildSettlements(
                protectionRewardGrants,
                protectionWorkRecords
            ),
            protectionRewardSettlements)) {
        throw std::invalid_argument("Finalized block protection reward settlements do not match work records.");
    }

    if (!ProtectionRewards::sameSummary(
            ProtectionRewards::buildSummary(
                protectionRewardBudget,
                protectionRewardSettlements
            ),
            protectionRewardSummary)) {
        throw std::invalid_argument("Finalized block protection reward summary does not match settlements.");
    }

    if (!inflationEpochSnapshot.active() ||
        !ControlledIssuance::sameAuthorization(
            ControlledIssuance::buildNoMintAuthorization(inflationEpochSnapshot),
            mintAuthorizationRecord) ||
        !ControlledIssuance::sameExpansion(
            ControlledIssuance::buildNoSupplyExpansion(mintAuthorizationRecord, inflationEpochSnapshot),
            supplyExpansionRecord)) {
        throw std::invalid_argument("Finalized block controlled issuance records are invalid.");
    }

    if (!FeeEconomics::sameBalance(
            FeeEconomics::buildFeeEconomicBalance(
                feeEconomicBalance.blockHeight(),
                totalFee
            ),
            feeEconomicBalance)) {
        throw std::invalid_argument("Finalized block fee economic balance does not match total fee.");
    }

    if (RewardDistributionCalculator::totalReward(rewardDistributions) != feeEconomicBalance.validatorRewardAmount()) {
        throw std::invalid_argument("Finalized block validator rewards do not match fee split.");
    }

    if (!FeeEconomics::sameBurn(
            FeeEconomics::buildFeeBurnRecord(
                feeEconomicBalance,
                feeBurnRecord.supplyBefore()
            ),
            feeBurnRecord)) {
        throw std::invalid_argument("Finalized block fee burn record does not match fee split.");
    }

    if (!FeeEconomics::sameTreasuryFee(
            FeeEconomics::buildTreasuryFeeRecord(feeEconomicBalance),
            treasuryFeeRecord)) {
        throw std::invalid_argument("Finalized block treasury fee record does not match fee split.");
    }

    if (feeBurnRecord.burnAmount() != monetaryFirewallAudit.supplyLedger().burned() ||
        feeBurnRecord.supplyBefore() != monetaryFirewallAudit.supplyLedger().supplyBefore() ||
        feeBurnRecord.supplyAfter() != monetaryFirewallAudit.supplyLedger().supplyAfter() ||
        treasuryFeeRecord.treasuryAmount() != monetaryFirewallAudit.supplyLedger().treasuryDelta()) {
        throw std::invalid_argument("Finalized block fee records do not match monetary firewall audit.");
    }

    if (parsedSupplyDelta.blockHeight() != block.index() ||
        parsedSupplyDelta.blockHash() != block.hash() ||
        parsedSupplyDelta.supplyBefore() != monetaryFirewallAudit.supplyLedger().supplyBefore() ||
        parsedSupplyDelta.mintedAmount() != monetaryFirewallAudit.supplyLedger().minted() ||
        parsedSupplyDelta.burnedAmount() != monetaryFirewallAudit.supplyLedger().burned() ||
        parsedSupplyDelta.supplyAfter() != monetaryFirewallAudit.supplyLedger().supplyAfter()) {
        throw std::invalid_argument("Finalized block SupplyDelta does not match monetary firewall audit.");
    }

    if (parsedSupplyDelta.burnedAmount() != feeBurnRecord.burnAmount() ||
        parsedSupplyDelta.mintedAmount() != supplyExpansionRecord.mintedAmount()) {
        throw std::invalid_argument("Finalized block SupplyDelta does not match legacy monetary records.");
    }

    const std::vector<SlashingEvidenceRecord> expectedSlashingEvidence =
        SlashingEvidence::buildEvidenceRecords(
            validatorRiskAssessments,
            validatorNetworkPolicies,
            protectionWorkRecords
        );

    if (!SlashingEvidence::sameEvidenceRecords(expectedSlashingEvidence, slashingEvidenceRecords)) {
        throw std::invalid_argument("Finalized block slashing evidence records do not match rebuilt security evidence.");
    }

    const std::vector<SlashingPreparationRecord> expectedSlashingPreparation =
        SlashingEvidence::buildPreparationRecords(
            expectedSlashingEvidence,
            lockedStakePositions
        );

    if (!SlashingEvidence::samePreparationRecords(expectedSlashingPreparation, slashingPreparationRecords)) {
        throw std::invalid_argument("Finalized block slashing preparation records do not match rebuilt evidence.");
    }

    const SlashingEvidenceSummary expectedSlashingSummary =
        SlashingEvidence::buildSummary(
            block.index(),
            expectedSlashingEvidence,
            expectedSlashingPreparation
        );

    if (!SlashingEvidence::sameSummary(expectedSlashingSummary, slashingEvidenceSummary)) {
        throw std::invalid_argument("Finalized block slashing evidence summary does not match rebuilt evidence.");
    }

    const std::vector<CryptographicSlashingEvidenceRecord> expectedCryptographicEvidence =
        CryptographicSlashing::buildEvidenceRecordsFromCertifiedVotes(
            quorumCertificate.votes()
        );

    if (!CryptographicSlashing::sameEvidenceRecords(expectedCryptographicEvidence, cryptographicSlashingEvidenceRecords)) {
        throw std::invalid_argument("Finalized block cryptographic slashing evidence does not match rebuilt vote evidence.");
    }

    const std::vector<StakePenaltyRecord> expectedStakePenalties =
        CryptographicSlashing::buildStakePenaltyRecords(
            expectedCryptographicEvidence,
            lockedStakePositions
        );

    if (!CryptographicSlashing::sameStakePenaltyRecords(expectedStakePenalties, stakePenaltyRecords)) {
        throw std::invalid_argument("Finalized block stake penalty records do not match rebuilt cryptographic evidence.");
    }

    const CryptographicSlashingSummary expectedCryptographicSlashingSummary =
        CryptographicSlashing::buildSummary(
            block.index(),
            expectedCryptographicEvidence,
            expectedStakePenalties
        );

    if (!CryptographicSlashing::sameSummary(expectedCryptographicSlashingSummary, cryptographicSlashingSummary)) {
        throw std::invalid_argument("Finalized block cryptographic slashing summary does not match rebuilt evidence.");
    }

    std::vector<std::pair<std::string, std::string>> canonicalFields = {
        {"blockIndex", document.requireField("blockIndex")},
        {"blockHash", document.requireField("blockHash")},
        {"previousHash", document.requireField("previousHash")},
        {"postStateRoot", postStateRoot},
        {"totalFeeRawUnits", std::to_string(totalFee.rawUnits())},
        {"rewardDistributionCount", std::to_string(rewardDistributions.size())},
        {"lockedStakePositionCount", std::to_string(lockedStakePositions.size())},
        {"securityScoreRecordCount", std::to_string(securityScoreRecords.size())},
        {"securityCheckpointCount", std::to_string(securityCheckpoints.size())},
        {"validatorRiskAssessmentCount", std::to_string(validatorRiskAssessments.size())},
        {"validatorContainmentDecisionCount", std::to_string(validatorContainmentDecisions.size())},
        {"validatorNetworkPolicyCount", std::to_string(validatorNetworkPolicies.size())},
        {"monetaryFirewallStatus", monetaryFirewallAudit.status()},
        {"genesisTreasuryStatus", genesisTreasurySnapshot.status()},
        {"protectionRewardBudgetStatus", protectionRewardBudget.status()},
        {"protectionRewardGrantCount", std::to_string(protectionRewardGrants.size())},
        {"protectionWorkRecordCount", std::to_string(protectionWorkRecords.size())},
        {"protectionRewardSummaryStatus", protectionRewardSummary.status()},
        {"protectionRewardSettlementCount", std::to_string(protectionRewardSettlements.size())},
        {"inflationEpochStatus", inflationEpochSnapshot.status()},
        {"mintAuthorizationStatus", mintAuthorizationRecord.status()},
        {"supplyExpansionStatus", supplyExpansionRecord.status()},
        {"feeEconomicBalanceStatus", feeEconomicBalance.status()},
        {"feeBurnStatus", feeBurnRecord.status()},
        {"treasuryFeeStatus", treasuryFeeRecord.status()},
        {"slashingEvidenceRecordCount", std::to_string(slashingEvidenceRecords.size())},
        {"slashingPreparationRecordCount", std::to_string(slashingPreparationRecords.size())},
        {"slashingEvidenceSummaryStatus", slashingEvidenceSummary.status()},
        {"cryptographicSlashingEvidenceCount", std::to_string(cryptographicSlashingEvidenceRecords.size())},
        {"stakePenaltyRecordCount", std::to_string(stakePenaltyRecords.size())},
        {"cryptographicSlashingSummaryStatus", cryptographicSlashingSummary.status()},
        {"governancePolicyStatus", governancePolicySnapshot.status()},
        {"governanceActionGuardCount", std::to_string(governanceActionGuards.size())},
        {"governanceSummaryStatus", governanceSummary.status()},
        {"validatorLifecycleRecordCount", std::to_string(validatorLifecycleRecords.size())},
        {"epochAccountingStatus", epochAccountingRecord.status()},
        {"validatorLifecycleSummaryStatus", validatorLifecycleSummary.status()},
        {"timestamp", document.requireField("timestamp")},
        {"recordCount", document.requireField("recordCount")}
    };

    for (std::size_t recordIndex = 0; recordIndex < recordCount; ++recordIndex) {
        const std::string key = "record." + std::to_string(recordIndex);
        canonicalFields.emplace_back(key, document.requireField(key));
    }

    for (std::size_t rewardIndex = 0; rewardIndex < rewardDistributions.size(); ++rewardIndex) {
        const std::string prefix = "reward." + std::to_string(rewardIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", rewardDistributions[rewardIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(rewardDistributions[rewardIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "totalRewardRawUnits", std::to_string(rewardDistributions[rewardIndex].totalReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "liquidRewardRawUnits", std::to_string(rewardDistributions[rewardIndex].liquidReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "lockedRewardRawUnits", std::to_string(rewardDistributions[rewardIndex].lockedReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "reason", rewardDistributions[rewardIndex].reason());
    }

    for (std::size_t positionIndex = 0; positionIndex < lockedStakePositions.size(); ++positionIndex) {
        const std::string prefix = "lockedStake." + std::to_string(positionIndex) + ".";
        canonicalFields.emplace_back(prefix + "ownerAddress", lockedStakePositions[positionIndex].ownerAddress());
        canonicalFields.emplace_back(prefix + "amountRawUnits", std::to_string(lockedStakePositions[positionIndex].amount().rawUnits()));
        canonicalFields.emplace_back(prefix + "createdAtHeight", std::to_string(lockedStakePositions[positionIndex].createdAtHeight()));
        canonicalFields.emplace_back(prefix + "unlockAtHeight", std::to_string(lockedStakePositions[positionIndex].unlockAtHeight()));
        canonicalFields.emplace_back(prefix + "slashable", lockedStakePositions[positionIndex].slashable() ? "true" : "false");
        canonicalFields.emplace_back(prefix + "sourceRewardId", lockedStakePositions[positionIndex].sourceRewardId());
    }

    for (std::size_t scoreIndex = 0; scoreIndex < securityScoreRecords.size(); ++scoreIndex) {
        const std::string prefix = "securityScore." + std::to_string(scoreIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", securityScoreRecords[scoreIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(securityScoreRecords[scoreIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "score", std::to_string(securityScoreRecords[scoreIndex].score()));
        canonicalFields.emplace_back(prefix + "lockedStakeScore", std::to_string(securityScoreRecords[scoreIndex].lockedStakeScore()));
        canonicalFields.emplace_back(prefix + "participationScore", std::to_string(securityScoreRecords[scoreIndex].participationScore()));
        canonicalFields.emplace_back(prefix + "maturityScore", std::to_string(securityScoreRecords[scoreIndex].maturityScore()));
        canonicalFields.emplace_back(prefix + "penaltyScore", std::to_string(securityScoreRecords[scoreIndex].penaltyScore()));
        canonicalFields.emplace_back(prefix + "reason", securityScoreRecords[scoreIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceLockedStakeId", securityScoreRecords[scoreIndex].sourceLockedStakeId());
    }

    for (std::size_t checkpointIndex = 0; checkpointIndex < securityCheckpoints.size(); ++checkpointIndex) {
        const std::string prefix = "securityCheckpoint." + std::to_string(checkpointIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", securityCheckpoints[checkpointIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(securityCheckpoints[checkpointIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "score", std::to_string(securityCheckpoints[checkpointIndex].score()));
        canonicalFields.emplace_back(prefix + "band", securityCheckpoints[checkpointIndex].band());
        canonicalFields.emplace_back(prefix + "lockedStakeRawUnits", std::to_string(securityCheckpoints[checkpointIndex].lockedStake().rawUnits()));
        canonicalFields.emplace_back(prefix + "securityScoreRecordCount", std::to_string(securityCheckpoints[checkpointIndex].securityScoreRecordCount()));
        canonicalFields.emplace_back(prefix + "reason", securityCheckpoints[checkpointIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceDigest", securityCheckpoints[checkpointIndex].sourceDigest());
    }

    for (std::size_t assessmentIndex = 0; assessmentIndex < validatorRiskAssessments.size(); ++assessmentIndex) {
        const std::string prefix = "validatorRisk." + std::to_string(assessmentIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", validatorRiskAssessments[assessmentIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(validatorRiskAssessments[assessmentIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "score", std::to_string(validatorRiskAssessments[assessmentIndex].score()));
        canonicalFields.emplace_back(prefix + "band", validatorRiskAssessments[assessmentIndex].band());
        canonicalFields.emplace_back(prefix + "lockedStakeRawUnits", std::to_string(validatorRiskAssessments[assessmentIndex].lockedStake().rawUnits()));
        canonicalFields.emplace_back(prefix + "riskScore", std::to_string(validatorRiskAssessments[assessmentIndex].riskScore()));
        canonicalFields.emplace_back(prefix + "riskLevel", validatorRiskAssessments[assessmentIndex].riskLevel());
        canonicalFields.emplace_back(prefix + "recommendedAction", validatorRiskAssessments[assessmentIndex].recommendedAction());
        canonicalFields.emplace_back(prefix + "reason", validatorRiskAssessments[assessmentIndex].reason());
        canonicalFields.emplace_back(prefix + "checkpointDigest", validatorRiskAssessments[assessmentIndex].checkpointDigest());
    }

    for (std::size_t decisionIndex = 0; decisionIndex < validatorContainmentDecisions.size(); ++decisionIndex) {
        const std::string prefix = "validatorContainment." + std::to_string(decisionIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", validatorContainmentDecisions[decisionIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(validatorContainmentDecisions[decisionIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "riskLevel", validatorContainmentDecisions[decisionIndex].riskLevel());
        canonicalFields.emplace_back(prefix + "recommendedAction", validatorContainmentDecisions[decisionIndex].recommendedAction());
        canonicalFields.emplace_back(prefix + "containmentMode", validatorContainmentDecisions[decisionIndex].containmentMode());
        canonicalFields.emplace_back(prefix + "peerTrustState", validatorContainmentDecisions[decisionIndex].peerTrustState());
        canonicalFields.emplace_back(prefix + "networkAdmissionState", validatorContainmentDecisions[decisionIndex].networkAdmissionState());
        canonicalFields.emplace_back(prefix + "reason", validatorContainmentDecisions[decisionIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceRiskDigest", validatorContainmentDecisions[decisionIndex].sourceRiskDigest());
    }

    for (std::size_t policyIndex = 0; policyIndex < validatorNetworkPolicies.size(); ++policyIndex) {
        const std::string prefix = "validatorNetworkPolicy." + std::to_string(policyIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", validatorNetworkPolicies[policyIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(validatorNetworkPolicies[policyIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "containmentMode", validatorNetworkPolicies[policyIndex].containmentMode());
        canonicalFields.emplace_back(prefix + "peerTrustState", validatorNetworkPolicies[policyIndex].peerTrustState());
        canonicalFields.emplace_back(prefix + "networkAdmissionState", validatorNetworkPolicies[policyIndex].networkAdmissionState());
        canonicalFields.emplace_back(prefix + "connectionPolicy", validatorNetworkPolicies[policyIndex].connectionPolicy());
        canonicalFields.emplace_back(prefix + "messagePolicy", validatorNetworkPolicies[policyIndex].messagePolicy());
        canonicalFields.emplace_back(prefix + "consensusPolicy", validatorNetworkPolicies[policyIndex].consensusPolicy());
        canonicalFields.emplace_back(prefix + "requiresManualReview", validatorNetworkPolicies[policyIndex].requiresManualReview() ? "true" : "false");
        canonicalFields.emplace_back(prefix + "reason", validatorNetworkPolicies[policyIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceContainmentDigest", validatorNetworkPolicies[policyIndex].sourceContainmentDigest());
    }

    canonicalFields.emplace_back("monetary.blockHeight", std::to_string(monetaryFirewallAudit.supplyLedger().blockHeight()));
    canonicalFields.emplace_back("monetary.supplyBeforeRawUnits", std::to_string(monetaryFirewallAudit.supplyLedger().supplyBefore().rawUnits()));
    canonicalFields.emplace_back("monetary.mintedRawUnits", std::to_string(monetaryFirewallAudit.supplyLedger().minted().rawUnits()));
    canonicalFields.emplace_back("monetary.burnedRawUnits", std::to_string(monetaryFirewallAudit.supplyLedger().burned().rawUnits()));
    canonicalFields.emplace_back("monetary.treasuryDeltaRawUnits", std::to_string(monetaryFirewallAudit.supplyLedger().treasuryDelta().rawUnits()));
    canonicalFields.emplace_back("monetary.supplyAfterRawUnits", std::to_string(monetaryFirewallAudit.supplyLedger().supplyAfter().rawUnits()));
    canonicalFields.emplace_back("monetary.annualMintLimitRawUnits", std::to_string(monetaryFirewallAudit.annualMintLimit().rawUnits()));
    canonicalFields.emplace_back("monetary.annualMintUsedBeforeRawUnits", std::to_string(monetaryFirewallAudit.annualMintUsedBefore().rawUnits()));
    canonicalFields.emplace_back("monetary.annualMintUsedAfterRawUnits", std::to_string(monetaryFirewallAudit.annualMintUsedAfter().rawUnits()));
    canonicalFields.emplace_back("monetary.policyId", monetaryFirewallAudit.policyId());
    canonicalFields.emplace_back("monetary.reason", monetaryFirewallAudit.reason());

    canonicalFields.emplace_back("treasury.treasuryAddress", genesisTreasurySnapshot.treasuryAddress());
    canonicalFields.emplace_back("treasury.blockHeight", std::to_string(genesisTreasurySnapshot.blockHeight()));
    canonicalFields.emplace_back("treasury.genesisTreasuryBalanceRawUnits", std::to_string(genesisTreasurySnapshot.genesisTreasuryBalance().rawUnits()));
    canonicalFields.emplace_back("treasury.protectedReserveRawUnits", std::to_string(genesisTreasurySnapshot.protectedReserve().rawUnits()));
    canonicalFields.emplace_back("treasury.protectionBudgetRawUnits", std::to_string(genesisTreasurySnapshot.protectionBudget().rawUnits()));
    canonicalFields.emplace_back("treasury.availableBalanceRawUnits", std::to_string(genesisTreasurySnapshot.availableBalance().rawUnits()));
    canonicalFields.emplace_back("treasury.reason", genesisTreasurySnapshot.reason());

    canonicalFields.emplace_back("protectionBudget.blockHeight", std::to_string(protectionRewardBudget.blockHeight()));
    canonicalFields.emplace_back("protectionBudget.treasuryAddress", protectionRewardBudget.treasuryAddress());
    canonicalFields.emplace_back("protectionBudget.availableBudgetRawUnits", std::to_string(protectionRewardBudget.availableBudget().rawUnits()));
    canonicalFields.emplace_back("protectionBudget.plannedTotalRawUnits", std::to_string(protectionRewardBudget.plannedTotal().rawUnits()));
    canonicalFields.emplace_back("protectionBudget.remainingBudgetRawUnits", std::to_string(protectionRewardBudget.remainingBudget().rawUnits()));
    canonicalFields.emplace_back("protectionBudget.beneficiaryCount", std::to_string(protectionRewardBudget.beneficiaryCount()));
    canonicalFields.emplace_back("protectionBudget.reason", protectionRewardBudget.reason());
    canonicalFields.emplace_back("protectionBudget.sourceTreasuryDigest", protectionRewardBudget.sourceTreasuryDigest());

    for (std::size_t grantIndex = 0; grantIndex < protectionRewardGrants.size(); ++grantIndex) {
        const std::string prefix = "protectionGrant." + std::to_string(grantIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", protectionRewardGrants[grantIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(protectionRewardGrants[grantIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "plannedRewardRawUnits", std::to_string(protectionRewardGrants[grantIndex].plannedReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "securityScore", std::to_string(protectionRewardGrants[grantIndex].securityScore()));
        canonicalFields.emplace_back(prefix + "reason", protectionRewardGrants[grantIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceBudgetDigest", protectionRewardGrants[grantIndex].sourceBudgetDigest());
    }

    for (std::size_t workIndex = 0; workIndex < protectionWorkRecords.size(); ++workIndex) {
        const std::string prefix = "protectionWork." + std::to_string(workIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", protectionWorkRecords[workIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(protectionWorkRecords[workIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "uptimeScore", std::to_string(protectionWorkRecords[workIndex].uptimeScore()));
        canonicalFields.emplace_back(prefix + "correctVoteScore", std::to_string(protectionWorkRecords[workIndex].correctVoteScore()));
        canonicalFields.emplace_back(prefix + "attackDetectionScore", std::to_string(protectionWorkRecords[workIndex].attackDetectionScore()));
        canonicalFields.emplace_back(prefix + "auditContributionScore", std::to_string(protectionWorkRecords[workIndex].auditContributionScore()));
        canonicalFields.emplace_back(prefix + "securityScore", std::to_string(protectionWorkRecords[workIndex].securityScore()));
        canonicalFields.emplace_back(prefix + "riskPenaltyScore", std::to_string(protectionWorkRecords[workIndex].riskPenaltyScore()));
        canonicalFields.emplace_back(prefix + "totalWorkScore", std::to_string(protectionWorkRecords[workIndex].totalWorkScore()));
        canonicalFields.emplace_back(prefix + "reason", protectionWorkRecords[workIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceSecurityDigest", protectionWorkRecords[workIndex].sourceSecurityDigest());
    }

    canonicalFields.emplace_back("protectionSummary.blockHeight", std::to_string(protectionRewardSummary.blockHeight()));
    canonicalFields.emplace_back("protectionSummary.plannedTotalRawUnits", std::to_string(protectionRewardSummary.plannedTotal().rawUnits()));
    canonicalFields.emplace_back("protectionSummary.earnedTotalRawUnits", std::to_string(protectionRewardSummary.earnedTotal().rawUnits()));
    canonicalFields.emplace_back("protectionSummary.deferredTotalRawUnits", std::to_string(protectionRewardSummary.deferredTotal().rawUnits()));
    canonicalFields.emplace_back("protectionSummary.beneficiaryCount", std::to_string(protectionRewardSummary.beneficiaryCount()));
    canonicalFields.emplace_back("protectionSummary.reason", protectionRewardSummary.reason());
    canonicalFields.emplace_back("protectionSummary.sourceBudgetDigest", protectionRewardSummary.sourceBudgetDigest());

    for (std::size_t settlementIndex = 0; settlementIndex < protectionRewardSettlements.size(); ++settlementIndex) {
        const std::string prefix = "protectionSettlement." + std::to_string(settlementIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", protectionRewardSettlements[settlementIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(protectionRewardSettlements[settlementIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "plannedRewardRawUnits", std::to_string(protectionRewardSettlements[settlementIndex].plannedReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "earnedRewardRawUnits", std::to_string(protectionRewardSettlements[settlementIndex].earnedReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "deferredRewardRawUnits", std::to_string(protectionRewardSettlements[settlementIndex].deferredReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "workScore", std::to_string(protectionRewardSettlements[settlementIndex].workScore()));
        canonicalFields.emplace_back(prefix + "securityScore", std::to_string(protectionRewardSettlements[settlementIndex].securityScore()));
        canonicalFields.emplace_back(prefix + "reason", protectionRewardSettlements[settlementIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceGrantDigest", protectionRewardSettlements[settlementIndex].sourceGrantDigest());
        canonicalFields.emplace_back(prefix + "sourceWorkDigest", protectionRewardSettlements[settlementIndex].sourceWorkDigest());
    }

    canonicalFields.emplace_back("inflationEpoch.blockHeight", std::to_string(inflationEpochSnapshot.blockHeight()));
    canonicalFields.emplace_back("inflationEpoch.epochStartBlock", std::to_string(inflationEpochSnapshot.epochStartBlock()));
    canonicalFields.emplace_back("inflationEpoch.epochEndBlock", std::to_string(inflationEpochSnapshot.epochEndBlock()));
    canonicalFields.emplace_back("inflationEpoch.maxAnnualInflationBasisPoints", std::to_string(inflationEpochSnapshot.maxAnnualInflationBasisPoints()));
    canonicalFields.emplace_back("inflationEpoch.baseSupplyRawUnits", std::to_string(inflationEpochSnapshot.baseSupply().rawUnits()));
    canonicalFields.emplace_back("inflationEpoch.annualMintLimitRawUnits", std::to_string(inflationEpochSnapshot.annualMintLimit().rawUnits()));
    canonicalFields.emplace_back("inflationEpoch.mintedThisEpochRawUnits", std::to_string(inflationEpochSnapshot.mintedThisEpoch().rawUnits()));
    canonicalFields.emplace_back("inflationEpoch.remainingMintCapacityRawUnits", std::to_string(inflationEpochSnapshot.remainingMintCapacity().rawUnits()));
    canonicalFields.emplace_back("inflationEpoch.policyId", inflationEpochSnapshot.policyId());
    canonicalFields.emplace_back("inflationEpoch.reason", inflationEpochSnapshot.reason());

    canonicalFields.emplace_back("mintAuthorization.blockHeight", std::to_string(mintAuthorizationRecord.blockHeight()));
    canonicalFields.emplace_back("mintAuthorization.authorizationId", mintAuthorizationRecord.authorizationId());
    canonicalFields.emplace_back("mintAuthorization.authorizedAmountRawUnits", std::to_string(mintAuthorizationRecord.authorizedAmount().rawUnits()));
    canonicalFields.emplace_back("mintAuthorization.activationBlock", std::to_string(mintAuthorizationRecord.activationBlock()));
    canonicalFields.emplace_back("mintAuthorization.expirationBlock", std::to_string(mintAuthorizationRecord.expirationBlock()));
    canonicalFields.emplace_back("mintAuthorization.requiredApprovalBasisPoints", std::to_string(mintAuthorizationRecord.requiredApprovalBasisPoints()));
    canonicalFields.emplace_back("mintAuthorization.timelockBlocks", std::to_string(mintAuthorizationRecord.timelockBlocks()));
    canonicalFields.emplace_back("mintAuthorization.governanceDigest", mintAuthorizationRecord.governanceDigest());
    canonicalFields.emplace_back("mintAuthorization.reason", mintAuthorizationRecord.reason());
    canonicalFields.emplace_back("mintAuthorization.sourceEpochDigest", mintAuthorizationRecord.sourceEpochDigest());

    canonicalFields.emplace_back("supplyExpansion.blockHeight", std::to_string(supplyExpansionRecord.blockHeight()));
    canonicalFields.emplace_back("supplyExpansion.mintedAmountRawUnits", std::to_string(supplyExpansionRecord.mintedAmount().rawUnits()));
    canonicalFields.emplace_back("supplyExpansion.recipientAddress", supplyExpansionRecord.recipientAddress());
    canonicalFields.emplace_back("supplyExpansion.authorizationId", supplyExpansionRecord.authorizationId());
    canonicalFields.emplace_back("supplyExpansion.policyId", supplyExpansionRecord.policyId());
    canonicalFields.emplace_back("supplyExpansion.reason", supplyExpansionRecord.reason());
    canonicalFields.emplace_back("supplyExpansion.sourceAuthorizationDigest", supplyExpansionRecord.sourceAuthorizationDigest());

    canonicalFields.emplace_back("feeBalance.blockHeight", std::to_string(feeEconomicBalance.blockHeight()));
    canonicalFields.emplace_back("feeBalance.totalFeeRawUnits", std::to_string(feeEconomicBalance.totalFee().rawUnits()));
    canonicalFields.emplace_back("feeBalance.validatorRewardRawUnits", std::to_string(feeEconomicBalance.validatorRewardAmount().rawUnits()));
    canonicalFields.emplace_back("feeBalance.treasuryRawUnits", std::to_string(feeEconomicBalance.treasuryAmount().rawUnits()));
    canonicalFields.emplace_back("feeBalance.burnRawUnits", std::to_string(feeEconomicBalance.burnAmount().rawUnits()));
    canonicalFields.emplace_back("feeBalance.policyId", feeEconomicBalance.policyId());
    canonicalFields.emplace_back("feeBalance.reason", feeEconomicBalance.reason());

    canonicalFields.emplace_back("feeBurn.blockHeight", std::to_string(feeBurnRecord.blockHeight()));
    canonicalFields.emplace_back("feeBurn.burnAmountRawUnits", std::to_string(feeBurnRecord.burnAmount().rawUnits()));
    canonicalFields.emplace_back("feeBurn.supplyBeforeRawUnits", std::to_string(feeBurnRecord.supplyBefore().rawUnits()));
    canonicalFields.emplace_back("feeBurn.supplyAfterRawUnits", std::to_string(feeBurnRecord.supplyAfter().rawUnits()));
    canonicalFields.emplace_back("feeBurn.reason", feeBurnRecord.reason());
    canonicalFields.emplace_back("feeBurn.sourceFeeBalanceDigest", feeBurnRecord.sourceFeeBalanceDigest());

    canonicalFields.emplace_back("treasuryFee.blockHeight", std::to_string(treasuryFeeRecord.blockHeight()));
    canonicalFields.emplace_back("treasuryFee.treasuryAddress", treasuryFeeRecord.treasuryAddress());
    canonicalFields.emplace_back("treasuryFee.treasuryAmountRawUnits", std::to_string(treasuryFeeRecord.treasuryAmount().rawUnits()));
    canonicalFields.emplace_back("treasuryFee.reason", treasuryFeeRecord.reason());
    canonicalFields.emplace_back("treasuryFee.sourceFeeBalanceDigest", treasuryFeeRecord.sourceFeeBalanceDigest());

    for (std::size_t evidenceIndex = 0; evidenceIndex < slashingEvidenceRecords.size(); ++evidenceIndex) {
        const std::string prefix = "slashingEvidence." + std::to_string(evidenceIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", slashingEvidenceRecords[evidenceIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(slashingEvidenceRecords[evidenceIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "evidenceType", slashingEvidenceRecords[evidenceIndex].evidenceType());
        canonicalFields.emplace_back(prefix + "severityScore", std::to_string(slashingEvidenceRecords[evidenceIndex].severityScore()));
        canonicalFields.emplace_back(prefix + "slashable", slashingEvidenceRecords[evidenceIndex].slashable() ? "true" : "false");
        canonicalFields.emplace_back(prefix + "recommendedAction", slashingEvidenceRecords[evidenceIndex].recommendedAction());
        canonicalFields.emplace_back(prefix + "reason", slashingEvidenceRecords[evidenceIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceSecurityDigest", slashingEvidenceRecords[evidenceIndex].sourceSecurityDigest());
    }

    for (std::size_t preparationIndex = 0; preparationIndex < slashingPreparationRecords.size(); ++preparationIndex) {
        const std::string prefix = "slashingPreparation." + std::to_string(preparationIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", slashingPreparationRecords[preparationIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(slashingPreparationRecords[preparationIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "evidenceCount", std::to_string(slashingPreparationRecords[preparationIndex].evidenceCount()));
        canonicalFields.emplace_back(prefix + "slashableEvidenceCount", std::to_string(slashingPreparationRecords[preparationIndex].slashableEvidenceCount()));
        canonicalFields.emplace_back(prefix + "maxSeverityScore", std::to_string(slashingPreparationRecords[preparationIndex].maxSeverityScore()));
        canonicalFields.emplace_back(prefix + "preparedPenaltyRawUnits", std::to_string(slashingPreparationRecords[preparationIndex].preparedPenaltyAmount().rawUnits()));
        canonicalFields.emplace_back(prefix + "enforcementAction", slashingPreparationRecords[preparationIndex].enforcementAction());
        canonicalFields.emplace_back(prefix + "reason", slashingPreparationRecords[preparationIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceEvidenceDigest", slashingPreparationRecords[preparationIndex].sourceEvidenceDigest());
    }

    canonicalFields.emplace_back("slashingSummary.blockHeight", std::to_string(slashingEvidenceSummary.blockHeight()));
    canonicalFields.emplace_back("slashingSummary.evidenceCount", std::to_string(slashingEvidenceSummary.evidenceCount()));
    canonicalFields.emplace_back("slashingSummary.slashableEvidenceCount", std::to_string(slashingEvidenceSummary.slashableEvidenceCount()));
    canonicalFields.emplace_back("slashingSummary.maxSeverityScore", std::to_string(slashingEvidenceSummary.maxSeverityScore()));
    canonicalFields.emplace_back("slashingSummary.preparedPenaltyTotalRawUnits", std::to_string(slashingEvidenceSummary.preparedPenaltyTotal().rawUnits()));
    canonicalFields.emplace_back("slashingSummary.reason", slashingEvidenceSummary.reason());
    canonicalFields.emplace_back("slashingSummary.sourcePreparationDigest", slashingEvidenceSummary.sourcePreparationDigest());

    for (std::size_t evidenceIndex = 0; evidenceIndex < cryptographicSlashingEvidenceRecords.size(); ++evidenceIndex) {
        const std::string prefix = "cryptographicSlashingEvidence." + std::to_string(evidenceIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", cryptographicSlashingEvidenceRecords[evidenceIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(cryptographicSlashingEvidenceRecords[evidenceIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "round", std::to_string(cryptographicSlashingEvidenceRecords[evidenceIndex].round()));
        canonicalFields.emplace_back(prefix + "evidenceType", cryptographicSlashingEvidenceRecords[evidenceIndex].evidenceType());
        canonicalFields.emplace_back(prefix + "severityScore", std::to_string(cryptographicSlashingEvidenceRecords[evidenceIndex].severityScore()));
        canonicalFields.emplace_back(prefix + "penaltyBasisPoints", std::to_string(cryptographicSlashingEvidenceRecords[evidenceIndex].penaltyBasisPoints()));
        canonicalFields.emplace_back(prefix + "firstVoteDigest", cryptographicSlashingEvidenceRecords[evidenceIndex].firstVoteDigest());
        canonicalFields.emplace_back(prefix + "secondVoteDigest", cryptographicSlashingEvidenceRecords[evidenceIndex].secondVoteDigest());
        canonicalFields.emplace_back(prefix + "reason", cryptographicSlashingEvidenceRecords[evidenceIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceEvidenceDigest", cryptographicSlashingEvidenceRecords[evidenceIndex].sourceEvidenceDigest());
    }

    for (std::size_t penaltyIndex = 0; penaltyIndex < stakePenaltyRecords.size(); ++penaltyIndex) {
        const std::string prefix = "stakePenalty." + std::to_string(penaltyIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", stakePenaltyRecords[penaltyIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(stakePenaltyRecords[penaltyIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "lockedStakeBeforeRawUnits", std::to_string(stakePenaltyRecords[penaltyIndex].lockedStakeBefore().rawUnits()));
        canonicalFields.emplace_back(prefix + "penaltyAmountRawUnits", std::to_string(stakePenaltyRecords[penaltyIndex].penaltyAmount().rawUnits()));
        canonicalFields.emplace_back(prefix + "lockedStakeAfterRawUnits", std::to_string(stakePenaltyRecords[penaltyIndex].lockedStakeAfter().rawUnits()));
        canonicalFields.emplace_back(prefix + "evidenceCount", std::to_string(stakePenaltyRecords[penaltyIndex].evidenceCount()));
        canonicalFields.emplace_back(prefix + "reason", stakePenaltyRecords[penaltyIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceEvidenceDigest", stakePenaltyRecords[penaltyIndex].sourceEvidenceDigest());
    }

    canonicalFields.emplace_back("cryptographicSlashingSummary.blockHeight", std::to_string(cryptographicSlashingSummary.blockHeight()));
    canonicalFields.emplace_back("cryptographicSlashingSummary.evidenceCount", std::to_string(cryptographicSlashingSummary.evidenceCount()));
    canonicalFields.emplace_back("cryptographicSlashingSummary.slashableEvidenceCount", std::to_string(cryptographicSlashingSummary.slashableEvidenceCount()));
    canonicalFields.emplace_back("cryptographicSlashingSummary.maxSeverityScore", std::to_string(cryptographicSlashingSummary.maxSeverityScore()));
    canonicalFields.emplace_back("cryptographicSlashingSummary.penaltyTotalRawUnits", std::to_string(cryptographicSlashingSummary.penaltyTotal().rawUnits()));
    canonicalFields.emplace_back("cryptographicSlashingSummary.reason", cryptographicSlashingSummary.reason());
    canonicalFields.emplace_back("cryptographicSlashingSummary.sourcePenaltyDigest", cryptographicSlashingSummary.sourcePenaltyDigest());

    canonicalFields.emplace_back("governancePolicy.blockHeight", std::to_string(governancePolicySnapshot.blockHeight()));
    canonicalFields.emplace_back("governancePolicy.requiredApprovalBasisPoints", std::to_string(governancePolicySnapshot.requiredApprovalBasisPoints()));
    canonicalFields.emplace_back("governancePolicy.timelockBlocks", std::to_string(governancePolicySnapshot.timelockBlocks()));
    canonicalFields.emplace_back("governancePolicy.activationDelayBlocks", std::to_string(governancePolicySnapshot.activationDelayBlocks()));
    canonicalFields.emplace_back("governancePolicy.policyId", governancePolicySnapshot.policyId());
    canonicalFields.emplace_back("governancePolicy.reason", governancePolicySnapshot.reason());

    for (std::size_t guardIndex = 0; guardIndex < governanceActionGuards.size(); ++guardIndex) {
        const std::string prefix = "governanceGuard." + std::to_string(guardIndex) + ".";
        canonicalFields.emplace_back(prefix + "actionType", governanceActionGuards[guardIndex].actionType());
        canonicalFields.emplace_back(prefix + "status", governanceActionGuards[guardIndex].status());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(governanceActionGuards[guardIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "protectedResource", governanceActionGuards[guardIndex].protectedResource());
        canonicalFields.emplace_back(prefix + "requiredApprovalBasisPoints", std::to_string(governanceActionGuards[guardIndex].requiredApprovalBasisPoints()));
        canonicalFields.emplace_back(prefix + "timelockBlocks", std::to_string(governanceActionGuards[guardIndex].timelockBlocks()));
        canonicalFields.emplace_back(prefix + "reason", governanceActionGuards[guardIndex].reason());
        canonicalFields.emplace_back(prefix + "sourcePolicyDigest", governanceActionGuards[guardIndex].sourcePolicyDigest());
    }

    canonicalFields.emplace_back("governanceSummary.blockHeight", std::to_string(governanceSummary.blockHeight()));
    canonicalFields.emplace_back("governanceSummary.guardCount", std::to_string(governanceSummary.guardCount()));
    canonicalFields.emplace_back("governanceSummary.activeProposalCount", std::to_string(governanceSummary.activeProposalCount()));
    canonicalFields.emplace_back("governanceSummary.approvedProposalCount", std::to_string(governanceSummary.approvedProposalCount()));
    canonicalFields.emplace_back("governanceSummary.executableProposalCount", std::to_string(governanceSummary.executableProposalCount()));
    canonicalFields.emplace_back("governanceSummary.executedProposalCount", std::to_string(governanceSummary.executedProposalCount()));
    canonicalFields.emplace_back("governanceSummary.reason", governanceSummary.reason());
    canonicalFields.emplace_back("governanceSummary.sourceGuardDigest", governanceSummary.sourceGuardDigest());

    for (std::size_t lifecycleIndex = 0; lifecycleIndex < validatorLifecycleRecords.size(); ++lifecycleIndex) {
        const std::string prefix = "validatorLifecycle." + std::to_string(lifecycleIndex) + ".";
        canonicalFields.emplace_back(prefix + "validatorAddress", validatorLifecycleRecords[lifecycleIndex].validatorAddress());
        canonicalFields.emplace_back(prefix + "blockHeight", std::to_string(validatorLifecycleRecords[lifecycleIndex].blockHeight()));
        canonicalFields.emplace_back(prefix + "epochIndex", std::to_string(validatorLifecycleRecords[lifecycleIndex].epochIndex()));
        canonicalFields.emplace_back(prefix + "lifecycleStatus", validatorLifecycleRecords[lifecycleIndex].lifecycleStatus());
        canonicalFields.emplace_back(prefix + "lockedStakeRawUnits", std::to_string(validatorLifecycleRecords[lifecycleIndex].lockedStake().rawUnits()));
        canonicalFields.emplace_back(prefix + "earnedRewardRawUnits", std::to_string(validatorLifecycleRecords[lifecycleIndex].earnedReward().rawUnits()));
        canonicalFields.emplace_back(prefix + "slashingPenaltyRawUnits", std::to_string(validatorLifecycleRecords[lifecycleIndex].slashingPenalty().rawUnits()));
        canonicalFields.emplace_back(prefix + "securityScore", std::to_string(validatorLifecycleRecords[lifecycleIndex].securityScore()));
        canonicalFields.emplace_back(prefix + "reason", validatorLifecycleRecords[lifecycleIndex].reason());
        canonicalFields.emplace_back(prefix + "sourceDigest", validatorLifecycleRecords[lifecycleIndex].sourceDigest());
    }

    canonicalFields.emplace_back("epochAccounting.blockHeight", std::to_string(epochAccountingRecord.blockHeight()));
    canonicalFields.emplace_back("epochAccounting.epochIndex", std::to_string(epochAccountingRecord.epochIndex()));
    canonicalFields.emplace_back("epochAccounting.epochStartBlock", std::to_string(epochAccountingRecord.epochStartBlock()));
    canonicalFields.emplace_back("epochAccounting.epochEndBlock", std::to_string(epochAccountingRecord.epochEndBlock()));
    canonicalFields.emplace_back("epochAccounting.validatorCount", std::to_string(epochAccountingRecord.validatorCount()));
    canonicalFields.emplace_back("epochAccounting.activeValidatorCount", std::to_string(epochAccountingRecord.activeValidatorCount()));
    canonicalFields.emplace_back("epochAccounting.totalLockedStakeRawUnits", std::to_string(epochAccountingRecord.totalLockedStake().rawUnits()));
    canonicalFields.emplace_back("epochAccounting.totalEarnedRewardsRawUnits", std::to_string(epochAccountingRecord.totalEarnedRewards().rawUnits()));
    canonicalFields.emplace_back("epochAccounting.totalSlashingPenaltiesRawUnits", std::to_string(epochAccountingRecord.totalSlashingPenalties().rawUnits()));
    canonicalFields.emplace_back("epochAccounting.reason", epochAccountingRecord.reason());
    canonicalFields.emplace_back("epochAccounting.sourceLifecycleDigest", epochAccountingRecord.sourceLifecycleDigest());

    canonicalFields.emplace_back("validatorLifecycleSummary.blockHeight", std::to_string(validatorLifecycleSummary.blockHeight()));
    canonicalFields.emplace_back("validatorLifecycleSummary.epochIndex", std::to_string(validatorLifecycleSummary.epochIndex()));
    canonicalFields.emplace_back("validatorLifecycleSummary.activeValidatorCount", std::to_string(validatorLifecycleSummary.activeValidatorCount()));
    canonicalFields.emplace_back("validatorLifecycleSummary.jailedValidatorCount", std::to_string(validatorLifecycleSummary.jailedValidatorCount()));
    canonicalFields.emplace_back("validatorLifecycleSummary.slashedValidatorCount", std::to_string(validatorLifecycleSummary.slashedValidatorCount()));
    canonicalFields.emplace_back("validatorLifecycleSummary.totalLockedStakeRawUnits", std::to_string(validatorLifecycleSummary.totalLockedStake().rawUnits()));
    canonicalFields.emplace_back("validatorLifecycleSummary.totalEarnedRewardsRawUnits", std::to_string(validatorLifecycleSummary.totalEarnedRewards().rawUnits()));
    canonicalFields.emplace_back("validatorLifecycleSummary.totalSlashingPenaltiesRawUnits", std::to_string(validatorLifecycleSummary.totalSlashingPenalties().rawUnits()));
    canonicalFields.emplace_back("validatorLifecycleSummary.reason", validatorLifecycleSummary.reason());
    canonicalFields.emplace_back("validatorLifecycleSummary.sourceEpochDigest", validatorLifecycleSummary.sourceEpochDigest());

    canonicalFields.emplace_back("block", serializedBlock);
    canonicalFields.emplace_back("quorumCertificate", quorumCertificate.serialize());
    canonicalFields.emplace_back("finalizedRecord", finalizedRecord.serialize());

    FinalizedMonetarySectionCodec::appendSupplyDeltaFields(
        parsedSupplyDelta,
        canonicalFields
    );

    const std::string canonicalContents =
        serialization::KeyValueFileCodec::serialize(
            FinalizedArtifactSchema::currentSchemaId(),
            canonicalFields
        );

    if (contents != canonicalContents) {
        throw std::invalid_argument("Finalized block file is not canonical.");
    }

    FinalizedBlockArtifact artifact(
        block,
        postStateRoot,
        totalFee,
        rewardDistributions,
        lockedStakePositions,
        securityScoreRecords,
        securityCheckpoints,
        validatorRiskAssessments,
        validatorContainmentDecisions,
        validatorNetworkPolicies,
        monetaryFirewallAudit,
        genesisTreasurySnapshot,
        protectionRewardBudget,
        protectionRewardGrants,
        protectionWorkRecords,
        protectionRewardSummary,
        protectionRewardSettlements,
        inflationEpochSnapshot,
        mintAuthorizationRecord,
        supplyExpansionRecord,
        feeEconomicBalance,
        feeBurnRecord,
        treasuryFeeRecord,
        slashingEvidenceRecords,
        slashingPreparationRecords,
        slashingEvidenceSummary,
        cryptographicSlashingEvidenceRecords,
        stakePenaltyRecords,
        cryptographicSlashingSummary,
        governancePolicySnapshot,
        governanceActionGuards,
        governanceSummary,
        validatorLifecycleRecords,
        epochAccountingRecord,
        validatorLifecycleSummary,
        quorumCertificate,
        finalizedRecord
    );
    artifact.setSupplyDelta(std::move(parsedSupplyDelta));
    return artifact;
}

} // namespace nodo::node
