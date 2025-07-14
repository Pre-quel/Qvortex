/**
 * Qvortex Hash Sanity Check Test
 * 
 * Tests basic functionality, avalanche effect, and performance
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <mach/mach_time.h>
#include "qvortex.h"

/* Hex dump utility */
void hex_dump(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
    }
    printf("\n");
}

/* Test vectors */
void test_vectors() {
    printf("=== Test Vectors ===\n");
    
    /* Test 1: Empty input */
    {
        uint8_t hash[32];
        qvortex256((const uint8_t *)"", 0, hash);
        printf("Empty string: ");
        hex_dump("Hash", hash, 32);
    }
    
    /* Test 2: Single byte */
    {
        uint8_t hash[32];
        qvortex256((const uint8_t *)"a", 1, hash);
        printf("Single 'a': ");
        hex_dump("Hash", hash, 32);
    }
    
    /* Test 3: Known string */
    {
        uint8_t hash[32];
        const char *msg = "The quick brown fox jumps over the lazy dog";
        qvortex256((const uint8_t *)msg, strlen(msg), hash);
        printf("Fox string: ");
        hex_dump("Hash", hash, 32);
    }
    
    /* Test 4: With key */
    {
        uint8_t hash[32];
        const char *key = "secret";
        const char *msg = "message";
        qvortex_hash((const uint8_t *)key, strlen(key),
                    (const uint8_t *)msg, strlen(msg),
                    hash, 32);
        printf("Keyed hash: ");
        hex_dump("Hash", hash, 32);
    }
    
    printf("\n");
}

/* Avalanche effect test */
void avalanche_test() {
    printf("=== Avalanche Effect Test ===\n");
    
    uint8_t data1[64] = {0};
    uint8_t data2[64] = {0};
    uint8_t hash1[32];
    uint8_t hash2[32];
    
    /* Fill with test pattern */
    for (int i = 0; i < 64; i++) {
        data1[i] = i;
        data2[i] = i;
    }
    
    /* Test changing single bits */
    for (int test = 0; test < 5; test++) {
        /* Flip one bit */
        int byte_pos = test * 13 % 64;
        int bit_pos = test % 8;
        data2[byte_pos] ^= (1 << bit_pos);
        
        qvortex256(data1, 64, hash1);
        qvortex256(data2, 64, hash2);
        
        /* Count bit differences */
        int diff_bits = 0;
        for (int i = 0; i < 32; i++) {
            uint8_t diff = hash1[i] ^ hash2[i];
            for (int b = 0; b < 8; b++) {
                if (diff & (1 << b)) diff_bits++;
            }
        }
        
        printf("Test %d: Changed bit %d of byte %d -> %d/256 bits differ (%.1f%%)\n",
               test, bit_pos, byte_pos, diff_bits, (diff_bits * 100.0) / 256.0);
        
        /* Restore bit */
        data2[byte_pos] ^= (1 << bit_pos);
    }
    
    printf("\n");
}

/* Incremental hashing test */
void incremental_test() {
    printf("=== Incremental Hashing Test ===\n");
    
    const char *message = "This is a test message for incremental hashing.";
    size_t msg_len = strlen(message);
    uint8_t hash_oneshot[32];
    uint8_t hash_incremental[32];
    
    /* One-shot hash */
    qvortex256((const uint8_t *)message, msg_len, hash_oneshot);
    
    /* Incremental hash */
    qvortex_ctx ctx;
    qvortex_init(&ctx, NULL, 0);
    
    /* Feed data in chunks */
    size_t pos = 0;
    size_t chunks[] = {5, 10, 7, 15, 100}; /* Various chunk sizes */
    
    for (int i = 0; i < 5 && pos < msg_len; i++) {
        size_t chunk = chunks[i];
        if (pos + chunk > msg_len) {
            chunk = msg_len - pos;
        }
        qvortex_update(&ctx, (const uint8_t *)message + pos, chunk);
        pos += chunk;
        printf("  Fed %zu bytes (total: %zu/%zu)\n", chunk, pos, msg_len);
    }
    
    qvortex_final(&ctx, hash_incremental, 32);
    
    /* Compare */
    if (memcmp(hash_oneshot, hash_incremental, 32) == 0) {
        printf("✓ Incremental hash matches one-shot hash\n");
    } else {
        printf("✗ ERROR: Incremental hash differs from one-shot hash!\n");
        hex_dump("One-shot", hash_oneshot, 32);
        hex_dump("Incremental", hash_incremental, 32);
    }
    
    printf("\n");
}

