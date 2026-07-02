#include "node/MonetaryFirewall.hpp"

#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nodo::node {

namespace {

utils::Amount calculateSupplyAfter(
    utils::Amount supplyBefore,
    utils::Amount minted,
    utils::Amount burned
) {
    return supplyBefore + minted - burned;
}

std::int64_t basisPointAmount(
    std::int64_t rawUnits,
    std::uint32_t basisPoints
) {
    if (rawUnits <= 0 || basisPoints == 0) {
        return 0;
    }

    const std::int64_t whole =
        rawUnits / 10000;

    const std::int64_t remainder =
        rawUnits % 10000;

    if (whole > std::numeric_limits<std::int64_t>::max() / static_cast<std::int64_t>(basisPoints)) {
        return std::numeric_limits<std::int64_t>::max();
    }

    const std::int64_t wholePart =
        whole * static_cast<std::int64_t>(basisPoints);

    const std::int64_t bpSigned = static_cast<std::int64_t>(basisPoints);
    const std::int64_t remainderProduct =
        (bpSigned > 0 && remainder > std::numeric_limits<std::int64_t>::max() / bpSigned)
            ? std::numeric_limits<std::int64_t>::max()
            : remainder * bpSigned;
    const std::int64_t remainderPart = remainderProduct / 10000;

    if (wholePart > std::numeric_limits<std::int64_t>::max() - remainderPart) {
        return std::numeric_limits<std::int64_t>::max();
    }

    return wholePart + remainderPart;
}

} // namespace

MonetaryPolicy::MonetaryPolicy()
    : m_maxAnnualInflationBasisPoints(0),
      m_ruleId(""),
      m_reason("") {}

MonetaryPolicy::MonetaryPolicy(
    std::uint32_t maxAnnualInflationBasisPoints,
    std::string ruleId,
    std::string reason
)
    : m_maxAnnualInflationBasisPoints(maxAnnualInflationBasisPoints),
      m_ruleId(std::move(ruleId)),
      m_reason(std::move(reason)) {}

std::uint32_t MonetaryPolicy::maxAnnualInflationBasisPoints() const {
    return m_maxAnnualInflationBasisPoints;
}

const std::string& MonetaryPolicy::ruleId() const {
    return m_ruleId;
}

const std::string& MonetaryPolicy::reason() const {
    return m_reason;
}

bool MonetaryPolicy::isValid() const {
    return !m_ruleId.empty() &&
           !m_reason.empty() &&
           m_maxAnnualInflationBasisPoints <= NODO_MAX_ANNUAL_INFLATION_BASIS_POINTS;
}

std::string MonetaryPolicy::deterministicId() const {
    return serialize();
}

