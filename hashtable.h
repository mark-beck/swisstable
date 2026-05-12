#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <immintrin.h>
#include "dynstring.h"

#define GROUP_SIZE 8

typedef struct _Group {
    uint8_t meta[GROUP_SIZE];
    String keys[GROUP_SIZE];
    String values[GROUP_SIZE];

    struct _Group *overflow;
} Group;

typedef struct {
    uint32_t max_slots;
    uint32_t size;

    Group *slots;
} HashTable;

HashTable init_hashtable(uint32_t max_slots) {
    Group *slots = calloc(max_slots, sizeof(Group));

    HashTable table = {
        .max_slots = max_slots,
        .size = 0,
        .slots = slots
    };
    return table;
}

void free_group(Group *group) {
    for (int i = 0; i < GROUP_SIZE; i++) {
        if (group->keys[i].content != NULL)
        free_string(group->keys[i]);
        free_string(group->values[i]);
    }
    if (group->overflow != NULL) {
        free_group(group->overflow);
        free(group->overflow);
    }
}

void free_hashtable(HashTable *table) {
    for (uint32_t i = 0; i < table->max_slots; i++) {
        free_group(&table->slots[i]);
    }
    free(table->slots);
}

static inline uint32_t _hash(const char *str)
{
    uint32_t h = 2166136261u;
    while (*str) {
        h ^= (uint8_t)*str++;
        h *= 16777619u;
    }
    return h;
}

void group_insert(Group *group, String key, String value, uint8_t h1, uint32_t n) {
    // 64 bit masking version
    // uint64_t chunk;
    // memcpy(&chunk, group->meta, 8);

    // uint64_t zero_byte_mask = (chunk - 0x0101010101010101ULL) & ~chunk & 0x8080808080808080ULL;

    // if (zero_byte_mask) {
    //     int first = __builtin_ctzll(zero_byte_mask) >> 3;
    //     group->meta[first] = h1;
    //     group->keys[first] = key;
    //     group->values[first] = value;
    //     return;
    // }


    // simd masking version
    __m128i mask = _mm_set1_epi8(0);
    __m128i data = _mm_loadl_epi64((const __m128i*)group->meta);
    uint16_t matches = _mm_movemask_epi8(_mm_cmpeq_epi8(data, mask));
    matches &= 0xFF;
    if (matches) {
        int first = __builtin_ctz(matches);
        group->meta[first] = h1;
        group->keys[first] = key;
        group->values[first] = value;
        return;
    }

    if (group->overflow == NULL) {
        group->overflow = calloc(1, sizeof(Group));
    }
    group_insert(group->overflow, key, value, h1, n);
}

void hashtable_insert(HashTable* table, String key, String value) {
    uint32_t hash = _hash(key.content);
    uint32_t slot = hash % table->max_slots;
    uint8_t h1 = (hash >> 25) | 0b10000000;
    group_insert(&table->slots[slot], key, value, h1, slot);
    table->size++;
}

String *group_get(Group *group, String key, uint8_t h1) {
    // uint64_t chunk;
    // memcpy(&chunk, group->meta, 8);
    // uint64_t repeated = h1 * 0x0101010101010101ULL;
    // uint64_t x = chunk ^ repeated;
    // uint64_t matches = (x - 0x0101010101010101ULL) & ~x & 0x8080808080808080ULL;

    // while (matches) {
    //     int first_bit = __builtin_ctzll(matches);
    //     int first = first_bit >> 3;
    //     if (string_eq(group->keys[first], key)) {
    //         return &group->values[first];
    //     }
    //     matches = matches & (~(1 << first_bit));
    // }


    __m128i mask = _mm_set1_epi8(h1);
    __m128i data = _mm_loadl_epi64((const __m128i*)group->meta);
    uint16_t matches = _mm_movemask_epi8(_mm_cmpeq_epi8(data, mask));
    matches &= 0xFF;
    while (matches) {
        int first = __builtin_ctz(matches);
        if (string_eq(group->keys[first], key)) {
            return &group->values[first];
        }
        matches = matches & (~(1 << first));
    }



    if (group->overflow == NULL) {
        return NULL;
    }
    return group_get(group, key, h1);
}

String *hashtable_get(HashTable *table, String key) {
    uint32_t hash = _hash(key.content);
    uint32_t slot = hash % table->max_slots;
    uint8_t h1 = (hash >> 25) | 0b10000000;
    return group_get(&table->slots[slot], key, h1);
}

String *group_get_str(Group *group, const char* key, uint8_t h1, uint32_t n) {
    // uint64_t chunk;
    // memcpy(&chunk, group->meta, 8);
    // uint64_t repeated = h1 * 0x0101010101010101ULL;
    // uint64_t x = chunk ^ repeated;
    // uint64_t matches = (x - 0x0101010101010101ULL) & ~x & 0x8080808080808080ULL;

    // while (matches != 0) {
    //     int first_bit = __builtin_ctzll(matches);
    //     int first = first_bit >> 3;
    //     if (strcmp(group->keys[first].content, key) == 0) {
    //         return &group->values[first];
    //     }
    //     matches = matches & (~(1ull << first_bit));
    // }


    __m128i mask = _mm_set1_epi8(h1);
    __m128i data = _mm_loadl_epi64((const __m128i*)group->meta);
    uint16_t matches = _mm_movemask_epi8(_mm_cmpeq_epi8(data, mask));
    matches &= 0xFF;
    while (matches) {
        int first = __builtin_ctz(matches);
        if (strcmp(group->keys[first].content, key) == 0) {
            return &group->values[first];
        }
        matches = matches & (~(1 << first));
    }

    if (group->overflow == NULL) {
        return NULL;
    }
    return group_get_str(group->overflow, key, h1, n);
}

String *hashtable_get_str(HashTable *table, const char* key) {
    uint32_t hash = _hash(key);
    uint32_t slot = hash % table->max_slots;
    uint8_t h1 = (hash >> 25) | 0b10000000;
    return group_get_str(&table->slots[slot], key, h1, slot);
}

