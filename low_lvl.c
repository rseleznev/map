#include "low_lvl.h"
#include <stdlib.h>
#include <mmintrin.h>

// публичные функции
Table* get_matching_table(Map* map, unsigned long hash);
Group* get_matching_group(Table* table, unsigned long h1);
int get_group_idx(Table* table, unsigned long h1);
int get_matching_slot_idx(Group* group, unsigned char h2, char* key);
int get_first_available_slot_idx(Group* group);
int is_empty_slot(Group* group, int slot_idx);
int is_dead_slot(Group* group, int slot_idx);

void init_control_word(Group* group);
void update_control_word(Group* group, int slot_idx, unsigned char new_part);

unsigned long hash_string(unsigned long seed, char* str);

// приватные функции
static int get_bitmap(unsigned long control_word, unsigned char mask);

static char convert_long_to_bitmap(unsigned long l);

/*
==================
ПУБЛИЧНЫЕ ФУНКЦИИ
==================
*/

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

int is_empty_slot(Group* group, int slot_idx) {

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

unsigned long hash_string(unsigned long seed, char* str) {
    unsigned long h = seed ^ 0x9E3779B97F4A7C15UL;

    for (size_t i = 0; i < strlen(str); i++) {
        h ^= (unsigned long)str[i];
        h *= 1099511628211UL;
        h ^= h >> 33;
    }

    return h;
}

/*
==================
ПРИВАТНЫЕ ФУНКЦИИ
==================
*/

static int get_bitmap(unsigned long control_word, unsigned char mask) {
    __m64 vector = _m_from_int64(control_word);
    __m64 vector_mask = _mm_set1_pi8(mask);
    __m64 cmp = _mm_cmpeq_pi8(vector, vector_mask);

    unsigned long value = _m_to_int64(vector);
    unsigned char bitmap = convert_long_to_bitmap(value);

    return (int)bitmap;
}

static char convert_long_to_bitmap(unsigned long l) {
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