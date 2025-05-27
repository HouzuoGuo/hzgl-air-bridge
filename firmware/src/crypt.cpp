#include <stdint.h>
#include <string.h>
#include "crypt.h"
#include "uECC.h"

int crypt_is_valid_pubkey(uint8_t *pub_key_compressed)
{
    uint8_t with_sign_byte[29];
    uint8_t pub_key_uncompressed[128];
    const struct uECC_Curve_t *curve = uECC_secp224r1();
    with_sign_byte[0] = 0x02;
    memcpy(&with_sign_byte[1], pub_key_compressed, 28);
    uECC_decompress(with_sign_byte, pub_key_uncompressed, curve);
    if (!uECC_valid_public_key(pub_key_uncompressed, curve))
    {
        return 0;
    }
    return 1;
}

void crypt_copy_4b_big_endian(uint8_t *dst, const uint32_t *src)
{
    uint32_t value = *src;
    dst[0] = (value >> 24) & 0xFF;
    dst[1] = (value >> 16) & 0xFF;
    dst[2] = (value >> 8) & 0xFF;
    dst[3] = value & 0xFF;
}

void crypt_copy_8b_big_endian(uint8_t *dst, const uint64_t *src)
{
    uint64_t value = *src;
    dst[0] = (value >> 56) & 0xFF;
    dst[1] = (value >> 48) & 0xFF;
    dst[2] = (value >> 40) & 0xFF;
    dst[3] = (value >> 32) & 0xFF;
    dst[4] = (value >> 24) & 0xFF;
    dst[5] = (value >> 16) & 0xFF;
    dst[6] = (value >> 8) & 0xFF;
    dst[7] = value & 0xFF;
}

void crypt_pub_from_priv(uint8_t *pub_compressed, uint8_t *priv)
{
    const struct uECC_Curve_t *curve = uECC_secp224r1();
    uint8_t pub_key_tmp[128];
    uECC_compute_public_key(priv, pub_key_tmp, curve);
    uECC_compress(pub_key_tmp, pub_compressed, curve);
}
