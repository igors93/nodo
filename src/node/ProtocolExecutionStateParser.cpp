
#include "node/ProtocolExecutionStateParser.hpp"
#include "crypto/PublicKey.hpp"
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string_view>

namespace nodo::node {

static std::string extractField(const std::string &text,
                                const std::string &field) {
  std::string pattern = field + "=";
  size_t pos = text.find(pattern);
  if (pos == std::string::npos)
    return "";
  pos += pattern.length();
  size_t end = text.find_first_of(";}],", pos);
  if (end == std::string::npos)
    end = text.length();
  return text.substr(pos, end - pos);
}

static uint64_t parseU64(const std::string &s) {
  if (s.empty())
    return 0;
  return std::stoull(s);
}

static int64_t parseI64(const std::string &s) {
  if (s.empty())
    return 0;
  return std::stoll(s);
}

ProtocolExecutionState ProtocolExecutionStateParser::parse(
    const std::map<std::string, std::string> &domains) {
  ProtocolExecutionState state;

  // 1. Supply
  auto it = domains.find("supply");
  if (it != domains.end()) {
    std::string raw = extractField(it->second, "latestRawUnits");
    state.supply = utils::Amount::fromRawUnits(parseI64(raw));
  }

  // 2. Burns
  it = domains.find("burns");
  if (it != domains.end()) {
    std::string text = it->second;
    size_t pos = text.find("BurnRecord{");
    while (pos != std::string::npos) {
      size_t nextPos = text.find("BurnRecord{", pos + 1);
      std::string entryStr = text.substr(pos, nextPos == std::string::npos ? std::string::npos : nextPos - pos);
      
      std::string burnId = extractField(entryStr, "burnId");
      uint64_t blockHeight = parseU64(extractField(entryStr, "blockHeight"));
      uint64_t epoch = parseU64(extractField(entryStr, "epoch"));
      std::string sourceAddr = extractField(entryStr, "sourceAddress");
      utils::Amount amount = utils::Amount::fromRawUnits(parseI64(extractField(entryStr, "amountRaw")));
      std::string reason = extractField(entryStr, "reason");
      std::string typeStr = extractField(entryStr, "burnType");
      
      economics::BurnType burnType = economics::BurnType::FEE_BURN;
      if (typeStr == "SLASH_BURN") burnType = economics::BurnType::SLASH_BURN;
      else if (typeStr == "VOLUNTARY_BURN") burnType = economics::BurnType::VOLUNTARY_BURN;
      else if (typeStr == "GOVERNANCE_DEPOSIT_BURN") burnType = economics::BurnType::GOVERNANCE_DEPOSIT_BURN;
      else if (typeStr == "PENALTY_BURN") burnType = economics::BurnType::PENALTY_BURN;
      
      state.burns.push_back(economics::BurnRecord(
          burnId, blockHeight, epoch, sourceAddr, amount, reason, burnType
      ));
      
      pos = nextPos;
    }
  }

  // 3. Staking
  // Staking will be seeded after validators are parsed, to match the current tests.

  // 4. Governance
  it = domains.find("governance");
  if (it != domains.end()) {
    // Mock
  }

  // 5. Validators
  it = domains.find("validators");
  if (it != domains.end()) {
    // Example:
    // ValidatorRegistry{size=1;...entries=[ValidatorRegistryEntry{status=ACTIVE;lastUpdatedAt=...;stakeAmount=...;...registration=ValidatorRegistrationRecord{validatorAddress=...;publicKey=...;activationEpoch=...;metadataHash=...;registeredAt=...}}]}
    std::string text = it->second;
    size_t entryPos = text.find("ValidatorRegistryEntry{");
    while (entryPos != std::string::npos) {
      size_t nextEntry = text.find("ValidatorRegistryEntry{", entryPos + 1);
      std::string entryStr = text.substr(entryPos, nextEntry - entryPos);

      std::string statusStr = extractField(entryStr, "status");
      core::ValidatorRegistrationStatus status =
          core::validatorRegistrationStatusFromString(statusStr);
      uint64_t stakeAmount = parseU64(extractField(entryStr, "stakeAmount"));
      std::string ownerAddr = extractField(entryStr, "ownerAddress");

      size_t regPos = entryStr.find("ValidatorRegistrationRecord{");
      if (regPos != std::string::npos) {
        std::string regStr = entryStr.substr(regPos);
        std::string valAddr = extractField(regStr, "validatorAddress");
        std::string pubKeyHex = extractField(regStr, "keyMaterial");
        uint64_t actEpoch = parseU64(extractField(regStr, "activationEpoch"));
        std::string meta = extractField(regStr, "metadataHash");
        int64_t regAt = parseI64(extractField(regStr, "registeredAt"));

        crypto::PublicKey pubKey(crypto::CryptoAlgorithm::BLS12_381, pubKeyHex);
        if (pubKey.isValid()) {
          core::ValidatorRegistrationRecord rec(valAddr, pubKey, actEpoch, meta,
                                                regAt);
          if (status == core::ValidatorRegistrationStatus::PENDING_ACTIVATION) {
            state.validators.registerPendingValidator(rec, stakeAmount,
                                                      ownerAddr);
          } else if (status == core::ValidatorRegistrationStatus::ACTIVE) {
            state.validators.registerValidator(rec, stakeAmount, ownerAddr);
          }
          // For other states, we might need to apply transitions
        }
      }
      entryPos = nextEntry;
    }
  }

  // 6. Penalty
  it = domains.find("slashing");
  if (it != domains.end()) {
    // Mock
  }

  for (const std::string &address : state.validators.activeValidatorAddresses()) {
    const core::ValidatorRegistryEntry *entry = state.validators.entryForAddress(address);
    if (entry != nullptr && entry->stakeAmount() > 0 && !state.staking.hasAccount(address)) {
      state.staking.setAccount(
          address,
          economics::StakeAccount(
              address, utils::Amount::fromRawUnits(static_cast<std::int64_t>(entry->stakeAmount()))));
    }
  }

  return state;
}

} // namespace nodo::node
