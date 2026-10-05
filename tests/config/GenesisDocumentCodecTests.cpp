#include "config/GenesisDocumentCodec.hpp"

#include "config/GenesisRegistry.hpp"

#include "../common/TestnetCandidateGenesisFixture.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

namespace {

using nodo::config::GenesisConfig;
using nodo::config::GenesisDocumentCodec;

std::string replaceOnce(std::string text, const std::string &from,
                        const std::string &to) {
  const std::size_t position = text.find(from);
  assert(position != std::string::npos);
  text.replace(position, from.size(), to);
  return text;
}

std::string lineValue(const std::string &text, const std::string &key) {
  const std::string prefix = "\n" + key + "=";
  const std::size_t start = text.find(prefix);
  assert(start != std::string::npos);
  const std::size_t valueStart = start + prefix.size();
  return text.substr(valueStart, text.find('\n', valueStart) - valueStart);
}

std::string replaceValue(const std::string &text, const std::string &key,
                         const std::string &value) {
  return replaceOnce(text, "\n" + key + "=" + lineValue(text, key) + "\n",
                     "\n" + key + "=" + value + "\n");
}

std::string removeLine(const std::string &text, const std::string &key) {
  return replaceOnce(text, "\n" + key + "=" + lineValue(text, key) + "\n",
                     "\n");
}

void requireRejected(const std::string &contents,
                     const std::string &expectedFragment) {
  try {
    (void)GenesisDocumentCodec::decode(contents);
  } catch (const std::invalid_argument &error) {
    assert(std::string(error.what()).find(expectedFragment) !=
           std::string::npos);
    return;
  }
  assert(false && "decode should have rejected the document");
}

std::string validDocument() {
  return GenesisDocumentCodec::encode(
      nodo::tests::testnetCandidateTestGenesis());
}

void testRoundTripPreservesGenesisIdentity() {
  const GenesisConfig original = nodo::tests::testnetCandidateTestGenesis();
  const std::string encoded = GenesisDocumentCodec::encode(original);
  const GenesisConfig decoded = GenesisDocumentCodec::decode(encoded);

  assert(decoded.deterministicId() == original.deterministicId());
  assert(GenesisDocumentCodec::encode(decoded) == encoded);
  assert(encoded.rfind(GenesisDocumentCodec::VERSION, 0) == 0);
}

// Network parameters come from the code profile, never from the document.
void testDocumentCarriesNoNetworkParameters() {
  const std::string encoded = validDocument();
  assert(encoded.find("quorum") == std::string::npos);
  assert(encoded.find("minimumFee") == std::string::npos);
  assert(encoded.find("network=testnet-candidate\n") != std::string::npos);
}

void testBuiltInGenesisRoundTrips() {
  const GenesisConfig localnet =
      nodo::config::GenesisRegistry::get("localnet").genesis();
  const GenesisConfig decoded =
      GenesisDocumentCodec::decode(GenesisDocumentCodec::encode(localnet));
  assert(decoded.deterministicId() == localnet.deterministicId());
}

void testWindowsLineEndingsAccepted() {
  std::string crlf;
  for (const char character : validDocument()) {
    if (character == '\n') {
      crlf += "\r\n";
    } else {
      crlf += character;
    }
  }
  assert(GenesisDocumentCodec::decode(crlf).deterministicId() ==
         nodo::tests::testnetCandidateTestGenesis().deterministicId());
}

void testRejectsWrongVersion() {
  requireRejected(replaceOnce(validDocument(), GenesisDocumentCodec::VERSION,
                              "NODO_GENESIS_DOCUMENT_V0"),
                  "Unsupported key-value file version");
}

void testRejectsUnknownField() {
  requireRejected(validDocument() + "quorumThresholdNumerator=1\n",
                  "Unknown key-value field");
}

void testRejectsMissingField() {
  requireRejected(removeLine(validDocument(), "validator.2.publicKey"),
                  "validator.2.publicKey");
}

void testRejectsChainIdMismatch() {
  requireRejected(replaceValue(validDocument(), "chainId", "nodo-forged-1"),
                  "chain id");
}

void testRejectsProtocolVersionMismatch() {
  requireRejected(
      replaceValue(validDocument(), "protocolVersion", "nodo/9.9"),
      "protocol version");
}

void testRejectsMainnetAndUnknownNetworks() {
  requireRejected(replaceValue(validDocument(), "network", "mainnet"),
                  "unsupported network");
  requireRejected(replaceValue(validDocument(), "network", "fantasy"),
                  "unsupported network");
}

void testRejectsNonCanonicalPublicKey() {
  const std::string document = validDocument();
  std::string upper = lineValue(document, "validator.0.publicKey");
  for (char &character : upper) {
    if (character >= 'a' && character <= 'f') {
      character = static_cast<char>(character - 'a' + 'A');
    }
  }
  requireRejected(replaceValue(document, "validator.0.publicKey", upper),
                  "lowercase hex");
  requireRejected(replaceValue(document, "validator.0.publicKey", "abcd"),
                  "48-byte");
}

void testRejectsNonBlsValidatorAlgorithm() {
  requireRejected(replaceValue(validDocument(), "validator.0.algorithm",
                               "CLASSIC_ED25519"),
                  "BLS12_381");
}

void testRejectsBadAddressChecksum() {
  const std::string document = validDocument();
  std::string owner = lineValue(document, "validator.1.ownerAddress");
  owner.back() = owner.back() == '0' ? '1' : '0';
  requireRejected(replaceValue(document, "validator.1.ownerAddress", owner),
                  "not a valid Nodo address");
}

void testRejectsDuplicateValidator() {
  const std::string document = validDocument();
  requireRejected(replaceValue(document, "validator.3.publicKey",
                               lineValue(document, "validator.0.publicKey")),
                  "duplicates an earlier validator");
}

void testRejectsTooFewValidators() {
  std::string document = validDocument();
  for (const char *name : {"algorithm", "publicKey", "activationEpoch",
                           "bootstrapWeight", "metadataHash",
                           "ownerAddress"}) {
    document = removeLine(document, std::string("validator.3.") + name);
  }
  document = replaceValue(document, "validatorCount", "3");
  requireRejected(document, "at least 4 are required");
}

void testRejectsMalformedNumbers() {
  requireRejected(
      replaceValue(validDocument(), "account.0.balanceRaw", "-5"),
      "not an unsigned integer");
  requireRejected(replaceValue(validDocument(), "genesisTimestamp", "01"),
                  "leading zeros");
  requireRejected(replaceValue(validDocument(), "validatorCount", "1001"),
                  "exceeds its maximum");
}

void testRejectsZeroTimestamp() {
  requireRejected(replaceValue(validDocument(), "genesisTimestamp", "0"),
                  "does not describe a valid genesis");
}

} // namespace

int main() {
  testRoundTripPreservesGenesisIdentity();
  testDocumentCarriesNoNetworkParameters();
  testBuiltInGenesisRoundTrips();
  testWindowsLineEndingsAccepted();
  testRejectsWrongVersion();
  testRejectsUnknownField();
  testRejectsMissingField();
  testRejectsChainIdMismatch();
  testRejectsProtocolVersionMismatch();
  testRejectsMainnetAndUnknownNetworks();
  testRejectsNonCanonicalPublicKey();
  testRejectsNonBlsValidatorAlgorithm();
  testRejectsBadAddressChecksum();
  testRejectsDuplicateValidator();
  testRejectsTooFewValidators();
  testRejectsMalformedNumbers();
  testRejectsZeroTimestamp();
  return 0;
}
