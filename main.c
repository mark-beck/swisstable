#include <stdio.h>
#include "swisstable.h"
#include "string.h"

void run_n_inserts(long n) {
    Table table = init_table(1000);

    for (long i = 0; i < n; i++) {
        char str[30];
        snprintf(str, 30, "key-%ld", i);
        String key = init_string(str);
        snprintf(str, 30, "val-%ld", i);
        String val = init_string(str);
        table_insert(&table, key, val);
    }

    for (long i = 0; i < n; i++) {
        char str[30];
        snprintf(str, 30, "key-%ld", i);
        String key = init_string(str);
        snprintf(str, 30, "val-%ld", i);
        String val = init_string(str);
        String *res = table_get(&table, key);
        if (res == NULL) {
            printf("no value on key %s\n", key.content);
            return;
        }
        assert(res != NULL);
        assert(string_eq(val, *res));
        free_string(key);
        free_string(val);
    }
    free_table(&table);
}

int main() {
    printf("Hello world\n");
    Table table = init_table(1000);

    table_insert(&table, init_string("key1"), init_string("value1"));
    table_insert(&table, init_string("key2"), init_string("value2"));
    
    table_insert(&table, init_string("key3"), init_string("value3"));
    table_remove(&table, init_string("key2"));
    table_insert(&table, init_string("key1"), init_string("value11"));
    String *val1 = table_get(&table, init_string("key1"));
    printf("val1: %s\n", val1->content);
    free_table(&table);

    run_n_inserts(50000);
}

