#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <immintrin.h>

typedef struct {
    char *content;
    uint32_t size;
} String;

String init_string(char *str) {
    size_t len = strlen(str);

    char *content = malloc(len * sizeof(char) + 1);
    memcpy(content, str, len + 1);

    String string = {
        .content = content,
        .size = len
    };

    return string;
}

void free_string(String str) {
    free(str.content);
}

bool string_eq(String s1, String s2) {
    if (s1.size == s2.size) {
        return memcmp(s1.content, s2.content, s1.size) == 0;
    } else {
        return false;
    }
}

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

Table init_table(uint32_t size) {
    String *keys   = malloc(size * sizeof(String));
    String *values = malloc(size * sizeof(String));
    uint8_t *metadata = calloc(size, sizeof(uint8_t));

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

uint32_t _table_find_from(Table *table, uint32_t from, uint8_t hash) {
    assert(table != nullptr);
    assert(table->used_slots + table->reclaimed_slots < table->max_slots);
    assert(from < table->max_slots);

    uint32_t first_reclaimed_idx = 0xFF;

    __m128i key_mask = _mm_set1_epi8(hash);
    __m128i empty_mask = _mm_set1_epi8(_FREE_SLOT);
    __m128i reclaimed_mask = _mm_set1_epi8(_RECLAIMED_SLOT);

    while (true) {
        // only compare 16 bytes at once when there is enough space to load 16 bytes from metadata
        if (from + 16 < table->max_slots) {
            __m128i metadata_chunk = _mm_loadu_si128((const __m128i_u *)(table->metadata + from));
            uint16_t key_matches = _mm_movemask_epi8(_mm_cmpeq_epi8(metadata_chunk, key_mask));
            uint16_t empty_matches = _mm_movemask_epi8(_mm_cmpeq_epi8(metadata_chunk, empty_mask));
            uint16_t reclaimed_matches = _mm_movemask_epi8(_mm_cmpeq_epi8(metadata_chunk, reclaimed_mask));

            // if we find an empty slot or a match, we can return
            if (empty_matches || key_matches) {
                int first_zero = __builtin_clz(empty_matches) - 16;
                int first_match = __builtin_clz(key_matches) - 16;
                if (first_match < first_zero) {
                    return from + first_match;
                } else {
                    int first_reclaimed =  __builtin_clz(reclaimed_matches) - 16;
                    return first_zero < first_reclaimed ? from + first_zero : from + first_reclaimed;
                }
            }

            // if we have a reclaimed slot, we might have to update first_reclaimed
            if (reclaimed_matches && first_reclaimed_idx == 0xFF) {
                int first_reclaimed =  __builtin_clz(reclaimed_matches) - 16;
                first_reclaimed_idx = from + first_reclaimed;
            }

            from = (from + 16) % table->max_slots;
        } else {
            // do normal compare
            if (table->metadata[from] == hash) {
                return from;
            }

            if (table->metadata[from] == _FREE_SLOT) {
                if (first_reclaimed_idx != 0xFF) {
                    return first_reclaimed_idx;
                }
                return from;
            }

            if (first_reclaimed_idx == 0xFF && table->metadata[from] == _RECLAIMED_SLOT) {
                first_reclaimed_idx = from;
            }

            from = (from + 1) % table->max_slots;
        }   
    }
}

uint32_t _hash(char *str)
{
    uint32_t hash = 5381;
    uint32_t c;

    while (c = *str++)
        hash = hash * 33 + c;

    return hash;
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

    if ((table->used_slots + table->reclaimed_slots + 1) * 1.5 >= table->max_slots) {
        table_reallocate(table, table->max_slots * 2);
    }

    uint32_t h = _hash(key.content);
    uint8_t h1 = (h >> 25) | 0b10000000;
    uint32_t slot = h % table->max_slots;

    while (true) {
        slot = _table_find_from(table, slot, h1);

        if (table->metadata[slot] == _FREE_SLOT || table->metadata[slot] == _RECLAIMED_SLOT) {
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
        slot = (slot + 1) % table->max_slots;
    }
}

void table_remove(Table *table, String key) {
    assert(table != nullptr);

    uint32_t h = _hash(key.content);
    uint8_t h1 = (h >> 25) | 0b10000000;
    uint32_t slot = h % table->max_slots;

    while (true) {
        slot = _table_find_from(table, slot, h1);

        if (table->metadata[slot] == _FREE_SLOT || table->metadata[slot] == _RECLAIMED_SLOT) {
            return;
        } else if (string_eq(table->keys[slot], key)) {
            table->metadata[slot] = _RECLAIMED_SLOT;
            free_string(table->keys[slot]);
            free_string(table->values[slot]);
            table->used_slots--;
            table->reclaimed_slots++;
            return;
        }
        slot = (slot + 1) % table->max_slots;
    }
}

String *table_get(Table *table, String key) {
    assert(table != nullptr);

    uint32_t h = _hash(key.content);
    uint8_t h1 = (h >> 25) | 0b10000000;
    uint32_t slot = h % table->max_slots;

    while (true) {
        slot = _table_find_from(table, slot, h1);
        if (table->metadata[slot] == _FREE_SLOT || table->metadata[slot] == _RECLAIMED_SLOT) {
            return nullptr;
        } else if (string_eq(table->keys[slot], key)) {
            return &table->values[slot];
        }
        slot = (slot + 1) % table->max_slots;
    }
}