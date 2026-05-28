#ifndef NODO_UTILS_TIME_HPP
#define NODO_UTILS_TIME_HPP

#include <cstdint>

namespace nodo::utils {

/*
 * Retorna timestamp Unix em segundos.
 *
 * DECISÃO:
 * Manter tempo centralizado evita espalhar chamadas de relógio
 * pelo projeto inteiro.
 */
std::int64_t currentUnixTimestamp();

} // namespace nodo::utils

#endif