std::string MonetaryPolicy::serialize() const {
    std::ostringstream oss;

    oss << "MonetaryPolicy{"
        << "maxAnnualInflationBasisPoints=" << m_maxAnnualInflationBasisPoints
        << ";ruleId=" << m_ruleId
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

MonetaryPolicy MonetaryPolicy::protocolDefault() {
    return MonetaryPolicy(
        NODO_MAX_ANNUAL_INFLATION_BASIS_POINTS,
        "NODO_MONETARY_POLICY_V1",
        "MAX_4_PERCENT_ANNUAL_INFLATION"
    );
}

SupplyLedgerSnapshot::SupplyLedgerSnapshot()
    : m_blockHeight(0),
      m_supplyBefore(),
      m_minted(),
      m_burned(),
      m_treasuryDelta(),
      m_supplyAfter() {}

SupplyLedgerSnapshot::SupplyLedgerSnapshot(
    std::uint64_t blockHeight,
    utils::Amount supplyBefore,
    utils::Amount minted,
    utils::Amount burned,
    utils::Amount treasuryDelta,
    utils::Amount supplyAfter
)
    : m_blockHeight(blockHeight),
      m_supplyBefore(supplyBefore),
      m_minted(minted),
      m_burned(burned),
      m_treasuryDelta(treasuryDelta),
      m_supplyAfter(supplyAfter) {}

std::uint64_t SupplyLedgerSnapshot::blockHeight() const {
    return m_blockHeight;
}

utils::Amount SupplyLedgerSnapshot::supplyBefore() const {
    return m_supplyBefore;
}

utils::Amount SupplyLedgerSnapshot::minted() const {
    return m_minted;
}

utils::Amount SupplyLedgerSnapshot::burned() const {
    return m_burned;
}

utils::Amount SupplyLedgerSnapshot::treasuryDelta() const {
    return m_treasuryDelta;
}

utils::Amount SupplyLedgerSnapshot::supplyAfter() const {
    return m_supplyAfter;
}

bool SupplyLedgerSnapshot::isValid() const {
    if (m_blockHeight == 0 ||
        m_supplyBefore.isNegative() ||
        m_minted.isNegative() ||
        m_burned.isNegative() ||
        m_supplyAfter.isNegative()) {
        return false;
    }

    return calculateSupplyAfter(
        m_supplyBefore,
        m_minted,
        m_burned
    ) == m_supplyAfter;
}

std::string SupplyLedgerSnapshot::serialize() const {
    std::ostringstream oss;

    oss << "SupplyLedgerSnapshot{"
        << "blockHeight=" << m_blockHeight
        << ";supplyBeforeRawUnits=" << m_supplyBefore.rawUnits()
        << ";mintedRawUnits=" << m_minted.rawUnits()
        << ";burnedRawUnits=" << m_burned.rawUnits()
        << ";treasuryDeltaRawUnits=" << m_treasuryDelta.rawUnits()
        << ";supplyAfterRawUnits=" << m_supplyAfter.rawUnits()
        << "}";

    return oss.str();
}

MonetaryFirewallAudit::MonetaryFirewallAudit()
    : m_status("NOT_EVALUATED"),
      m_supplyLedger(),
      m_annualMintLimit(),
      m_annualMintUsedBefore(),
      m_annualMintUsedAfter(),
      m_policyId(""),
      m_reason(MonetaryFirewall::NOT_EVALUATED_REASON) {}

MonetaryFirewallAudit::MonetaryFirewallAudit(
    std::string status,
    SupplyLedgerSnapshot supplyLedger,
    utils::Amount annualMintLimit,
    utils::Amount annualMintUsedBefore,
    utils::Amount annualMintUsedAfter,
    std::string policyId,
    std::string reason
)
    : m_status(std::move(status)),
      m_supplyLedger(std::move(supplyLedger)),
      m_annualMintLimit(annualMintLimit),
      m_annualMintUsedBefore(annualMintUsedBefore),
      m_annualMintUsedAfter(annualMintUsedAfter),
      m_policyId(std::move(policyId)),
      m_reason(std::move(reason)) {}

MonetaryFirewallAudit MonetaryFirewallAudit::notEvaluated() {
    return MonetaryFirewallAudit();
}

const std::string& MonetaryFirewallAudit::status() const {
    return m_status;
}

const SupplyLedgerSnapshot& MonetaryFirewallAudit::supplyLedger() const {
    return m_supplyLedger;
}

utils::Amount MonetaryFirewallAudit::annualMintLimit() const {
    return m_annualMintLimit;
}

utils::Amount MonetaryFirewallAudit::annualMintUsedBefore() const {
    return m_annualMintUsedBefore;
}

utils::Amount MonetaryFirewallAudit::annualMintUsedAfter() const {
    return m_annualMintUsedAfter;
}

const std::string& MonetaryFirewallAudit::policyId() const {
    return m_policyId;
}

const std::string& MonetaryFirewallAudit::reason() const {
    return m_reason;
}

bool MonetaryFirewallAudit::passed() const {
    return m_status == "PASS" && isValid();
}

bool MonetaryFirewallAudit::isValid() const {
    if (m_status == "NOT_EVALUATED") {
        return m_reason == MonetaryFirewall::NOT_EVALUATED_REASON;
    }

    if (m_status != "PASS" ||
        !m_supplyLedger.isValid() ||
        m_annualMintLimit.isNegative() ||
        m_annualMintUsedBefore.isNegative() ||
        m_annualMintUsedAfter.isNegative() ||
        m_policyId != MonetaryPolicy::protocolDefault().deterministicId() ||
        (m_reason != MonetaryFirewall::ZERO_MINT_REASON &&
         m_reason != MonetaryFirewall::EPOCH_REWARD_MINT_REASON)) {
        return false;
    }

    if (m_annualMintUsedAfter != m_annualMintUsedBefore + m_supplyLedger.minted()) {
        return false;
    }

    return m_annualMintUsedAfter <= m_annualMintLimit;
}

std::string MonetaryFirewallAudit::serialize() const {
    std::ostringstream oss;

    oss << "MonetaryFirewallAudit{"
        << "status=" << m_status
        << ";supplyLedger=" << m_supplyLedger.serialize()
        << ";annualMintLimitRawUnits=" << m_annualMintLimit.rawUnits()
        << ";annualMintUsedBeforeRawUnits=" << m_annualMintUsedBefore.rawUnits()
        << ";annualMintUsedAfterRawUnits=" << m_annualMintUsedAfter.rawUnits()
        << ";policyId=" << m_policyId
        << ";reason=" << m_reason
        << "}";

    return oss.str();
}

utils::Amount MonetaryFirewall::genesisSupply(
    const config::GenesisConfig& genesisConfig
) {
    if (!genesisConfig.isValid()) {
        throw std::invalid_argument("Cannot calculate supply from invalid genesis config.");
    }

    utils::Amount total;

    for (const config::GenesisAccountConfig& account : genesisConfig.genesisAccounts()) {
        if (!account.isValid()) {
            throw std::invalid_argument("Cannot calculate supply from invalid genesis account.");
        }

        total = total + account.balance();
    }

    if (total.isNegative()) {
        throw std::invalid_argument("Genesis supply cannot be negative.");
    }

    return total;
}

utils::Amount MonetaryFirewall::annualMintLimit(
    utils::Amount baseSupply,
    const MonetaryPolicy& policy
) {
    if (baseSupply.isNegative() || !policy.isValid()) {
        throw std::invalid_argument("Cannot calculate annual mint limit from invalid monetary inputs.");
    }

    return utils::Amount::fromRawUnits(
        basisPointAmount(
            baseSupply.rawUnits(),
            policy.maxAnnualInflationBasisPoints()
        )
    );
}

MonetaryFirewallAudit MonetaryFirewall::buildZeroMintAudit(
    const config::GenesisConfig& genesisConfig,
    std::uint64_t blockHeight
) {
    return buildAudit(
        genesisConfig,
        blockHeight,
        utils::Amount(),
        utils::Amount(),
        utils::Amount(),
        utils::Amount()
    );
}

MonetaryFirewallAudit MonetaryFirewall::buildAudit(
    const config::GenesisConfig& genesisConfig,
    std::uint64_t blockHeight,
    utils::Amount minted,
    utils::Amount burned,
    utils::Amount treasuryDelta,
    utils::Amount annualMintUsedBefore
) {
    return buildAuditWithSupplyBefore(
        blockHeight,
        genesisSupply(genesisConfig),
        minted,
        burned,
        treasuryDelta,
        annualMintUsedBefore
    );
}

MonetaryFirewallAudit MonetaryFirewall::buildAuditWithSupplyBefore(
    std::uint64_t blockHeight,
    utils::Amount supplyBefore,
    utils::Amount minted,
    utils::Amount burned,
    utils::Amount treasuryDelta,
    utils::Amount annualMintUsedBefore
) {
    if (blockHeight == 0) {
        throw std::invalid_argument("Cannot build monetary firewall audit at genesis height.");
    }

    if (supplyBefore.isNegative() ||
        minted.isNegative() ||
        burned.isNegative() ||
        annualMintUsedBefore.isNegative()) {
        throw std::invalid_argument("Cannot build monetary firewall audit with negative monetary values.");
    }

    if (minted.isPositive()) {
        throw std::invalid_argument("Monetary firewall rejected block: minting requires an explicit monetary authorization record.");
    }

    const MonetaryPolicy policy =
        MonetaryPolicy::protocolDefault();

    const utils::Amount supplyAfter =
        calculateSupplyAfter(
            supplyBefore,
            minted,
            burned
        );

    if (supplyAfter.isNegative()) {
        throw std::invalid_argument("Monetary firewall rejected block: burn exceeds available supply.");
    }

    const utils::Amount annualLimit =
        annualMintLimit(
            supplyBefore,
            policy
        );

    const utils::Amount annualMintUsedAfter =
        annualMintUsedBefore + minted;

    if (annualMintUsedAfter > annualLimit) {
        throw std::invalid_argument("Monetary firewall rejected block: annual mint limit exceeded.");
    }

    return MonetaryFirewallAudit(
        "PASS",
        SupplyLedgerSnapshot(
            blockHeight,
            supplyBefore,
            minted,
            burned,
            treasuryDelta,
            supplyAfter
        ),
        annualLimit,
        annualMintUsedBefore,
        annualMintUsedAfter,
        policy.deterministicId(),
        ZERO_MINT_REASON
    );
}

MonetaryFirewallAudit MonetaryFirewall::buildEpochRewardAuditWithSupplyBefore(
    std::uint64_t blockHeight,
    utils::Amount supplyBefore,
    utils::Amount minted,
    utils::Amount burned,
    utils::Amount treasuryDelta,
    utils::Amount annualMintUsedBefore
) {
    if (blockHeight == 0 || supplyBefore.isNegative() || !minted.isPositive() ||
        burned.isNegative() || annualMintUsedBefore.isNegative()) {
        throw std::invalid_argument("Invalid canonical epoch reward audit inputs.");
    }
    const MonetaryPolicy policy = MonetaryPolicy::protocolDefault();
    const utils::Amount annualLimit = annualMintLimit(supplyBefore, policy);
    const utils::Amount annualMintUsedAfter = annualMintUsedBefore + minted;
    if (annualMintUsedAfter > annualLimit) {
        throw std::invalid_argument("Canonical epoch rewards exceed the annual mint limit.");
    }
    const utils::Amount supplyAfter = calculateSupplyAfter(supplyBefore, minted, burned);
    if (supplyAfter.isNegative()) {
        throw std::invalid_argument("Canonical epoch reward audit has supply underflow.");
    }
    return MonetaryFirewallAudit(
        "PASS",
        SupplyLedgerSnapshot(
            blockHeight, supplyBefore, minted, burned, treasuryDelta, supplyAfter
        ),
        annualLimit,
        annualMintUsedBefore,
        annualMintUsedAfter,
        policy.deterministicId(),
        EPOCH_REWARD_MINT_REASON
    );
}

bool MonetaryFirewall::sameAudit(
    const MonetaryFirewallAudit& left,
    const MonetaryFirewallAudit& right
) {
    return left.serialize() == right.serialize();
}

} // namespace nodo::node
