#include "map.h"
#include "models.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <mmintrin.h>

void* map_new(int len);
int map_len(void* map_ptr);
void map_set_key(void* map_ptr, char* key, char* value);
int map_has_key(void* map_ptr, char* key);
void* map_get_key(void* map_prt, char* key);
void map_delete_key(void* map_prt, char* key);

int group_try_insert(Group* group, unsigned char h2, char* key, char* value);

Table* get_matching_table(Map* map, unsigned long hash);
Group* get_matching_group(Table* table, unsigned long h1);
int get_group_idx(Table* table, unsigned long h1);
int get_matching_slot_idx(Group* group, unsigned char h2, char* key);
int get_first_available_slot_idx(Group* group);

Map* init_small_map(void);
void free_small_map(Map* map);
Map* init_big_map(int groups_amount);
Map* map_evolve(Map* small_map);
void table_grow(Map* map, Table* table);

Group* init_group(void);
Table* init_table(int groups_amount);

void init_control_word(Group* group);
void update_control_word(Group* group, int slot_idx, unsigned char new_part);
int get_bitmap(unsigned long control_word, unsigned char mask);

int probe_group_for_insert(Table* table, int start_group_idx, unsigned h2, char* key, char* value);

int incr_idx_looped(int start_idx, int incr_amount, int loop_len);
char convert_long_to_bitmap(unsigned long l);
unsigned long hash_string(unsigned long seed, char* str);

void* map_new(int len) {
    Map* map = NULL;
    
    if (len <= 8) {
        // создаем маленькую мапу с одной группой
        map = init_small_map();
    }
    else {
        int groups_amount = 2;
        while (1) {
            if (groups_amount * 7 <= len) {
                groups_amount *= 2;
                continue;
            }
            break;
        }

        map = init_big_map(groups_amount);    
    }

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
    unsigned long hash = hash_string(map->hash_seed, key);
    unsigned long h1 = (hash >> 7) & 0xFFFFFFFFFFFFFFFF; // 57 старших битов
    unsigned char h2 = (char)hash & (0xFF >> 1); // 7 младших битов

    if (map->dir_len == 0) {
        // работаем напрямую с группой

        Group* group = (Group*)map->dir_ptr;
        if (group_try_insert(group, h2, key, value) == 0) {
            map->entries_len++;
            
            return;
        }

        map = map_evolve(map);
    }

    while (1) {
        Table* table = get_matching_table(map, hash);
        if (table->free_slots_len <= 0) {
            grow_table(map, table);
            continue;
        }

        Group* group = get_matching_group(table, h1);

        // пробуем вставить в данную группу
        if (group_try_insert(group, h2, key, value) == 0) {
            map->entries_len++;
            table->free_slots_len--;
            
            return;
        }

        // в данной группе нет места - пробируем след группы
        if (probe_group_for_insert(table, get_group_idx(table, h1), h2, key, value) == 0) {
            map->entries_len++;
            table->free_slots_len--;
            
            return;
        }
        
        // нет свободных групп - увеличиваем таблицу
        grow_table(map, table);
    }
}

int group_try_insert(Group* group, unsigned char h2, char* key, char* value) {
    int slot_idx = get_matching_slot_idx(group, h2, key);
    if (slot_idx != -1) {
        // ключ не меняется, т.к. он уже совпадает
        group->slots[slot_idx].value = value;
        // контрольное слово не меняется, т.к. оно уже равно h2

        return 0;
    }

    slot_idx = get_first_available_slot_idx(group);
    if (slot_idx != -1) {
        group->slots[slot_idx].key = key;
        group->slots[slot_idx].value = value;
        update_control_word_part(group, slot_idx, h2);

        return 0;
    }

    return -1;
}

Table* get_matching_table(Map* map, unsigned long hash) {
    Directory* dir = (Directory*)(map->dir_ptr);
    Table* table = NULL;

    if (map->global_depth == 0) {
        table = dir->tables[0];
    }
    else {
        table = dir->tables[(int)hash >> (64 - map->global_depth)];
    }

    return table;
}

Group* get_matching_group(Table* table, unsigned long h1) {
    return table->groups[get_group_idx(table, h1)];
}

int get_group_idx(Table* table, unsigned long h1) {
    return (int)h1 & (table->len-1);
}

int get_matching_slot_idx(Group* group, unsigned char h2, char* key) {
    int bitmap = get_bitmap(group->control_word, h2); // все слоты, подходящие по h2
    if (bitmap == 0) {
        return -1;
    }

    int last_matching_idx = -1;
    int i = __builtin_ctz((unsigned int)bitmap);
    while (i > last_matching_idx && i <= 7) {
        if (strcmp(group->slots[i].key, key) == 0) {
            return i;
        }

        last_matching_idx = i++;
        bitmap = (bitmap >> i) << i; // отрезаем первый единичный бит справа
        if (bitmap == 0) {
            return -1;
        }
        i = __builtin_ctz((unsigned int)bitmap);
    }

    return -1;
}

