#include "map.h"
#include <stdlib.h>
// #include <immintrin.h>
#include <mmintrin.h>

typedef struct Slot {
    void* key;
    void* value;
} Slot;

typedef struct Group {
    Slot slots[8];
    unsigned long control_word;

    int spreaded; // имеет данные в след группах (пробирование)
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
int map_len(void* map_ptr);
void map_set_key(void* map_ptr, char* key, char* value);
int map_has_key(void* map_ptr, char* key);
void* map_get_key(void* map_prt, char* key);
void map_delete_key(void* map_prt, char* key);

int group_try_insert(Group* group, char h2, char* key, char* value);

void init_group_control_word(Group* group);
int get_idx_for_mask(long control_word, char mask);
int get_first_free_idx(long control_word);
int get_first_dead_idx(long control_word);

int next_probe_idx(int curr_idx, int len, int step);

char convert_long_to_bitmap(long l);
long hash_string(char* str);

void* map_new(int len) {
    if (len <= 8) {
        // создаем только одну группу
        Group* group = calloc(1, sizeof(Group));
        init_group_control_word(group);

        Map* map = malloc(sizeof(Map));
        map->entries_len = 0;
        map->global_depth = 0;
        map->dir_ptr = (void*)group;
        map->dir_len = 0;

        return (void*)map;
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

int map_len(void* map_ptr) {
    if (map_ptr == NULL) {
        return -1;
    }
    
    Map* map = (Map*)map_ptr;
    return map->entries_len;
}

void map_set_key(void* map_ptr, char* key, char* value) {
    if (map_ptr == NULL) {
        return;
    }
    
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
    if (group_try_insert(group, h2, key, value) == 0) {
        return;
    }

    // в данной группе нет места - пробируем след группы
    int group_idx = (int)h1 & (table->len-1);
    if (group_idx + 1 >= table->len) {
        group_idx = 0;
    }

    int probe_step = 1;
    for (; group_idx < table->len; group_idx = next_probe_idx(group_idx, table->len, ++probe_step)) {
        group = table->groups[group_idx];
        if (group_try_insert(group, h2, key, value) == 0) {
            return;
        }
    }
}

int group_try_insert(Group* group, char h2, char* key, char* value) {
    // ищем совпадение h2 в контрольном слове
    int i = get_idx_for_mask(group->control_word, h2);
    if (i != -1) {
        group->slots[i].key = key;
        group->slots[i].value = value;

        // контрольное слово не меняется, т.к. оно уже равно h2

        return 0;
    }

    // ключа нет, ищем свободное место для вставки
    i = get_first_free_idx(group->control_word);
    if (i != -1) {
        group->slots[i].key = key;
        group->slots[i].value = value;

        unsigned long control_word_new_part = (long)h2 << 8*i;
        unsigned long mask = 0xFF << 8*i;
        unsigned long control_word_old_part = mask ^ 0xFFFFFFFFFFFFFFFF;

        group->control_word = control_word_new_part | control_word_old_part;

        return 0;
    }

    return -1;
}

void init_group_control_word(Group* group) {
    __m64 vector = _mm_set1_pi8((unsigned char)128);

    unsigned long value = _m_to_int64(vector);
    group->control_word = value;
}

int get_idx_for_mask(long control_word, char mask) {
    __m64 vector = _m_from_int64(control_word);
    __m64 vector_mask = _mm_set1_pi8((unsigned char)mask);
    __m64 cmp = _mm_cmpeq_pi8(vector, vector_mask);

    unsigned long value = _m_to_int64(vector);
    unsigned char bitmap = convert_long_to_bitmap(value);

    if (bitmap == 0) {
        return -1;
    }

    return __builtin_ctz((unsigned int)bitmap);
}

int get_first_free_idx(long control_word) {
    __m64 vector = _m_from_int64(control_word);
    __m64 vector_mask = _mm_set1_pi8((unsigned char)128);
    __m64 cmp = _mm_cmpeq_pi8(vector, vector_mask);

    unsigned long value = _m_to_int64(cmp);
    unsigned char bitmap = convert_long_to_bitmap(value);

    if (bitmap == 0) {
        return -1;
    }

    return __builtin_ctz((unsigned int)bitmap);
}

int get_first_dead_idx(long control_word) {
    __m64 vector = _m_from_int64(control_word);
    __m64 vector_mask = _mm_set1_pi8((unsigned char)254);
    __m64 cmp = _mm_cmpeq_pi8(vector, vector_mask);

    unsigned long value = _m_to_int64(cmp);
    unsigned char bitmap = convert_long_to_bitmap(value);

    if (bitmap == 0) {
        return -1;
    }

    return __builtin_ctz((unsigned int)bitmap);
}

int next_probe_idx(int curr_idx, int len, int step) {
    /*
        Нужно сделать закольцовку индекса, чтобы при curr_idx = 4, len = 5 и offset = 2 произошел сброс индекса до 0
        и относительное смещение, т.е. чтобы получилось new_idx = 1
    */
    
    int offset = curr_idx + step;
    int new_idx = curr_idx + offset;
    while (new_idx >= len) {
        int available_incr = (len-1) - curr_idx;
        new_idx = new_idx - (curr_idx - available_incr + 1);
    }

    return offset;
}

char convert_long_to_bitmap(long l) {
    unsigned char bit8 = (unsigned char)(l >> 56) & (unsigned char)128;
    unsigned char bit7 = (unsigned char)(l >> 48) & (unsigned char)64;
    unsigned char bit6 = (unsigned char)(l >> 40) & (unsigned char)32;
    unsigned char bit5 = (unsigned char)(l >> 32) & (unsigned char)16;
    unsigned char bit4 = (unsigned char)(l >> 24) & (unsigned char)8;
    unsigned char bit3 = (unsigned char)(l >> 18) & (unsigned char)4;
    unsigned char bit2 = (unsigned char)(l >> 8) & (unsigned char)2;
    unsigned char bit1 = (unsigned char)(l & 1);

    char res = 0xFF;
    res &= bit8 | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1;

    return res;
}

long hash_string(char* str) {
    return 0;
}