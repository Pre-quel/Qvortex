/**
 * Qvortex Hash - Simplified for SMHasher
 * 
 * This version prioritizes distribution quality and speed over
 * cryptographic properties, specifically targeting SMHasher tests.
 */

#include "qvortex.h"
#include <string.h>
#include <stdlib.h>
#if defined(__aarch64__) || defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif
/* Platform detection */
// #if defined(__ARM_NEON) || defined(__ARM_NEON__)
// #include <arm_neon.h>



/* Prime constants from xxHash */
#define PRIME64_1 0x9E3779B185EBCA87ULL
#define PRIME64_2 0xC2B2AE3D27D4EB4FULL
#define PRIME64_3 0x165667B19E3779F9ULL
#define PRIME64_4 0x85EBCA77C2B2AE63ULL
#define PRIME64_5 0x27D4EB2F165667C5ULL

/* Rotation */
static inline uint64_t rotl64(uint64_t x, int r) {
    return (x << r) | (x >> (64 - r));
}

static inline uint64_t chaotic_round(uint64_t acc, uint64_t input) {
    // Logistic map in integer arithmetic
    uint64_t x = acc ^ input;
    
    // x = r * x * (1 - x), scaled to integers
    // Using r = 3.9 for chaotic behavior
    uint64_t one_minus_x = ~x;
    uint64_t chaos = (x >> 32) * (one_minus_x >> 32);
    
    // Mix with input and rotate
    acc = chaos + input * PRIME64_2;
    acc = rotl64(acc, 31);
    acc *= PRIME64_1;
    
    return acc;
}


