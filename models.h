#ifndef MAP_MODELS_H
#define MAP_MODELS_H

typedef struct Slot {
    void* key;
    void* value;
} Slot;

typedef struct Group {
    Slot slots[8];
    unsigned long control_word;
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

#endif