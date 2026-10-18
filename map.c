#include "map.h"
#include <immintrin.h>

typedef struct Map {
    char key_type[3];
    char value_type[3];
    int len;

    int hash_seed;
    int global_depth;

    void* dir_ptr; // ссылка на директорию или группу
    int dir_len;
} Map;

typedef struct Directory {
    Table** tables;
    int len;
} Directory;

typedef struct Table {
    Group** groups;
    int len;

    int local_depth;
} Table;

typedef struct Group {
    Slot** slots;
    int len;

    long control_word;
} Group;

typedef struct Slot {
    void* data[8];
} Slot;

void* map_new(char* key_type, char* value_type, int len);
void map_set_key(void* map_ptr, char* key, char* value);

int group_probe(Group* group, char mask);

long hash_string(char* str);

void* map_new(char* key_type, char* value_type, int len) {
    if (len <= 8) {
        // создаем только группу
    }

    Slot* slot = malloc(sizeof(Slot));
    Slot** slots_arr = malloc(sizeof(Slot) * 1);
    slots_arr[0] = slot;

    Group* group = malloc(sizeof(Group));
    group->slots = slots_arr;
    group->len = 1;
    group->control_word = 0;

    Group** groups_arr = malloc(sizeof(Group) * 1);
    groups_arr[0] = group;

    Table* table = malloc(sizeof(Table));
    table->groups = groups_arr;
    table->len = 1;
    table->local_depth = 1;

    Table** tables_arr = malloc(sizeof(Table) * 1);
    tables_arr[0] = table;

    Directory* dir = malloc(sizeof(Directory));
    dir->tables = tables_arr;
    dir->len = 1;

    Map* map = malloc(sizeof(Map));
    map->len = 0;
    map->global_depth = 1;
    map->dir_ptr = (void*)dir;
    map->dir_len = dir->len;

    return (void*)map;
}

void map_set_key(void* map_ptr, char* key, char* value) {
    Map* map = (Map*)map_ptr;
    long hash = hash_string(key);
    long h1 = (hash >> 7) & 0xFFFFFFFFFFFFFFFF; // 57 старших битов
    char h2 = (char)hash & (0xF >> 1); // 7 младших битов

    // если map->dir_len == 0, значит работаем напрямую с группой

    char h1_mask = (h1 << (8 - map->global_depth)) >> (8 - map->global_depth); // временное решение!!
    Directory* dir = (Directory*)(map->dir_ptr);
    Table* table = dir->tables[(int)h1 & h1_mask];

    // поиск по группам
    for (int i = 0; i < table->len; i++) {
        int res = 0;
        res = group_probe(table->groups[i], h2);
        if (res == 1) {
            // ищем в самой группе

            break;
        }
    }
}

int group_probe(Group* group, char mask) {
    __m128i vector = _mm_loadl_epi64((__m128i*)group->control_word);
    __m128i mask_vector = _mm_set1_epi8(mask);
    __m128i cmp = _mm_cmpeq_epi8(vector, mask_vector);

    int bits = _mm_movemask_epi8(cmp);
    return bits != 0;
}