int get_first_available_slot_idx(Group* group) {
    int bitmap = get_bitmap(group->control_word, (unsigned char)128); // все свободные слоты
    if (bitmap != 0) {
        return __builtin_ctz((unsigned int)bitmap);
    }

    // if (!group->spreaded) {
    //     bitmap = get_bitmap(group->control_word, (unsigned char)254); // все удаленные слоты
    //     if (bitmap != 0) {
    //         return __builtin_ctz((unsigned int)bitmap);
    //     }
    // }

    return -1;
}

Map* init_small_map(void) {
    Group* group = init_group();

    Map* map = malloc(sizeof(Map));
    map->entries_len = 0;
    map->global_depth = 0;
    map->dir_ptr = (void*)group;
    map->dir_len = 0;
    map->hash_seed = (unsigned long)time(NULL) ^ ((unsigned long)clock() << 16);

    return map;
}

void free_small_map(Map* map) {
    Group* group = (Group*)map->dir_ptr;
    free(group);
    free(map);
}

Map* init_big_map(int groups_amount) {
    if (groups_amount > 1024) {
        // создаем несколько таблиц
    }

    Group** groups_arr = malloc(sizeof(Group) * groups_amount);

    for (int group_idx = 0; group_idx < groups_amount; group_idx++) {
        Group* group = init_group();
        groups_arr[group_idx] = group;
    }

    Table* table = init_table(groups_amount);
    table->groups = groups_arr;

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
    map->hash_seed = (unsigned long)time(NULL) ^ ((unsigned long)clock() << 16);

    return map;
}

Map* map_evolve(Map* small_map) {
    Map* new_map = init_big_map(2); // мапа эволюционирует только из группы, поэтому будет достаточно минимальной большой мапы

    // Переносим данные
    Group* group = (Group*)small_map->dir_ptr;
    for (int i = 0; i < 8; i++) {
        char* key = group->slots[i].key;
        char* value = group->slots[i].value;

        map_set_key(new_map, key, value);
    }

    free_small_map(small_map);

    return new_map;
}

void table_grow(Map* map, Table* table) {

}

Group* init_group(void) {
    Group* group = calloc(1, sizeof(Group));
    init_group_control_word(group);

    return group;
}

Table* init_table(int groups_amount) {
    Table* table = malloc(sizeof(Table));
    table->len = groups_amount;
    table->local_depth = 0;
    table->free_slots_len = groups_amount * 7;

    return table;
}

void init_control_word(Group* group) {
    __m64 vector = _mm_set1_pi8((unsigned char)128);

    unsigned long value = _m_to_int64(vector);
    group->control_word = value;
}

void update_control_word(Group* group, int slot_idx, unsigned char new_part) {
    unsigned long control_word_new_part = (long)new_part << 8*slot_idx;
    unsigned long mask = 0xFF << 8*slot_idx;
    unsigned long control_word_old_part = mask ^ 0xFFFFFFFFFFFFFFFF;

    group->control_word = control_word_new_part | control_word_old_part;
}

int get_bitmap(unsigned long control_word, unsigned char mask) {
    __m64 vector = _m_from_int64(control_word);
    __m64 vector_mask = _mm_set1_pi8(mask);
    __m64 cmp = _mm_cmpeq_pi8(vector, vector_mask);

    unsigned long value = _m_to_int64(vector);
    unsigned char bitmap = convert_long_to_bitmap(value);

    return (int)bitmap;
}

int probe_group_for_insert(Table* table, int start_group_idx, unsigned h2, char* key, char* value) {
    int step = 1;
    int offset_value = step;

    while (step <= table->len) {
        Group* probe_group = table->groups[incr_idx_looped(start_group_idx, offset_value, table->len)];
        if (group_try_insert(probe_group, h2, key, value) == 0) {
            return 0;
        }

        step++;
        offset_value += step;
    }

    return -1;
}

// Надо хорошенько проверить
int incr_idx_looped(int start_idx, int incr_amount, int loop_len) {
    int next_idx = start_idx + incr_amount;
    while (next_idx >= loop_len) {
        int fill_curr_cycle = loop_len - (start_idx+1);
        int fill_next_cycle = incr_amount - fill_curr_cycle;
        next_idx = fill_next_cycle;
    }

    return next_idx;
}

char convert_long_to_bitmap(unsigned long l) {
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

unsigned long hash_string(unsigned long seed, char* str) {
    unsigned long h = seed ^ 0x9E3779B97F4A7C15UL;

    for (size_t i = 0; i < strlen(str); i++) {
        h ^= (unsigned long)str[i];
        h *= 1099511628211UL;
        h ^= h >> 33;
    }

    return h;
}