/* Performance benchmark */
void performance_test() {
    printf("=== Performance Benchmark ===\n");
    
    const size_t sizes[] = {64, 256, 1024, 4096, 65536, 1048576};
    const char *labels[] = {"64B", "256B", "1KB", "4KB", "64KB", "1MB"};
    
    for (int s = 0; s < 6; s++) {
        size_t size = sizes[s];
        uint8_t *data = malloc(size);
        uint8_t hash[32];
        
        /* Fill with pseudo-random data */
        for (size_t i = 0; i < size; i++) {
            data[i] = (uint8_t)(i * 7 + i/256);
        }
        
        /* Warm up */
        for (int i = 0; i < 100; i++) {
            qvortex256(data, size, hash);
        }
        
        /* Benchmark */
        const int iterations = (s < 4) ? 100000 : 10000;
        
        mach_timebase_info_data_t timebase;
        mach_timebase_info(&timebase);
        
        uint64_t start = mach_absolute_time();
        
        for (int i = 0; i < iterations; i++) {
            qvortex256(data, size, hash);
        }
        
        uint64_t end = mach_absolute_time();
        uint64_t elapsed_ns = (end - start) * timebase.numer / timebase.denom;
        double elapsed_sec = elapsed_ns / 1e9;
        
        double bytes_per_sec = (size * iterations) / elapsed_sec;
        double mb_per_sec = bytes_per_sec / (1024 * 1024);
        
        printf("  %6s: %7d iters in %.3fs = %.1f MB/s\n",
               labels[s], iterations, elapsed_sec, mb_per_sec);
        
        free(data);
    }
    
    printf("\n");
}

/* Distribution test */
void distribution_test() {
    printf("=== Distribution Test ===\n");
    
    const int num_hashes = 10000;
    uint32_t buckets[256] = {0};
    
    for (int i = 0; i < num_hashes; i++) {
        uint8_t data[8];
        uint8_t hash[32];
        
        /* Create input from counter */
        memcpy(data, &i, sizeof(i));
        
        qvortex256(data, 8, hash);
        
        /* Count byte distribution of first 4 bytes */
        for (int j = 0; j < 4; j++) {
            buckets[hash[j]]++;
        }
    }
    
    /* Calculate statistics */
    double expected = (num_hashes * 4) / 256.0;
    double chi_square = 0;
    int min_count = buckets[0];
    int max_count = buckets[0];
    
    for (int i = 0; i < 256; i++) {
        double diff = buckets[i] - expected;
        chi_square += (diff * diff) / expected;
        if (buckets[i] < min_count) min_count = buckets[i];
        if (buckets[i] > max_count) max_count = buckets[i];
    }
    
    printf("  Expected count per bucket: %.1f\n", expected);
    printf("  Min count: %d (%.1f%% of expected)\n", 
           min_count, (min_count * 100.0) / expected);
    printf("  Max count: %d (%.1f%% of expected)\n", 
           max_count, (max_count * 100.0) / expected);
    printf("  Chi-square: %.2f (lower is better, ~255 is expected)\n", chi_square);
    
    printf("\n");
}

/* Check NEON support */
void check_platform() {
    printf("=== Platform Info ===\n");
    
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    printf("✓ NEON support: ENABLED\n");
    printf("  Running optimized ARM NEON code path\n");
#else
    printf("✗ NEON support: DISABLED\n");
    printf("  Running scalar code path\n");
#endif
    
#ifdef __clang__
    printf("✓ Compiler: Clang %d.%d.%d\n", 
           __clang_major__, __clang_minor__, __clang_patchlevel__);
#endif
    
#ifdef __aarch64__
    printf("✓ Architecture: ARM64 (aarch64)\n");
#endif
    
    printf("\n");
}

int main() {
    printf("Qvortex Hash Function - Sanity Check\n");
    printf("====================================\n\n");
    
    check_platform();
    test_vectors();
    avalanche_test();
    incremental_test();
    distribution_test();
    performance_test();
    
    printf("=== Summary ===\n");
    printf("✓ All basic tests completed\n");
    printf("✓ Ready for SMHasher testing\n");
    
    return 0;
}
