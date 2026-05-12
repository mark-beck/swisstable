#include <stdio.h>
#include "hashtable.h"
#define STRING_ARENA
#include "dynstring.h"

void run_n_inserts(long n) {
    HashTable table = init_hashtable(1000000);

    for (long i = 0; i < n; i++) {
        char str[30];
        snprintf(str, 30, "key-%ld", i);
        String key = init_string(str);
        snprintf(str, 30, "val-%ld", i);
        String val = init_string(str);
        hashtable_insert(&table, key, val);
    }
    printf("finished inserting");

    for (long i = 0; i < n; i++) {
        char str[30];
        char key[30];
        snprintf(key, 30, "key-%ld", i);
        snprintf(str, 30, "val-%ld", i);
        String val = init_string(str);
        String *res = hashtable_get_str(&table, key);
        if (res == NULL) {
            printf("no value on key %s\n", key);
            return;
        }
        assert(res != NULL);
        assert(string_eq(val, *res));
        free_string(val);
    }
    free_hashtable(&table);
}

int main() {
    run_n_inserts(850000);
}
