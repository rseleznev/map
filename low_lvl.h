#ifndef MAP_LOW_LVL_H
#define MAP_LOW_LVL_H

#include "models.h"

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

#endif