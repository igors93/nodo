#ifndef NODO_UTILS_AMOUNT_HPP
#define NODO_UTILS_AMOUNT_HPP

#include <cstdint>
#include <string>

namespace nodo::utils {

/*
 * Amount representa valores monetários da moeda NODO.
 *
 * DECISÃO DE SEGURANÇA:
 * Nunca usamos double/float para moeda.
 * Valores monetários usam inteiros para evitar erros de arredondamento.
 *
 * 1 NODO = 100.000.000 units
 * parecido com o conceito de satoshis no Bitcoin.
 */
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