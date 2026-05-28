#ifndef NODO_CRYPTO_HASH_H
#define NODO_CRYPTO_HASH_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ATENÇÃO:
 * Esta versão é apenas educativa.
 * Não é um hash criptográfico seguro.
 *
 * A arquitetura isola o hash aqui para permitir trocar depois
 * por SHA-256 real sem alterar o core da blockchain.
 */
void nodo_hash_string(const char* input, char* output, unsigned int output_size);

#ifdef __cplusplus
}
#endif

#endif