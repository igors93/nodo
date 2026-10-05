#include "config/GenesisDocumentCodec.hpp"

#include "config/NetworkProfileRegistry.hpp"
#include "crypto/Address.hpp"
#include "crypto/CryptoAlgorithm.hpp"
#include "crypto/Hex.hpp"
#include "crypto/PublicKey.hpp"
#include "serialization/KeyValueFileCodec.hpp"
#include "storage/AtomicFile.hpp"
#include "utils/Amount.hpp"

#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nodo::config {

namespace {

// Upper bounds keep a hostile document from driving unbounded allocation
// before any entry has been validated.
constexpr std::uint64_t kMaxValidators = 1000;
constexpr std::uint64_t kMaxAccounts = 100000;
constexpr std::size_t kBlsPublicKeyBytes = 48;

std::string validatorField(std::uint64_t index, const std::string &name) {
  return "validator." + std::to_string(index) + "." + name;
}

std::string accountField(std::uint64_t index, const std::string &name) {
  return "account." + std::to_string(index) + "." + name;
}

std::uint64_t parseUnsigned(const std::string &field, const std::string &text,
                            std::uint64_t maximum) {
  if (text.empty() || text.size() > 20) {
    throw std::invalid_argument("Genesis document field '" + field +
                                "' is not an unsigned integer.");
  }

  std::uint64_t value = 0;
  for (const char character : text) {
    if (character < '0' || character > '9') {
      throw std::invalid_argument("Genesis document field '" + field +
                                  "' is not an unsigned integer.");
    }

    const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
    if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
      throw std::invalid_argument("Genesis document field '" + field +
                                  "' is out of range.");
    }
    value = value * 10 + digit;
  }

  if (text.size() > 1 && text.front() == '0') {
    throw std::invalid_argument("Genesis document field '" + field +
                                "' has leading zeros.");
  }

  if (value > maximum) {
    throw std::invalid_argument("Genesis document field '" + field +
                                "' exceeds its maximum of " +
                                std::to_string(maximum) + ".");
  }

  return value;
}

std::int64_t parseNonNegativeSigned(const std::string &field,
                                    const std::string &text) {
  return static_cast<std::int64_t>(parseUnsigned(
      field, text,
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())));
}

bool isLowercaseHex(const std::string &value) {
  for (const char character : value) {
    const bool digit = character >= '0' && character <= '9';
    const bool lower = character >= 'a' && character <= 'f';
    if (!digit && !lower) {
      return false;
    }
  }
  return true;
}

std::string requireAddress(const serialization::KeyValueFileDocument &document,
                           const std::string &field) {
  const std::string value = document.requireField(field);
  if (!crypto::Address(value).isValid()) {
    throw std::invalid_argument("Genesis document field '" + field +
                                "' is not a valid Nodo address: " + value);
  }
  return value;
}

} // namespace

std::string GenesisDocumentCodec::encode(const GenesisConfig &genesis) {
  const NetworkParameters &params = genesis.networkParameters();

  std::vector<std::pair<std::string, std::string>> fields{
      {"network", params.networkName()},
      {"chainId", params.chainId()},
      {"protocolVersion", params.protocolVersion()},
      {"genesisTimestamp", std::to_string(genesis.genesisTimestamp())},
      {"genesisMemo", genesis.genesisMemo()},
      {"validatorCount",
       std::to_string(genesis.bootstrapValidators().size())}};

  for (std::size_t index = 0; index < genesis.bootstrapValidators().size();
       ++index) {
    const BootstrapValidatorConfig &validator =
        genesis.bootstrapValidators()[index];

    fields.emplace_back(validatorField(index, "algorithm"),
                        crypto::cryptoAlgorithmToString(
                            validator.validatorPublicKey().algorithm()));
    fields.emplace_back(validatorField(index, "publicKey"),
                        validator.validatorPublicKey().keyMaterial());
    fields.emplace_back(validatorField(index, "activationEpoch"),
                        std::to_string(validator.activationEpoch()));
    fields.emplace_back(validatorField(index, "bootstrapWeight"),
                        std::to_string(validator.bootstrapWeight()));
    fields.emplace_back(validatorField(index, "metadataHash"),
                        validator.metadataHash());

    if (!validator.ownerAddress().empty()) {
      fields.emplace_back(validatorField(index, "ownerAddress"),
                          validator.ownerAddress());
    }
  }

  fields.emplace_back("accountCount",
                      std::to_string(genesis.genesisAccounts().size()));

  for (std::size_t index = 0; index < genesis.genesisAccounts().size();
       ++index) {
    const GenesisAccountConfig &account = genesis.genesisAccounts()[index];

    fields.emplace_back(accountField(index, "address"), account.address());
    fields.emplace_back(accountField(index, "balanceRaw"),
                        std::to_string(account.balance().rawUnits()));
    fields.emplace_back(accountField(index, "nonce"),
                        std::to_string(account.nonce()));
  }

  return serialization::KeyValueFileCodec::serialize(VERSION, fields);
}

