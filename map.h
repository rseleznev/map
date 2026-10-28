#ifndef MAP_H
#define MAP_H

void* map_new(int len);
int map_len(void* map_ptr);
void map_set_key(void* map_ptr, char* key, char* value);
int map_has_key(void* map_ptr, char* key);
void* map_get_key(void* map_prt, char* key);
void map_delete_key(void* map_prt, char* key);

#endif