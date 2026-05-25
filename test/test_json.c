#include "json.h"
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"
#include <stdio.h>


void setUp(void) {

}

void tearDown(void) {

}

void test_compile(void) {
    fprintf(stdout, "eyyyy lmao\n");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_compile);
    UNITY_END();
    return 0;
}
