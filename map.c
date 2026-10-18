#include "map.h"
#include <stdio.h>
#include <immintrin.h>

typedef struct Map {
    int entries_len;

    int hash_seed;
    int global_depth;

    void* dir_ptr; // ссылка на директорию или группу
    int dir_len;
} Map;

typedef struct Slot {
    void* key;
    int key_len;

    void* value;
    int value_len;

    char status;
} Slot;

typedef struct Group {
    Slot slots[8];
    long control_word;
} Group;

typedef struct Table {
    Group** groups;
    int len;

    int local_depth;
} Table;

typedef struct Directory {
    Table** tables;
    int len;
} Directory;

void* map_new(int len);
void map_set_key(void* map_ptr, char* key, char* value);

int group_probe(Group* group, char mask);

long hash_string(char* str);

void* map_new(int len) {
    if (len <= 8) {
        // создаем только группу
    }

    int table_len = 2;
    while (1) {
        if (table_len * 7 <= len) {
            table_len *= 2;
            continue;
        }
        break;
    }

    Group** groups_arr = malloc(sizeof(Group) * table_len);

    for (int group_idx = 0; group_idx < table_len; group_idx++) {
        Group* group = calloc(1, sizeof(Group));
        groups_arr[group_idx] = group;
    }

    Table* table = malloc(sizeof(Table));
    table->groups = groups_arr;
    table->len = table_len;
    table->local_depth = 0;

    Table** tables_arr = malloc(sizeof(Table) * 1);
    tables_arr[0] = table;

    Directory* dir = malloc(sizeof(Directory));
    dir->tables = tables_arr;
    dir->len = 1;

    Map* map = malloc(sizeof(Map));
    map->entries_len = 0;
    map->global_depth = 0;
    map->dir_ptr = (void*)dir;
    map->dir_len = dir->len;

    return (void*)map;
}

void map_set_key(void* map_ptr, char* key, char* value) {
    Map* map = (Map*)map_ptr;
    long hash = hash_string(key);

    // если map->dir_len == 0, значит работаем напрямую с группой

    Directory* dir = (Directory*)(map->dir_ptr);
    Table* table = NULL;

    if (map->global_depth == 0) {
        table = dir->tables[0];
    }
    else {
        table = dir->tables[(int)hash >> (64 - map->global_depth)];
    }
    
    long h1 = (hash >> 7) & 0xFFFFFFFFFFFFFFFF; // 57 старших битов
    char h2 = (char)hash & (0xFF >> 1); // 7 младших битов

    Group* group = table->groups[(int)h1 & (table->len-1)];
    printf("slot 0 status: %d \n", group->slots[0].status);
}

int group_probe(Group* group, char mask) {
    __m128i vector = _mm_loadl_epi64((__m128i*)group->control_word);
    __m128i mask_vector = _mm_set1_epi8(mask);
    __m128i cmp = _mm_cmpeq_epi8(vector, mask_vector);

    int bits = _mm_movemask_epi8(cmp);
    return bits != 0;
}

long hash_string(char* str) {
    return 0;
}