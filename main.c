#include "map.h"
#include <stdio.h>

int main(void) {
    void* map = map_new(10);
    map_set_key(map, "test", "OK");

    if (map_has_key(map, "test")) {
        printf("map has key 'test' \n");
    }

    return 0;
}