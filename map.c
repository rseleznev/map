#include "map.h"
#include <stdlib.h>
#include <string.h>
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

int group_try_insert(Group* group, unsigned char h2, char* key, char* value);

Table* get_matching_table(Map* map, unsigned long hash);
Group* get_matching_group(Table* table, unsigned long h1);
int get_group_idx(Table* table, unsigned long h1);
int get_matching_slot_idx(Group* group, unsigned char h2, char* key);
int get_first_available_slot_idx(Group* group);

void init_group_control_word(Group* group);
int get_bitmap(unsigned long control_word, unsigned char mask);
void update_control_word_part(Group* group, int slot_idx, unsigned char new_part);

Group* probe_insert_group(Table* table, int start_group_idx);

char convert_long_to_bitmap(unsigned long l);
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
    unsigned long hash = hash_string(key);
    unsigned long h1 = (hash >> 7) & 0xFFFFFFFFFFFFFFFF; // 57 старших битов
    unsigned char h2 = (char)hash & (0xFF >> 1); // 7 младших битов

    if (map->dir_len == 0) {
        // работаем напрямую с группой
    } 

    Table* table = get_matching_table(map, hash);
    Group* group = get_matching_group(table, h1);
    if (group_try_insert(group, h2, key, value) == 0) {
        return;
    }

    // в данной группе нет места - пробируем след группы
    Group* probe_group = probe_insert_group(table, get_group_idx(table, h1));
    if (probe_group != NULL) {
        if (group_try_insert(probe_group, h2, key, value) == 0) {
            group->spreaded = 1;
            
            return;
        }
    }
    
    // нет свободных групп - увеличиваем таблицу
}

int group_try_insert(Group* group, unsigned char h2, char* key, char* value) {
    int slot_idx = get_matching_slot_idx(group, h2, key);
    if (slot_idx != -1) {
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

    if (!group->spreaded) {
        bitmap = get_bitmap(group->control_word, (unsigned char)254); // все удаленные слоты
        if (bitmap != 0) {
            return __builtin_ctz((unsigned int)bitmap);
        }
    }

    return -1;
}

void init_group_control_word(Group* group) {
    __m64 vector = _mm_set1_pi8((unsigned char)128);

    unsigned long value = _m_to_int64(vector);
    group->control_word = value;
}

int get_bitmap(unsigned long control_word, unsigned char mask) {
    __m64 vector = _m_from_int64(control_word);
    __m64 vector_mask = _mm_set1_pi8(mask);
    __m64 cmp = _mm_cmpeq_pi8(vector, vector_mask);

    unsigned long value = _m_to_int64(vector);
    unsigned char bitmap = convert_long_to_bitmap(value);

    return (int)bitmap;
}

void update_control_word_part(Group* group, int slot_idx, unsigned char new_part) {
    unsigned long control_word_new_part = (long)new_part << 8*slot_idx;
    unsigned long mask = 0xFF << 8*slot_idx;
    unsigned long control_word_old_part = mask ^ 0xFFFFFFFFFFFFFFFF;

    group->control_word = control_word_new_part | control_word_old_part;
}

Group* probe_insert_group(Table* table, int start_group_idx) {
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

long hash_string(char* str) {
    return 0;
}