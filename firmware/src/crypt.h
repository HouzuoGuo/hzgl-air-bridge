#pragma once
#include <stdint.h>

int crypt_is_valid_pubkey(uint8_t *pub_key_compressed);
void crypt_copy_4b_big_endian(uint8_t *dst, const uint32_t *src);
void crypt_copy_8b_big_endian(uint8_t *dst, const uint64_t *src);
void crypt_pub_from_priv(uint8_t *pub_compressed, uint8_t *priv);