#include "node/ControlledIssuance.hpp"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

std::uint64_t epochStartForBlock(
    std::uint64_t blockHeight
) {
    if (blockHeight == 0) {
        throw std::invalid_argument("Controlled issuance cannot evaluate genesis height.");
    }

    return ((blockHeight - 1) / NODO_CONTROLLED_ISSUANCE_EPOCH_BLOCKS) *
               NODO_CONTROLLED_ISSUANCE_EPOCH_BLOCKS +
           1;
}

} // namespace

InflationEpochSnapshot::InflationEpochSnapshot()
    : m_status("NOT_EVALUATED"),
      m_blockHeight(0),
      m_epochStartBlock(0),
      m_epochEndBlock(0),
      m_maxAnnualInflationBasisPoints(0),
      m_baseSupply(),
      m_annualMintLimit(),
      m_mintedThisEpoch(),
      m_remainingMintCapacity(),
      m_policyId(""),
      m_reason("") {}

InflationEpochSnapshot::InflationEpochSnapshot(
    std::string status,
    std::uint64_t blockHeight,
    std::uint64_t epochStartBlock,
    std::uint64_t epochEndBlock,
    std::uint32_t maxAnnualInflationBasisPoints,
    utils::Amount baseSupply,
    utils::Amount annualMintLimit,
    utils::Amount mintedThisEpoch,
    utils::Amount remainingMintCapacity,
    std::string policyId,
    std::string reason
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_epochStartBlock(epochStartBlock),
      m_epochEndBlock(epochEndBlock),
      m_maxAnnualInflationBasisPoints(maxAnnualInflationBasisPoints),
      m_baseSupply(baseSupply),
      m_annualMintLimit(annualMintLimit),
      m_mintedThisEpoch(mintedThisEpoch),
      m_remainingMintCapacity(remainingMintCapacity),
      m_policyId(std::move(policyId)),
      m_reason(std::move(reason)) {}

InflationEpochSnapshot InflationEpochSnapshot::notEvaluated() {
    return InflationEpochSnapshot();
}

const std::string& InflationEpochSnapshot::status() const {
    return m_status;
}

std::uint64_t InflationEpochSnapshot::blockHeight() const {
    return m_blockHeight;
}

std::uint64_t InflationEpochSnapshot::epochStartBlock() const {
    return m_epochStartBlock;
}

std::uint64_t InflationEpochSnapshot::epochEndBlock() const {
    return m_epochEndBlock;
}

std::uint32_t InflationEpochSnapshot::maxAnnualInflationBasisPoints() const {
    return m_maxAnnualInflationBasisPoints;
}

utils::Amount InflationEpochSnapshot::baseSupply() const {
    return m_baseSupply;
}

utils::Amount InflationEpochSnapshot::annualMintLimit() const {
    return m_annualMintLimit;
}

utils::Amount InflationEpochSnapshot::mintedThisEpoch() const {
    return m_mintedThisEpoch;
}

utils::Amount InflationEpochSnapshot::remainingMintCapacity() const {
    return m_remainingMintCapacity;
}

const std::string& InflationEpochSnapshot::policyId() const {
    return m_policyId;
}

const std::string& InflationEpochSnapshot::reason() const {
    return m_reason;
}

bool InflationEpochSnapshot::active() const {
    return m_status == "ACTIVE" && isValid();
}

bool InflationEpochSnapshot::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_blockHeight == 0 &&
               m_epochStartBlock == 0 &&
               m_epochEndBlock == 0 &&
               m_policyId.empty() &&
               m_reason.empty();
    }

    if (m_status != "ACTIVE" ||
        m_blockHeight == 0 ||
        m_epochStartBlock == 0 ||
        m_epochEndBlock < m_epochStartBlock ||
        m_blockHeight < m_epochStartBlock ||
        m_blockHeight > m_epochEndBlock ||
        m_maxAnnualInflationBasisPoints > NODO_MAX_ANNUAL_INFLATION_BASIS_POINTS ||
        m_baseSupply.isNegative() ||
        m_annualMintLimit.isNegative() ||
        m_mintedThisEpoch.isNegative() ||
        m_remainingMintCapacity.isNegative() ||
        m_mintedThisEpoch > m_annualMintLimit ||
        m_remainingMintCapacity != m_annualMintLimit - m_mintedThisEpoch ||
        m_policyId.empty() ||
        m_reason != ControlledIssuance::INFLATION_EPOCH_REASON) {
        return false;
    }

    return true;
}

