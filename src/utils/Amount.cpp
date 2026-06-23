#include "utils/Amount.hpp"

#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace nodo::utils {

Amount::Amount() : m_rawUnits(0) {}

Amount::Amount(std::int64_t rawUnits) : m_rawUnits(rawUnits) {
    if (rawUnits < 0) {
        throw std::invalid_argument("Amount cannot be created from negative raw units.");
    }
}

Amount Amount::fromNodo(std::int64_t wholeNodo) {
    if (wholeNodo < 0) {
        throw std::invalid_argument("Amount cannot be created from negative NODO value.");
    }

    if (wholeNodo > INT64_MAX / UNITS_PER_NODO) {
        throw std::overflow_error("Amount::fromNodo: value would overflow int64.");
    }

    return Amount(wholeNodo * UNITS_PER_NODO);
}

Amount Amount::fromRawUnits(std::int64_t rawUnits) {
    if (rawUnits < 0) {
        throw std::invalid_argument("Amount cannot be negative.");
    }

    return Amount(rawUnits);
}

std::int64_t Amount::rawUnits() const {
    return m_rawUnits;
}

bool Amount::isNegative() const {
    return m_rawUnits < 0;
}

bool Amount::isZero() const {
    return m_rawUnits == 0;
}

bool Amount::isPositive() const {
    return m_rawUnits > 0;
}

std::string Amount::toString() const {
    const std::int64_t whole = m_rawUnits / UNITS_PER_NODO;
    const std::int64_t fraction = m_rawUnits % UNITS_PER_NODO;

    std::ostringstream oss;
    oss << whole << "."
        << std::setw(8)
        << std::setfill('0')
        << fraction
        << " NODO";

    return oss.str();
}

Amount Amount::operator+(const Amount& other) const {
    if (other.m_rawUnits > 0 && m_rawUnits > INT64_MAX - other.m_rawUnits) {
        throw std::overflow_error("Amount addition overflow.");
    }

    return Amount(m_rawUnits + other.m_rawUnits);
}

Amount Amount::operator-(const Amount& other) const {
    if (other.m_rawUnits > m_rawUnits) {
        throw std::underflow_error("Amount subtraction would create negative value.");
    }

    return Amount(m_rawUnits - other.m_rawUnits);
}

bool Amount::operator==(const Amount& other) const {
    return m_rawUnits == other.m_rawUnits;
}

bool Amount::operator!=(const Amount& other) const {
    return !(*this == other);
}

bool Amount::operator<(const Amount& other) const {
    return m_rawUnits < other.m_rawUnits;
}

bool Amount::operator>(const Amount& other) const {
    return m_rawUnits > other.m_rawUnits;
}

bool Amount::operator<=(const Amount& other) const {
    return m_rawUnits <= other.m_rawUnits;
}

bool Amount::operator>=(const Amount& other) const {
    return m_rawUnits >= other.m_rawUnits;
}

} // namespace nodo::utils