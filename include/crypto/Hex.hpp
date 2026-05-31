#ifndef NODO_CRYPTO_HEX_HPP
#define NODO_CRYPTO_HEX_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace nodo::crypto {

std::string hexEncode(
    const unsigned char* data,
    std::size_t size
);

std::string hexEncode(
    const std::vector<unsigned char>& data
);

std::vector<unsigned char> hexDecode(
    const std::string& hex
);

bool isHexString(
    const std::string& hex
);

bool hasHexByteSize(
    const std::string& hex,
    std::size_t byteSize
);

} // namespace nodo::crypto

#endif
