#include "map.h"
#include <string.h>
#include <stdlib.h>
// #include <immintrin.h>
#include <mmintrin.h>

typedef struct Slot {
    void* key;
    void* value;
} Slot;

typedef struct Group {
    Slot slots[8];
    long control_word;
} Group;

typedef struct Table {
    Group** groups;
    int len;

    int local_depth;
    int free_slots_len;
} Table;

typedef struct Directory {
    Table** tables;
    int len;
} Directory;

typedef struct Map {
    int entries_len;

    int hash_seed;
    int global_depth;

    void* dir_ptr; // ссылка на директорию или группу
    int dir_len;
} Map;

void* map_new(int len);
void map_set_key(void* map_ptr, char* key, char* value);

void init_group_control_word(Group* group);
int get_idx_for_mask(long control_word, char mask);
int get_first_free_idx(long control_word);
int group_probe(Group* group, char mask);

char convert_long_to_char(long l);
long hash_string(char* str);

void* map_new(int len) {
    if (len <= 8) {
        // создаем только группу
    }

    int groups_amount = 2;
    while (1) {
        if (groups_amount * 7 <= len) {
            groups_amount *= 2;
            continue;
        }
        break;
    }

    if (groups_amount > 1024) {
        // создаем несколько таблиц
    }

    Group** groups_arr = malloc(sizeof(Group) * groups_amount);

    for (int group_idx = 0; group_idx < groups_amount; group_idx++) {
        Group* group = calloc(1, sizeof(Group));
        init_group_control_word(group);
        groups_arr[group_idx] = group;
    }

    Table* table = malloc(sizeof(Table));
    table->groups = groups_arr;
    table->len = groups_amount;
    table->local_depth = 0;
    table->free_slots_len = groups_amount * 7;

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
    int i = get_idx_for_mask(group->control_word, h2);
    if (i != -1) {
        group->slots[i].key = key;
        group->slots[i].value = value;

        return;
    }
}

void init_group_control_word(Group* group) {
    __m64 vector = _mm_set1_pi8((unsigned char)128);

    long value = _m_to_int64(vector);
    group->control_word = value;
}

int get_idx_for_mask(long control_word, char mask) {
    __m64 vector = _m_from_int64(control_word);
    __m64 vector_mask = _mm_set1_pi8((unsigned char)mask);
    __m64 cmp = _mm_cmpeq_pi8(vector, vector_mask);

    long value = _m_to_int64(vector);
    char bitmap = convert_long_to_char(value);

    if (bitmap == 0) {
        return -1;
    }

    return __builtin_ctz((unsigned int)bitmap);
}

int get_first_free_idx(long control_word) {
    __m64 vector = _m_from_int64(control_word);
    __m64 vector_mask = _mm_set1_pi8((unsigned char)128);
    __m64 cmp = _mm_cmpeq_pi8(vector, vector_mask);

    long value = _m_to_int64(cmp);
    char mask = ( ((unsigned char)(value >> 49) & (unsigned char)128) | ((char)value >> 42) | ((char)value >> 35) |  ((char)value >> 28) |
    ((char)value >> 21) | ((char)value >> 15) | ((char)value)
    );
}

// int group_probe(Group* group, char mask) {
//     __m128i vector = _mm_loadl_epi64((__m128i*)group->control_word);
//     __m128i mask_vector = _mm_set1_epi8(mask);
//     __m128i cmp = _mm_cmpeq_epi8(vector, mask_vector);

//     int bits = _mm_movemask_epi8(cmp);
//     return bits != 0;
// }

char convert_long_to_char(long l) {

}

long hash_string(char* str) {
    return 0;
}