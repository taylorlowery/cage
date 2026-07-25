#include "agent.h"
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"

// Unity fixtures
void setUp(void) {
}
void tearDown(void) {
}

void test_run(void) {
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_run);
    return UNITY_END();
}
