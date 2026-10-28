#include "map.h"
#include "models.h"
#include "low_lvl.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

void* map_new(int len);
int map_len(void* map_ptr);
void map_set_key(void* map_ptr, char* key, char* value);
int map_has_key(void* map_ptr, char* key);
void* map_get_key(void* map_prt, char* key);
void map_delete_key(void* map_prt, char* key);

int table_try_insert(Map* map, Table* table, unsigned long h1, unsigned long h2, char* key, char* value);
int group_try_insert(Group* group, unsigned char h2, char* key, char* value);

Map* init_small_map(void);
void free_small_map(Map* map);
Map* init_big_map(int groups_amount);
Map* map_evolve(Map* small_map);
void table_grow(Map* map, Table** old_table);
void table_split(Map* map, Table* table);
void table_copy(Map* map, Table* from, Table* to);

Group* init_group(void);
Table* init_table(int groups_amount);

int probe_group_for_insert(Table* table, int start_group_idx, unsigned h2, char* key, char* value);

int incr_idx_looped(int start_idx, int incr_amount, int loop_len);

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
        if (table_try_insert(map, table, h1, h2, key, value) == 0) {
            return;
        }
        
        // нет свободных групп - увеличиваем таблицу
        table_grow(map, &table);
    }
}

int table_try_insert(Map* map, Table* table, unsigned long h1, unsigned long h2, char* key, char* value) {
    if (table->free_slots_len <= 0) {
        table_grow(map, &table);
    }

    Group* group = get_matching_group(table, h1);

    // пробуем вставить в данную группу
    if (group_try_insert(group, h2, key, value) == 0) {
        map->entries_len++;
        table->free_slots_len--;
        
        return 0;
    }

    // в данной группе нет места - пробируем след группы
    if (probe_group_for_insert(table, get_group_idx(table, h1), h2, key, value) == 0) {
        map->entries_len++;
        table->free_slots_len--;
        
        return 0;
    }

    return -1;
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

        return;
    }

    Table* table = init_table(groups_amount);
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

void table_grow(Map* map, Table** old_table) {
    if ((*old_table)->len == 1024) {
        table_split(map, old_table);

        return;
    }

    int new_groups_amount = (*old_table)->len * 2;
    Table* new_table = init_table(new_groups_amount);
    table_copy(map, old_table, new_table);
    *old_table = new_table;
}

void table_split(Map* map, Table* table) {
    if (table->local_depth < map->global_depth) {
        // создаем 2 новые таблицы
        // вставляем ссылки на них в уже существующие указатели в директории
    }
    else {
        // создаем новые таблицы
        // расширяем массив указателей в директории
        // в новых указателях ставим ссылки на старые таблицы
        // для новых таблиц в директории указываем соответствующие указатели
    }
}

void table_copy(Map* map, Table* from, Table* to) {
    for (int i = 0; i < from->len; i++) {
        
        Group* group = from->groups[i];
        for (int j = 0; j < 8; j++) {
            if (is_empty_slot(group, j) || is_dead_slot(group, j)) {
                continue;
            }
            
            char* key = group->slots[j].key;
            char* value = group->slots[j].key;

            unsigned long hash = hash_string(map->hash_seed, key);
            unsigned long h1 = (hash >> 7) & 0xFFFFFFFFFFFFFFFF;
            unsigned char h2 = (char)hash & (0xFF >> 1);

            if (table_try_insert(map, to, h1, h2, key, value) == 0) {
                continue;
            }

            // нет свободных групп - увеличиваем таблицу назначения
            table_grow(map, &to);
            j--; // откатываем итератор, чтобы повторно пройтись по этому элементу  
        }
        free(group);
    }
    free(from->groups);
    free(from);
}

Group* init_group(void) {
    Group* group = calloc(1, sizeof(Group));
    init_group_control_word(group);

    return group;
}

Table* init_table(int groups_amount) {
    Group** groups_arr = malloc(sizeof(Group) * groups_amount);

    for (int group_idx = 0; group_idx < groups_amount; group_idx++) {
        Group* group = init_group();
        groups_arr[group_idx] = group;
    }
    
    Table* table = malloc(sizeof(Table));
    table->len = groups_amount;
    table->local_depth = 0;
    table->free_slots_len = groups_amount * 7;
    table->groups = groups_arr;

    return table;
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