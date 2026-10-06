#ifndef NODO_UTILS_AMOUNT_HPP
#define NODO_UTILS_AMOUNT_HPP

#include <cstdint>
#include <string>

namespace nodo::utils {

/* NODO monetary values use integer raw units to avoid floating-point rounding. One NODO is 100,000,000 raw units. */
class Amount {
public:
    static constexpr std::int64_t UNITS_PER_NODO = 100000000;

    Amount();
    explicit Amount(std::int64_t rawUnits);

    static Amount fromNodo(std::int64_t wholeNodo);
    static Amount fromRawUnits(std::int64_t rawUnits);

    std::int64_t rawUnits() const;

    bool isNegative() const;
    bool isZero() const;
    bool isPositive() const;

    std::string toString() const;

    Amount operator+(const Amount& other) const;
    Amount operator-(const Amount& other) const;

    bool operator==(const Amount& other) const;
    bool operator!=(const Amount& other) const;
    bool operator<(const Amount& other) const;
    bool operator>(const Amount& other) const;
    bool operator<=(const Amount& other) const;
    bool operator>=(const Amount& other) const;

private:
    std::int64_t m_rawUnits;
};

} // namespace nodo::utils

#endif