/* MurmurHash3 finalizer - best known mixer */
static inline uint64_t murmur3_mix(uint64_t h) {
    h ^= h >> 33;
    h *= 0xff51afd7ed598ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

/* Simplified state and context */
struct qvortex_ctx {
    uint64_t v1, v2, v3, v4;  /* Core accumulators */
    uint64_t total_len;
    uint64_t mem64[4];        /* Buffer for incomplete blocks */
    uint32_t memsize;
};

/* Initialize with seed */
void qvortex_init(qvortex_ctx *ctx, const uint8_t *key, size_t key_len) {
    uint64_t seed = 0;
    
    /* Simple key derivation */
    if (key && key_len > 0) {
        for (size_t i = 0; i < key_len; i++) {
            seed = rotl64(seed, 5) ^ key[i];
        }
        seed = murmur3_mix(seed);
    }
    
    ctx->v1 = seed + PRIME64_1 + PRIME64_2;
    ctx->v2 = seed + PRIME64_2;
    ctx->v3 = seed + 0;
    ctx->v4 = seed - PRIME64_1;
    ctx->total_len = 0;
    ctx->memsize = 0;
}

/* Process 32-byte blocks */
static void qvortex_process_block(qvortex_ctx *ctx, const uint8_t *p) {
    const uint64_t *p64 = (const uint64_t *)p;
    
#if defined(__aarch64__)
    /* NEON acceleration for ARM64 - using scalar operations for 64-bit multiply */
    uint64x2_t v12 = {ctx->v1, ctx->v2};
    uint64x2_t v34 = {ctx->v3, ctx->v4};
    uint64x2_t prime2 = vdupq_n_u64(PRIME64_2);
    
    uint64x2_t input12 = vld1q_u64(p64);
    uint64x2_t input34 = vld1q_u64(p64 + 2);
    
    /* Since vmlaq_u64 doesn't exist, we need to do the multiply-add manually */
    /* v = v + (input * prime2) */
    
    /* Extract lanes, do scalar multiply, then reconstruct */
    uint64_t v1_temp = vgetq_lane_u64(v12, 0) + vgetq_lane_u64(input12, 0) * PRIME64_2;
    uint64_t v2_temp = vgetq_lane_u64(v12, 1) + vgetq_lane_u64(input12, 1) * PRIME64_2;
    
    /* Rotate */
    v1_temp = rotl64(v1_temp, 31);
    v2_temp = rotl64(v2_temp, 31);
    
    /* Multiply by PRIME64_1 */
    v1_temp *= PRIME64_1;
    v2_temp *= PRIME64_1;
    
    /* Same for v3/v4 */
    uint64_t v3_temp = vgetq_lane_u64(v34, 0) + vgetq_lane_u64(input34, 0) * PRIME64_2;
    uint64_t v4_temp = vgetq_lane_u64(v34, 1) + vgetq_lane_u64(input34, 1) * PRIME64_2;
    
    v3_temp = rotl64(v3_temp, 31);
    v4_temp = rotl64(v4_temp, 31);
    
    v3_temp *= PRIME64_1;
    v4_temp *= PRIME64_1;
    
    /* Store results */
    ctx->v1 = v1_temp;
    ctx->v2 = v2_temp;
    ctx->v3 = v3_temp;
    ctx->v4 = v4_temp;
#else
    /* Scalar path */
    ctx->v1 = chaotic_round(ctx->v1, p64[0]);
    ctx->v2 = chaotic_round(ctx->v2, p64[1]);
    ctx->v3 = chaotic_round(ctx->v3, p64[2]);
    ctx->v4 = chaotic_round(ctx->v4, p64[3]);
#endif
}

/* Update hash with data */
void qvortex_update(qvortex_ctx *ctx, const uint8_t *input, size_t len) {
    ctx->total_len += len;
    
    /* Fill buffer if needed */
    if (ctx->memsize + len < 32) {
        memcpy((uint8_t *)ctx->mem64 + ctx->memsize, input, len);
        ctx->memsize += (uint32_t)len;
        return;
    }
    
    const uint8_t *p = input;
    const uint8_t *const pEnd = input + len;
    
    /* Complete current block */
    if (ctx->memsize) {
        memcpy((uint8_t *)ctx->mem64 + ctx->memsize, input, 32 - ctx->memsize);
        qvortex_process_block(ctx, (const uint8_t *)ctx->mem64);
        p += 32 - ctx->memsize;
        ctx->memsize = 0;
    }
    
    /* Process full blocks */
    if (p + 32 <= pEnd) {
        const uint8_t *const limit = pEnd - 32;
        do {
            qvortex_process_block(ctx, p);
            p += 32;
        } while (p <= limit);
    }
    
    /* Store remainder */
    if (p < pEnd) {
        memcpy(ctx->mem64, p, (size_t)(pEnd - p));
        ctx->memsize = (uint32_t)(pEnd - p);
    }
}

/* Finalize hash - this is critical for distribution */
void qvortex_final(qvortex_ctx *ctx, uint8_t *dst, size_t dst_len) {
    uint64_t h64;
    
    /* Merge accumulators */
    if (ctx->total_len >= 32) {
        uint64_t v1 = ctx->v1;
        uint64_t v2 = ctx->v2;
        uint64_t v3 = ctx->v3;
        uint64_t v4 = ctx->v4;
        
        h64 = rotl64(v1, 1) + rotl64(v2, 7) + rotl64(v3, 12) + rotl64(v4, 18);
        
        /* Avalanche mixing */
        v1 *= PRIME64_2; v1 = rotl64(v1, 31); v1 *= PRIME64_1;
        h64 ^= v1;
        h64 = h64 * PRIME64_1 + PRIME64_4;
        
        v2 *= PRIME64_2; v2 = rotl64(v2, 31); v2 *= PRIME64_1;
        h64 ^= v2;
        h64 = h64 * PRIME64_1 + PRIME64_4;
        
        v3 *= PRIME64_2; v3 = rotl64(v3, 31); v3 *= PRIME64_1;
        h64 ^= v3;
        h64 = h64 * PRIME64_1 + PRIME64_4;
        
        v4 *= PRIME64_2; v4 = rotl64(v4, 31); v4 *= PRIME64_1;
        h64 ^= v4;
        h64 = h64 * PRIME64_1 + PRIME64_4;
    } else {
        h64 = ctx->v3 + PRIME64_5;
    }
    
    h64 += ctx->total_len;
    
    /* Process remaining bytes */
    const uint8_t *p = (const uint8_t *)ctx->mem64;
    const uint8_t *const pEnd = p + ctx->memsize;
    
    /* Process 8-byte chunks */
    while (p + 8 <= pEnd) {
        uint64_t k1 = *(uint64_t *)p;
        k1 *= PRIME64_2;
        k1 = rotl64(k1, 31);
        k1 *= PRIME64_1;
        h64 ^= k1;
        h64 = rotl64(h64, 27) * PRIME64_1 + PRIME64_4;
        p += 8;
    }
    
    /* Process 4-byte chunk */
    if (p + 4 <= pEnd) {
        h64 ^= (uint64_t)(*(uint32_t *)p) * PRIME64_1;
        h64 = rotl64(h64, 23) * PRIME64_2 + PRIME64_3;
        p += 4;
    }
    
    /* Process remaining bytes */
    while (p < pEnd) {
        h64 ^= (*p++) * PRIME64_5;
        h64 = rotl64(h64, 11) * PRIME64_1;
    }
    
    /* Final avalanche - critical for good distribution */
    h64 ^= h64 >> 33;
    h64 *= PRIME64_2;
    h64 ^= h64 >> 29;
    h64 *= PRIME64_3;
    h64 ^= h64 >> 32;
    
    /* Generate output of requested length */
    size_t generated = 0;
    uint64_t h = h64;
    
    while (generated < dst_len) {
        size_t to_copy = (dst_len - generated > 8) ? 8 : (dst_len - generated);
        memcpy(dst + generated, &h, to_copy);
        generated += to_copy;
        
        /* Generate more output if needed */
        if (generated < dst_len) {
            h = murmur3_mix(h + PRIME64_5);
        }
    }
}

/* All-in-one hash function */
void qvortex_hash(const uint8_t *key, size_t key_len,
                  const uint8_t *data, size_t data_len,
                  uint8_t *out, size_t out_len) {
    qvortex_ctx ctx;
    qvortex_init(&ctx, key, key_len);
    qvortex_update(&ctx, data, data_len);
    qvortex_final(&ctx, out, out_len);
}

/* Optimized small hash for SMHasher */
void qvortex_hash_small(const uint8_t *key, size_t key_len,
                       const uint8_t *data, size_t data_len,
                       uint8_t *out, size_t out_len) {
    /* For very small inputs, use direct path */
    if (data_len <= 16) {
        uint64_t seed = 0;
        
        /* Key processing */
        if (key && key_len > 0) {
            for (size_t i = 0; i < key_len; i++) {
                seed = rotl64(seed, 5) ^ key[i];
            }
            seed = murmur3_mix(seed);
        }
        
        /* Direct hash for small data */
        uint64_t h = seed + PRIME64_5 + data_len;
        
        /* Mix in all bytes */
        for (size_t i = 0; i < data_len; i++) {
            h ^= data[i] * PRIME64_5;
            h = rotl64(h, 11) * PRIME64_1;
        }
        
        /* Final mix */
        h = murmur3_mix(h);
        
        /* Output */
        size_t generated = 0;
        while (generated < out_len) {
            size_t to_copy = (out_len - generated > 8) ? 8 : (out_len - generated);
            memcpy(out + generated, &h, to_copy);
            generated += to_copy;
            h = murmur3_mix(h + 1);
        }
    } else {
        qvortex_hash(key, key_len, data, data_len, out, out_len);
    }
}