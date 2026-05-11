#include <stdio.h>
#include "swisstable.h"

void run_n_inserts(long n) {
    Table table = init_table(1000);

    for (long i = 0; i < n; i++) {
        char str[20];
        snprintf(str, 24, "key-%ld", i);
        String key = init_string(str);
        snprintf(str, 24, "val-%ld", i);
        String val = init_string(str);
        table_insert(&table, key, val);
    }

    for (long i = 0; i < n; i++) {
        char str[20];
        snprintf(str, 24, "key-%ld", i);
        String key = init_string(str);
        snprintf(str, 24, "val-%ld", i);
        String val = init_string(str);
        String *res = table_get(&table, key);
        assert(string_eq(val, *res));
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

    run_n_inserts(500);
}