std::string InflationEpochSnapshot::serialize() const {
    std::ostringstream oss;

    oss << "InflationEpochSnapshot{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";epochStartBlock=" << m_epochStartBlock
        << ";epochEndBlock=" << m_epochEndBlock
        << ";maxAnnualInflationBasisPoints=" << m_maxAnnualInflationBasisPoints
        << ";baseSupplyRawUnits=" << m_baseSupply.rawUnits()
        << ";annualMintLimitRawUnits=" << m_annualMintLimit.rawUnits()
        << ";mintedThisEpochRawUnits=" << m_mintedThisEpoch.rawUnits()
        << ";remainingMintCapacityRawUnits=" << m_remainingMintCapacity.rawUnits()
        << ";policyId=" << m_policyId
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

MintAuthorizationRecord::MintAuthorizationRecord()
    : m_status("NONE"),
      m_blockHeight(0),
      m_authorizationId(""),
      m_authorizedAmount(),
      m_activationBlock(0),
      m_expirationBlock(0),
      m_requiredApprovalBasisPoints(0),
      m_timelockBlocks(0),
      m_governanceDigest(""),
      m_reason(""),
      m_sourceEpochDigest("") {}

MintAuthorizationRecord::MintAuthorizationRecord(
    std::string status,
    std::uint64_t blockHeight,
    std::string authorizationId,
    utils::Amount authorizedAmount,
    std::uint64_t activationBlock,
    std::uint64_t expirationBlock,
    std::uint32_t requiredApprovalBasisPoints,
    std::uint64_t timelockBlocks,
    std::string governanceDigest,
    std::string reason,
    std::string sourceEpochDigest
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_authorizationId(std::move(authorizationId)),
      m_authorizedAmount(authorizedAmount),
      m_activationBlock(activationBlock),
      m_expirationBlock(expirationBlock),
      m_requiredApprovalBasisPoints(requiredApprovalBasisPoints),
      m_timelockBlocks(timelockBlocks),
      m_governanceDigest(std::move(governanceDigest)),
      m_reason(std::move(reason)),
      m_sourceEpochDigest(std::move(sourceEpochDigest)) {}

MintAuthorizationRecord MintAuthorizationRecord::none(
    const InflationEpochSnapshot& epoch
) {
    if (!epoch.active()) {
        throw std::invalid_argument("Cannot build empty mint authorization from inactive inflation epoch.");
    }

    return MintAuthorizationRecord(
        "NONE",
        epoch.blockHeight(),
        "NO_ACTIVE_MINT_AUTHORIZATION",
        utils::Amount(),
        epoch.blockHeight() + NODO_CONTROLLED_ISSUANCE_TIMELOCK_BLOCKS,
        epoch.epochEndBlock(),
        ControlledIssuance::REQUIRED_APPROVAL_BASIS_POINTS,
        NODO_CONTROLLED_ISSUANCE_TIMELOCK_BLOCKS,
        ControlledIssuance::GOVERNANCE_LOCKED_DIGEST,
        ControlledIssuance::NO_AUTHORIZATION_REASON,
        epoch.serialize()
    );
}

const std::string& MintAuthorizationRecord::status() const {
    return m_status;
}

std::uint64_t MintAuthorizationRecord::blockHeight() const {
    return m_blockHeight;
}

const std::string& MintAuthorizationRecord::authorizationId() const {
    return m_authorizationId;
}

utils::Amount MintAuthorizationRecord::authorizedAmount() const {
    return m_authorizedAmount;
}

std::uint64_t MintAuthorizationRecord::activationBlock() const {
    return m_activationBlock;
}

std::uint64_t MintAuthorizationRecord::expirationBlock() const {
    return m_expirationBlock;
}

std::uint32_t MintAuthorizationRecord::requiredApprovalBasisPoints() const {
    return m_requiredApprovalBasisPoints;
}

std::uint64_t MintAuthorizationRecord::timelockBlocks() const {
    return m_timelockBlocks;
}

const std::string& MintAuthorizationRecord::governanceDigest() const {
    return m_governanceDigest;
}

const std::string& MintAuthorizationRecord::reason() const {
    return m_reason;
}

const std::string& MintAuthorizationRecord::sourceEpochDigest() const {
    return m_sourceEpochDigest;
}

bool MintAuthorizationRecord::isValid() const {
    if (m_status == "ACTIVE") {
        return m_blockHeight > 0 &&
               !m_authorizationId.empty() &&
               m_authorizedAmount.isPositive() &&
               m_activationBlock == m_blockHeight &&
               m_expirationBlock == m_blockHeight &&
               m_requiredApprovalBasisPoints == 10000 &&
               m_timelockBlocks == 0 &&
               !m_governanceDigest.empty() &&
               m_reason == ControlledIssuance::EPOCH_REWARD_AUTHORIZATION_REASON &&
               !m_sourceEpochDigest.empty();
    }
    if (m_status != "NONE" ||
        m_blockHeight == 0 ||
        m_authorizationId != "NO_ACTIVE_MINT_AUTHORIZATION" ||
        !m_authorizedAmount.isZero() ||
        m_activationBlock <= m_blockHeight ||
        m_expirationBlock < m_blockHeight ||
        m_requiredApprovalBasisPoints < ControlledIssuance::REQUIRED_APPROVAL_BASIS_POINTS ||
        m_requiredApprovalBasisPoints > 10000 ||
        m_timelockBlocks < NODO_CONTROLLED_ISSUANCE_TIMELOCK_BLOCKS ||
        m_governanceDigest != ControlledIssuance::GOVERNANCE_LOCKED_DIGEST ||
        m_reason != ControlledIssuance::NO_AUTHORIZATION_REASON ||
        m_sourceEpochDigest.empty()) {
        return false;
    }

    return true;
}

std::string MintAuthorizationRecord::serialize() const {
    std::ostringstream oss;

    oss << "MintAuthorizationRecord{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";authorizationId=" << m_authorizationId
        << ";authorizedAmountRawUnits=" << m_authorizedAmount.rawUnits()
        << ";activationBlock=" << m_activationBlock
        << ";expirationBlock=" << m_expirationBlock
        << ";requiredApprovalBasisPoints=" << m_requiredApprovalBasisPoints
        << ";timelockBlocks=" << m_timelockBlocks
        << ";governanceDigest=" << m_governanceDigest
        << ";reason=" << m_reason
        << ";sourceEpochDigest=" << m_sourceEpochDigest
        << "}";

    return oss.str();
}

SupplyExpansionRecord::SupplyExpansionRecord()
    : m_status("NONE"),
      m_blockHeight(0),
      m_mintedAmount(),
      m_recipientAddress(""),
      m_authorizationId(""),
      m_policyId(""),
      m_reason(""),
      m_sourceAuthorizationDigest("") {}

SupplyExpansionRecord::SupplyExpansionRecord(
    std::string status,
    std::uint64_t blockHeight,
    utils::Amount mintedAmount,
    std::string recipientAddress,
    std::string authorizationId,
    std::string policyId,
    std::string reason,
    std::string sourceAuthorizationDigest
)
    : m_status(std::move(status)),
      m_blockHeight(blockHeight),
      m_mintedAmount(mintedAmount),
      m_recipientAddress(std::move(recipientAddress)),
      m_authorizationId(std::move(authorizationId)),
      m_policyId(std::move(policyId)),
      m_reason(std::move(reason)),
      m_sourceAuthorizationDigest(std::move(sourceAuthorizationDigest)) {}

SupplyExpansionRecord SupplyExpansionRecord::none(
    const MintAuthorizationRecord& authorization,
    const InflationEpochSnapshot& epoch
) {
    if (!authorization.isValid() || !epoch.active()) {
        throw std::invalid_argument("Cannot build empty supply expansion from invalid issuance context.");
    }

    return SupplyExpansionRecord(
        "NONE",
        epoch.blockHeight(),
        utils::Amount(),
        ControlledIssuance::NO_RECIPIENT_ADDRESS,
        authorization.authorizationId(),
        epoch.policyId(),
        ControlledIssuance::NO_SUPPLY_EXPANSION_REASON,
        authorization.serialize()
    );
}

const std::string& SupplyExpansionRecord::status() const {
    return m_status;
}

std::uint64_t SupplyExpansionRecord::blockHeight() const {
    return m_blockHeight;
}

utils::Amount SupplyExpansionRecord::mintedAmount() const {
    return m_mintedAmount;
}

const std::string& SupplyExpansionRecord::recipientAddress() const {
    return m_recipientAddress;
}

const std::string& SupplyExpansionRecord::authorizationId() const {
    return m_authorizationId;
}

const std::string& SupplyExpansionRecord::policyId() const {
    return m_policyId;
}

const std::string& SupplyExpansionRecord::reason() const {
    return m_reason;
}

const std::string& SupplyExpansionRecord::sourceAuthorizationDigest() const {
    return m_sourceAuthorizationDigest;
}

bool SupplyExpansionRecord::isValid() const {
    if (m_status == "EXECUTED") {
        return m_blockHeight > 0 &&
               m_mintedAmount.isPositive() &&
               m_recipientAddress == "MULTIPLE_VALIDATORS" &&
               !m_authorizationId.empty() &&
               !m_policyId.empty() &&
               m_reason == ControlledIssuance::EPOCH_REWARD_EXPANSION_REASON &&
               !m_sourceAuthorizationDigest.empty();
    }
    return m_status == "NONE" &&
           m_blockHeight > 0 &&
           m_mintedAmount.isZero() &&
           m_recipientAddress == ControlledIssuance::NO_RECIPIENT_ADDRESS &&
           m_authorizationId == "NO_ACTIVE_MINT_AUTHORIZATION" &&
           !m_policyId.empty() &&
           m_reason == ControlledIssuance::NO_SUPPLY_EXPANSION_REASON &&
           !m_sourceAuthorizationDigest.empty();
}

std::string SupplyExpansionRecord::serialize() const {
    std::ostringstream oss;

    oss << "SupplyExpansionRecord{"
        << "status=" << m_status
        << ";blockHeight=" << m_blockHeight
        << ";mintedAmountRawUnits=" << m_mintedAmount.rawUnits()
        << ";recipientAddress=" << m_recipientAddress
        << ";authorizationId=" << m_authorizationId
        << ";policyId=" << m_policyId
        << ";reason=" << m_reason
        << ";sourceAuthorizationDigest=" << m_sourceAuthorizationDigest
        << "}";

    return oss.str();
}

InflationEpochSnapshot ControlledIssuance::buildInflationEpochSnapshot(
    const config::GenesisConfig& genesisConfig,
    std::uint64_t blockHeight,
    utils::Amount mintedThisEpoch
) {
    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot build controlled issuance epoch at genesis height.");
    }

    if (mintedThisEpoch.isNegative()) {
        throw std::invalid_argument("Cannot build controlled issuance epoch with negative minted amount.");
    }

    const MonetaryPolicy policy =
        MonetaryPolicy::protocolDefault();

    const utils::Amount baseSupply =
        MonetaryFirewall::genesisSupply(
            genesisConfig
        );

    const utils::Amount annualLimit =
        MonetaryFirewall::annualMintLimit(
            baseSupply,
            policy
        );

    if (mintedThisEpoch > annualLimit) {
        throw std::invalid_argument("Controlled issuance rejected epoch: minted amount exceeds annual cap.");
    }

    const std::uint64_t epochStart =
        epochStartForBlock(
            blockHeight
        );

    return InflationEpochSnapshot(
        "ACTIVE",
        blockHeight,
        epochStart,
        epochStart + NODO_CONTROLLED_ISSUANCE_EPOCH_BLOCKS - 1,
        policy.maxAnnualInflationBasisPoints(),
        baseSupply,
        annualLimit,
        mintedThisEpoch,
        annualLimit - mintedThisEpoch,
        policy.deterministicId(),
        INFLATION_EPOCH_REASON
    );
}

MintAuthorizationRecord ControlledIssuance::buildNoMintAuthorization(
    const InflationEpochSnapshot& epoch
) {
    return MintAuthorizationRecord::none(
        epoch
    );
}

SupplyExpansionRecord ControlledIssuance::buildNoSupplyExpansion(
    const MintAuthorizationRecord& authorization,
    const InflationEpochSnapshot& epoch
) {
    return SupplyExpansionRecord::none(
        authorization,
        epoch
    );
}

MintAuthorizationRecord ControlledIssuance::buildEpochRewardAuthorization(
    const InflationEpochSnapshot& epoch,
    utils::Amount authorizedAmount,
    const std::string& authorizationId,
    const std::string& rewardEvidenceDigest
) {
    if (!epoch.active() || !authorizedAmount.isPositive() ||
        authorizedAmount > epoch.mintedThisEpoch() || authorizationId.empty() ||
        rewardEvidenceDigest.empty()) {
        throw std::invalid_argument("Cannot authorize invalid canonical epoch rewards.");
    }
    MintAuthorizationRecord record(
        "ACTIVE", epoch.blockHeight(), authorizationId, authorizedAmount,
        epoch.blockHeight(), epoch.blockHeight(), 10000, 0,
        rewardEvidenceDigest, EPOCH_REWARD_AUTHORIZATION_REASON, epoch.serialize()
    );
    if (!record.isValid()) throw std::logic_error("Invalid epoch reward authorization produced.");
    return record;
}

SupplyExpansionRecord ControlledIssuance::buildEpochRewardExpansion(
    const MintAuthorizationRecord& authorization,
    const InflationEpochSnapshot& epoch
) {
    if (!authorization.isValid() || authorization.status() != "ACTIVE" || !epoch.active()) {
        throw std::invalid_argument("Cannot expand supply from invalid epoch reward authorization.");
    }
    SupplyExpansionRecord record(
        "EXECUTED", epoch.blockHeight(), authorization.authorizedAmount(),
        "MULTIPLE_VALIDATORS", authorization.authorizationId(), epoch.policyId(),
        EPOCH_REWARD_EXPANSION_REASON, authorization.serialize()
    );
    if (!record.isValid()) throw std::logic_error("Invalid epoch reward expansion produced.");
    return record;
}

bool ControlledIssuance::sameEpoch(
    const InflationEpochSnapshot& left,
    const InflationEpochSnapshot& right
) {
    return left.serialize() == right.serialize();
}

bool ControlledIssuance::sameAuthorization(
    const MintAuthorizationRecord& left,
    const MintAuthorizationRecord& right
) {
    return left.serialize() == right.serialize();
}

bool ControlledIssuance::sameExpansion(
    const SupplyExpansionRecord& left,
    const SupplyExpansionRecord& right
) {
    return left.serialize() == right.serialize();
}

} // namespace nodo::node
