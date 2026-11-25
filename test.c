#include "low_lvl.h"
#include "models.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct ControlWordTest {
    Group* group;
    int slot_idx;
    unsigned char new_part;

    unsigned long expected_result;
} ControlWordTest;

int test_update_control_word();
void print_binary(unsigned long n);

int main(void) {
    int res = 0;
    res = test_update_control_word();
    if (res != 0) {
        printf("TEST: test_update_control_word failed \n");
        return 1;
    }
    printf("TEST: test_update_control_word success \n");

    return 0;
}

int test_update_control_word() {
    ControlWordTest* tests = malloc(sizeof(ControlWordTest) * 1);
    int tests_amount = 1;

    // success
    tests[0].group = calloc(1, sizeof(Group));
    init_control_word(tests[0].group);
    tests[0].slot_idx = 1;
    tests[0].new_part = (unsigned char)85;
    tests[0].expected_result = 9247438699275255936UL;

    for (int i = 0; i < tests_amount; i++) {
        ControlWordTest test = tests[i];
        Group* group = test.group;
        
        update_control_word(group, test.slot_idx, test.new_part);
        if (group->control_word != test.expected_result) {
            printf("TEST: test_update_control_word: expected %lu, got %lu \n", test.expected_result, group->control_word);
            // print_binary(group->control_word);
            print_binary(18374967954648334335UL);
            return -1;
        }
    }

    return 0;
}

void print_binary(unsigned long n) {
    printf("------------ \n");
    printf("print_binary: ");
    int bits_count = 0;
    for (int p = 63; p >= 0; p--) {
        if ((n >> p) & 1 == 1) {
            printf("1");
        }
        else {
            printf("0");
        }
        bits_count++;

        if (bits_count == 8) {
            printf(" ");
            bits_count = 0;
        }
    }
    printf("\n");
    printf("------------ \n");
}