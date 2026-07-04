#include "p2p/EncryptedPeerChannel.hpp"

#include <cassert>
#include <string>

using namespace nodo::p2p;

int main() {
  EncryptedPeerSession sender("node-a", "node-b", "local-testnet-shared-secret",
                              100);

  EncryptedPeerSession receiver("node-b", "node-a",
                                "local-testnet-shared-secret", 100);

  assert(sender.isValid());
  assert(receiver.isValid());
  assert(sender.sessionId() == receiver.sessionId());

  NetworkEnvelope envelope("nodo-localnet", "nodo-localnet-chain", "nodo/1",
                           NetworkMessageType::PING, "node-a", 120, 30,
                           "hello-secure-peer");

  EncryptedPeerChannelFrame frame = sender.sealEnvelope(envelope, 121);

  assert(frame.isValid());
  assert(frame.ciphertextHex().find("hello-secure-peer") == std::string::npos);

  EncryptedPeerOpenResult opened = receiver.openFrame(frame);
  assert(opened.opened());
  assert(opened.envelope().has_value());
  assert(opened.envelope()->payload() == "hello-secure-peer");
  assert(opened.envelope()->senderNodeId() == "node-a");

  EncryptedPeerOpenResult replay = receiver.openFrame(frame);
  assert(!replay.opened());
  assert(replay.status() == EncryptedPeerChannelStatus::REPLAY_DETECTED);

  EncryptedPeerSession freshReceiver("node-b", "node-a",
                                     "local-testnet-shared-secret", 100);

  std::string tamperedCiphertext = frame.ciphertextHex();
  tamperedCiphertext[tamperedCiphertext.size() - 1] =
      tamperedCiphertext.back() == '0' ? '1' : '0';

  EncryptedPeerChannelFrame tampered(
      frame.sessionId(), frame.fromNodeId(), frame.toNodeId(), frame.sequence(),
      frame.createdAt(), frame.nonceHex(), tamperedCiphertext,
      frame.authenticationTagHex());

  EncryptedPeerOpenResult tamperedResult = freshReceiver.openFrame(tampered);
  assert(!tamperedResult.opened());
  assert(tamperedResult.status() ==
         EncryptedPeerChannelStatus::AUTHENTICATION_FAILED);

  const std::string encoded =
      EncryptedPeerChannelCodec::encodeFrameToString(frame);
  EncryptedPeerChannelFrame decoded =
      EncryptedPeerChannelCodec::decodeFrameFromString(encoded);
  assert(decoded.isValid());
  assert(decoded.sessionId() == frame.sessionId());
  assert(decoded.ciphertextHex() == frame.ciphertextHex());

  return 0;
}
