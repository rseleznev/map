#ifndef MAP_H
#define MAP_H

void* map_new(char* key_type, char* value_type, int len);
void map_set(void* map_ptr, char* key, char* value);

#endif