#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <immintrin.h>
#include "dynstring.h"

#define _FREE_SLOT 0
#define _RECLAIMED_SLOT 0b01111111

typedef struct {
    uint32_t max_slots;
    uint32_t used_slots;
    uint32_t reclaimed_slots;

    uint8_t *metadata;
    String *keys;
    String *values;
    
} Table;

uint32_t next_power_of_two_32(uint32_t x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

Table init_table(uint32_t size) {
    assert(size > 40);
    size = next_power_of_two_32(size);
    String *keys   = malloc(size * sizeof(String));
    String *values = malloc(size * sizeof(String));
    uint8_t *metadata = calloc(size + 16, sizeof(uint8_t));

    return (Table) {
        .max_slots = size,
        .used_slots = 0,
        .reclaimed_slots = 0,
        .metadata = metadata,
        .keys = keys,
        .values = values
    };
}

void free_table(Table *table) {
    for (uint32_t i = 0; i < table->max_slots; i++) {
        if (((table->metadata[i] >> 7) & 1) == 1) {
            free_string(table->keys[i]);
            free_string(table->values[i]);
        }
    }

    free(table->metadata);
    free(table->keys);
    free(table->values);
}

static inline uint32_t _table_find_from(Table *table, uint32_t from, uint8_t hash, uint32_t *first_reclaimed_idx) {

    assert(table != nullptr);
    assert(table->used_slots + table->reclaimed_slots < table->max_slots);
    assert(from < table->max_slots);

    uint8_t meta = table->metadata[from];
    if (meta == hash || meta == _FREE_SLOT) return from;
    if (meta == _RECLAIMED_SLOT && *first_reclaimed_idx == 0xFFFFFFFF) *first_reclaimed_idx = from;

    __m128i key_mask = _mm_set1_epi8(hash);
    __m128i empty_mask = _mm_set1_epi8(_FREE_SLOT);
    __m128i reclaimed_mask = _mm_set1_epi8(_RECLAIMED_SLOT);

    while (true) {
            __m128i metadata_chunk = _mm_loadu_si128((const __m128i_u *)(table->metadata + from));
            uint16_t key_matches = _mm_movemask_epi8(_mm_cmpeq_epi8(metadata_chunk, key_mask));
            uint16_t empty_matches = _mm_movemask_epi8(_mm_cmpeq_epi8(metadata_chunk, empty_mask));

            uint16_t relevant_matches = key_matches | empty_matches;
            if (relevant_matches) {
                int first_event =  __builtin_ctz(relevant_matches);
                if (key_matches & (1 << first_event)) {
                    return (from + first_event) & (table->max_slots - 1);
                } else {
                    return *first_reclaimed_idx == 0xFFFFFFFF ? (from + first_event) & (table->max_slots - 1) : *first_reclaimed_idx;
                }
            }

            uint16_t reclaimed_matches = _mm_movemask_epi8(_mm_cmpeq_epi8(metadata_chunk, reclaimed_mask));

            // if we have a reclaimed slot, we might have to update first_reclaimed
            if (reclaimed_matches && *first_reclaimed_idx == 0xFFFFFFFF) {
                int first_reclaimed =  __builtin_ctz(reclaimed_matches);
                *first_reclaimed_idx = (from + first_reclaimed) & (table->max_slots - 1);
            }

            from = (from + 16) & (table->max_slots - 1);
    }
}

static inline uint32_t _table_find_from_get(Table *table, uint32_t from, uint8_t hash) {

    assert(table != nullptr);
    assert(table->used_slots + table->reclaimed_slots < table->max_slots);
    assert(from < table->max_slots);

    uint8_t meta = table->metadata[from];
    if (meta == hash || meta == _FREE_SLOT) return from;

    __m128i key_mask = _mm_set1_epi8(hash);
    __m128i empty_mask = _mm_set1_epi8(_FREE_SLOT);

    while (true) {
            __m128i metadata_chunk = _mm_loadu_si128((const __m128i_u *)(table->metadata + from));
            uint16_t key_matches = _mm_movemask_epi8(_mm_cmpeq_epi8(metadata_chunk, key_mask));
            uint16_t empty_matches = _mm_movemask_epi8(_mm_cmpeq_epi8(metadata_chunk, empty_mask));
            uint16_t relevant_matches = key_matches | empty_matches;
            if (relevant_matches) {
                int first_event =  __builtin_ctz(relevant_matches);
                return (from + first_event) & (table->max_slots - 1);
            }

            from = (from + 16) & (table->max_slots - 1);
    }
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

void table_insert(Table *table, String key, String value);

void table_reallocate(Table *table, uint32_t size) {
    Table new_table = init_table(size);
    for (uint32_t i = 0; i < table->max_slots; i++) {
        if (((table->metadata[i] >> 7) & 1) == 1) {
            table_insert(&new_table, table->keys[i], table->values[i]);
        }
    }

    free(table->metadata);
    free(table->keys);
    free(table->values);
    *table = new_table; 
}

void table_insert(Table *table, String key, String value) {
    assert(table != nullptr);

    if ((table->used_slots + table->reclaimed_slots + 1) * 1.2 >= table->max_slots) {
        table_reallocate(table, table->max_slots * 2);
    }

    uint32_t h = _hash(key.content);
    uint8_t h1 = (h >> 25) | 0b10000000;
    uint32_t slot = h & (table->max_slots - 1);
    uint32_t first_reclaimed_idx = 0xFFFFFFFF;

    while (true) {
        slot = _table_find_from(table, slot, h1, &first_reclaimed_idx);

        if (table->metadata[slot] == _FREE_SLOT || table->metadata[slot] == _RECLAIMED_SLOT) {
            if (slot < 16) {
                table->metadata[table->max_slots + slot] = h1;
            }
            table->metadata[slot] = h1;
            table->keys[slot] = key;
            table->values[slot] = value;
            table->used_slots++;
            return;
        } else if (string_eq(table->keys[slot], key)) {
            free_string(key);
            free_string(table->values[slot]);
            table->values[slot] = value;
            return;
        }
        slot = (slot + 1) & (table->max_slots - 1);
    }
}

void table_remove(Table *table, String key) {
    assert(table != nullptr);

    uint32_t h = _hash(key.content);
    uint8_t h1 = (h >> 25) | 0b10000000;
    uint32_t slot = h & (table->max_slots - 1);
    uint32_t first_reclaimed_idx = 0xFFFFFFFF;

    while (true) {
        slot = _table_find_from(table, slot, h1, &first_reclaimed_idx);

        if (table->metadata[slot] == _FREE_SLOT || table->metadata[slot] == _RECLAIMED_SLOT) {
            return;
        } else if (string_eq(table->keys[slot], key)) {
            if (slot < 16) {
                table->metadata[table->max_slots + slot] = _RECLAIMED_SLOT;
            }
            table->metadata[slot] = _RECLAIMED_SLOT;
            free_string(table->keys[slot]);
            free_string(table->values[slot]);
            table->used_slots--;
            table->reclaimed_slots++;
            return;
        }
        slot = (slot + 1) & (table->max_slots - 1);
    }
}

String *table_get(Table *table, String key) {
    assert(table != nullptr);

    uint32_t h = _hash(key.content);
    uint8_t h1 = (h >> 25) | 0b10000000;
    uint32_t slot = h & (table->max_slots - 1);

    while (true) {
        slot = _table_find_from_get(table, slot, h1);
        if (table->metadata[slot] == _FREE_SLOT || table->metadata[slot] == _RECLAIMED_SLOT) {
            return nullptr;
        } else if (string_eq(table->keys[slot], key)) {
            return &table->values[slot];
        }
        slot = (slot + 1) & (table->max_slots - 1);
    }
}

String *table_get_str(Table *table, const char *key) {
    assert(table != nullptr);

    uint32_t h = _hash(key);
    uint8_t h1 = (h >> 25) | 0b10000000;
    uint32_t slot = h & (table->max_slots - 1);

    while (true) {
        slot = _table_find_from_get(table, slot, h1);
        if (table->metadata[slot] == _FREE_SLOT || table->metadata[slot] == _RECLAIMED_SLOT) {
            return nullptr;
        } else if (strcmp(table->keys[slot].content, key) == 0) {
            return &table->values[slot];
        }
        slot = (slot + 1) & (table->max_slots - 1);
    }
}