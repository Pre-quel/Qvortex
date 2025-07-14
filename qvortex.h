/**
 * Qvortex Hash Library - Header File
 */

#ifndef QVORTEX_H
#define QVORTEX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration constants */
#define QVORTEX_BLOCK_BYTES 32
#define QVORTEX_MAX_HASH_BYTES 64

/* Main context structure - simplified for speed */
typedef struct {
    uint64_t v1, v2, v3, v4;               /* Core accumulators */
    uint64_t total_len;                    /* Total input length */
    uint64_t coarse1, coarse2;    /* ADD: Coarse-grained state */
    uint32_t byte_counter;        /* ADD: Track bytes for coarse update */
    uint64_t mem64[4];                     /* 32-byte buffer */
    uint32_t memsize;                      /* Bytes in buffer */
} qvortex_ctx;

/* Main API functions */
void qvortex_init(qvortex_ctx *ctx, const uint8_t *key, size_t key_len);
void qvortex_update(qvortex_ctx *ctx, const uint8_t *data, size_t len);
void qvortex_final(qvortex_ctx *ctx, uint8_t *out, size_t out_len);

/* All-in-one hash function */
void qvortex_hash(const uint8_t *key, size_t key_len,
                  const uint8_t *data, size_t data_len,
                  uint8_t *out, size_t out_len);

/* Optimized for small inputs */
void qvortex_hash_small(const uint8_t *key, size_t key_len,
                       const uint8_t *data, size_t data_len,
                       uint8_t *out, size_t out_len);

/* Test suite compatibility */
#define QVORTEX_256_BYTES 32
#define QVORTEX_512_BYTES 64

static inline void qvortex256(const uint8_t *data, size_t data_len, uint8_t *out) {
    qvortex_hash(NULL, 0, data, data_len, out, QVORTEX_256_BYTES);
}

static inline void qvortex512(const uint8_t *data, size_t data_len, uint8_t *out) {
    qvortex_hash(NULL, 0, data, data_len, out, QVORTEX_512_BYTES);
}

/* SMHasher test compatibility wrapper */
typedef struct {
    void (*hash)(const void *key, int len, uint32_t seed, void *out);
    int hash_size;
    const char *name;
} HashInfo;

/* SMHasher-compatible hash function */
static inline void qvortex_smhasher(const void *key, int len, uint32_t seed, void *out) {
    uint8_t seed_bytes[4];
    seed_bytes[0] = seed & 0xFF;
    seed_bytes[1] = (seed >> 8) & 0xFF;
    seed_bytes[2] = (seed >> 16) & 0xFF;
    seed_bytes[3] = (seed >> 24) & 0xFF;
    
    qvortex_hash_small(seed_bytes, 4, (const uint8_t *)key, len, (uint8_t *)out, 32);
}

#ifdef __cplusplus
}
#endif

#endif /* QVORTEX_H */