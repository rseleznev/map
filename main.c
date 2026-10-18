#include "map.h"

int main(void) {
    void* map = map_new(10);
    map_set_key(map, "test", "OK");

    return 0;
}