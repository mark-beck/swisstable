#include <stdio.h>
#include "swisstable.h"


int main() {
    printf("Hello world\n");
    Table table = init_table(3);

    table_insert(&table, init_string("key1"), init_string("value1"));
    table_insert(&table, init_string("key2"), init_string("value2"));
    
    table_insert(&table, init_string("key3"), init_string("value3"));
    table_remove(&table, init_string("key2"));
    table_insert(&table, init_string("key1"), init_string("value11"));
    String *val1 = table_get(&table, init_string("key1"));
    printf("val1: %s\n", val1->content);
    free_table(&table);
}

