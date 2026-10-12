/* Internal portable SHA-256 for source-byte identity, not authentication. */
#ifndef TL_SHA256_H
#define TL_SHA256_H
#include <stddef.h>
#include <stdint.h>
void TLSha256(const unsigned char *bytes, size_t size, unsigned char out[32]);
#endif
