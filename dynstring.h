#pragma once

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