GenesisConfig GenesisDocumentCodec::decode(const std::string &contents) {
  const serialization::KeyValueFileDocument document =
      serialization::KeyValueFileCodec::parse(contents, VERSION);

  const std::string network = document.requireField("network");
  if (!NetworkProfileRegistry::isKnown(network) ||
      NetworkProfileRegistry::isMainnetLocked(network)) {
    throw std::invalid_argument("Genesis document declares unsupported network '" +
                                network + "'.");
  }

  const NetworkParameters params = NetworkProfileRegistry::get(network);

  if (document.requireField("chainId") != params.chainId()) {
    throw std::invalid_argument(
        "Genesis document chain id '" + document.requireField("chainId") +
        "' does not match network '" + network + "' chain id '" +
        params.chainId() + "'.");
  }

  if (document.requireField("protocolVersion") != params.protocolVersion()) {
    throw std::invalid_argument(
        "Genesis document protocol version '" +
        document.requireField("protocolVersion") +
        "' does not match network '" + network + "' protocol version '" +
        params.protocolVersion() + "'.");
  }

  const std::uint64_t validatorCount = parseUnsigned(
      "validatorCount", document.requireField("validatorCount"),
      kMaxValidators);
  const std::uint64_t accountCount = parseUnsigned(
      "accountCount", document.requireField("accountCount"), kMaxAccounts);

  std::set<std::string> allowedFields{
      "network",       "chainId",        "protocolVersion", "genesisTimestamp",
      "genesisMemo",   "validatorCount", "accountCount"};

  for (std::uint64_t index = 0; index < validatorCount; ++index) {
    for (const char *name : {"algorithm", "publicKey", "activationEpoch",
                             "bootstrapWeight", "metadataHash",
                             "ownerAddress"}) {
      allowedFields.insert(validatorField(index, name));
    }
  }

  for (std::uint64_t index = 0; index < accountCount; ++index) {
    for (const char *name : {"address", "balanceRaw", "nonce"}) {
      allowedFields.insert(accountField(index, name));
    }
  }

  document.requireOnlyFields(allowedFields);

  const std::int64_t genesisTimestamp = parseNonNegativeSigned(
      "genesisTimestamp", document.requireField("genesisTimestamp"));

  if (validatorCount < params.minimumValidatorCount()) {
    throw std::invalid_argument(
        "Genesis document for network '" + network + "' lists " +
        std::to_string(validatorCount) + " bootstrap validators; at least " +
        std::to_string(params.minimumValidatorCount()) + " are required.");
  }

  std::vector<BootstrapValidatorConfig> validators;
  validators.reserve(static_cast<std::size_t>(validatorCount));
  std::set<std::string> seenValidatorAddresses;

  for (std::uint64_t index = 0; index < validatorCount; ++index) {
    const std::string algorithmField = validatorField(index, "algorithm");
    const std::string algorithm = document.requireField(algorithmField);
    if (algorithm !=
        crypto::cryptoAlgorithmToString(crypto::CryptoAlgorithm::BLS12_381)) {
      throw std::invalid_argument("Genesis document field '" + algorithmField +
                                  "' must be BLS12_381 for a consensus "
                                  "validator key.");
    }

    const std::string publicKeyField = validatorField(index, "publicKey");
    const std::string publicKeyHex = document.requireField(publicKeyField);
    if (!crypto::hasHexByteSize(publicKeyHex, kBlsPublicKeyBytes) ||
        !isLowercaseHex(publicKeyHex)) {
      throw std::invalid_argument(
          "Genesis document field '" + publicKeyField +
          "' must be a 48-byte BLS12-381 public key in lowercase hex.");
    }

    const std::uint64_t activationEpoch = parseUnsigned(
        validatorField(index, "activationEpoch"),
        document.requireField(validatorField(index, "activationEpoch")),
        std::numeric_limits<std::uint64_t>::max());

    const std::uint64_t bootstrapWeight = parseUnsigned(
        validatorField(index, "bootstrapWeight"),
        document.requireField(validatorField(index, "bootstrapWeight")),
        std::numeric_limits<std::uint32_t>::max());

    const std::string ownerAddressField = validatorField(index, "ownerAddress");
    const std::string ownerAddress =
        document.hasField(ownerAddressField)
            ? requireAddress(document, ownerAddressField)
            : "";

    BootstrapValidatorConfig validator(
        crypto::PublicKey(crypto::CryptoAlgorithm::BLS12_381, publicKeyHex),
        activationEpoch, static_cast<std::uint32_t>(bootstrapWeight),
        document.requireField(validatorField(index, "metadataHash")),
        ownerAddress);

    if (!validator.isValid()) {
      throw std::invalid_argument("Genesis document validator " +
                                  std::to_string(index) + " is invalid.");
    }

    if (!seenValidatorAddresses.insert(validator.validatorAddress()).second) {
      throw std::invalid_argument("Genesis document validator " +
                                  std::to_string(index) +
                                  " duplicates an earlier validator key.");
    }

    validators.push_back(std::move(validator));
  }

  std::vector<GenesisAccountConfig> accounts;
  accounts.reserve(static_cast<std::size_t>(accountCount));
  std::set<std::string> seenAccountAddresses;

  for (std::uint64_t index = 0; index < accountCount; ++index) {
    const std::string address =
        requireAddress(document, accountField(index, "address"));

    if (!seenAccountAddresses.insert(address).second) {
      throw std::invalid_argument("Genesis document account " +
                                  std::to_string(index) +
                                  " duplicates an earlier account address.");
    }

    const std::int64_t balanceRaw = parseNonNegativeSigned(
        accountField(index, "balanceRaw"),
        document.requireField(accountField(index, "balanceRaw")));

    const std::uint64_t nonce = parseUnsigned(
        accountField(index, "nonce"),
        document.requireField(accountField(index, "nonce")),
        std::numeric_limits<std::uint64_t>::max());

    accounts.emplace_back(address, utils::Amount::fromRawUnits(balanceRaw),
                          nonce);
  }

  GenesisConfig genesis(params, genesisTimestamp, std::move(validators),
                        std::move(accounts),
                        document.requireField("genesisMemo"));

  if (!genesis.isValid()) {
    throw std::invalid_argument("Genesis document for network '" + network +
                                "' does not describe a valid genesis.");
  }

  return genesis;
}

GenesisConfig
GenesisDocumentCodec::loadFile(const std::filesystem::path &path) {
  return decode(storage::AtomicFile::readTextFile(path));
}

} // namespace nodo::config
