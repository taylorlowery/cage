#include "agent.h"
#include <stdio.h>
#include "vendor/unity/unity.h"

// validate that my greet() function
// prints the expected greeting to a given file pointer.
void test_greet(void) {
    char buffer[100];
    FILE *fp = fmemopen(&buffer, 100, "r");
    if (NULL == fp) {
        fprintf(stderr, "Failed to open memory buffer for fmemopen\n");
        return;
    }

    greet(fp);
    fflush(fp);
    rewind(fp);

    char validationBuf[100];
    fgets(validationBuf, sizeof(validationBuf), fp);
    TEST_ASSERT_EQUAL_STRING("Howdy, Pilgrim!", validationBuf);
}
