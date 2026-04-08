#include "agent.h"
#include <stdio.h>
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"

// Unity fixtures
void setUp(void) {}
void tearDown(void) {}

// validate that my greet() function
// prints the expected greeting to a given file pointer.
void test_greet(void) {
    char buffer[100];
    FILE *fp = fmemopen(buffer, 100, "w+");
    if (NULL == fp) {
        fprintf(stderr, "Failed to open memory buffer for fmemopen\n");
        return;
    }

    greet(fp);
    fflush(fp);
    rewind(fp);

    char validationBuf[100];
    if (fgets(validationBuf, sizeof(validationBuf), fp) == NULL) {
        TEST_FAIL_MESSAGE("failed to read from buffer");
    }
    fclose(fp);
    TEST_ASSERT_EQUAL_STRING("Howdy, Pilgrim!", validationBuf);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_greet);
    return UNITY_END();
}
