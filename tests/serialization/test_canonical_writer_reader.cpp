#include "serialization/CanonicalHash.hpp"
#include "serialization/CanonicalReader.hpp"
#include "serialization/CanonicalWriter.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

int main() {
    nodo::serialization::CanonicalWriter writer;
    writer.writeUInt8(7);
    writer.writeBool(true);
    writer.writeUInt32(0x01020304U);
    writer.writeUInt64(0x0102030405060708ULL);
    writer.writeInt64(-42);
    writer.writeString("nodo");
    writer.writeBytes({0x00, 0x01, 0xff});

    assert(!writer.empty());
    assert(writer.size() > 0);
    assert(!writer.hex().empty());

    nodo::serialization::CanonicalReader reader(writer.bytes());
    assert(reader.readUInt8() == 7);
    assert(reader.readBool());
    assert(reader.readUInt32() == 0x01020304U);
    assert(reader.readUInt64() == 0x0102030405060708ULL);
    assert(reader.readInt64() == -42);
    assert(reader.readString() == "nodo");

    const std::vector<unsigned char> bytes = reader.readBytes();
    assert(bytes.size() == 3);
    assert(bytes[0] == 0x00);
    assert(bytes[1] == 0x01);
    assert(bytes[2] == 0xff);
    reader.requireFullyConsumed();

    const std::string firstHash =
        nodo::serialization::CanonicalHash::hashBytes(
            writer.bytes(),
            "TEST_DOMAIN"
        );
    const std::string secondHash =
        nodo::serialization::CanonicalHash::hashBytes(
            writer.bytes(),
            "TEST_DOMAIN"
        );
    assert(firstHash == secondHash);
    assert(firstHash.size() == 64);

    bool rejectedTrailing = false;
    std::vector<unsigned char> withTrailing = writer.bytes();
    withTrailing.push_back(0x00);

    try {
        nodo::serialization::CanonicalReader trailing(withTrailing);
        trailing.readUInt8();
        trailing.requireFullyConsumed();
    } catch (const std::runtime_error&) {
        rejectedTrailing = true;
    }

    assert(rejectedTrailing);

    bool rejectedTruncated = false;

    try {
        nodo::serialization::CanonicalReader truncated(
            std::vector<unsigned char>{0x00, 0x00, 0x00}
        );
        truncated.readUInt32();
    } catch (const std::runtime_error&) {
        rejectedTruncated = true;
    }

    assert(rejectedTruncated);

    return 0;
}
