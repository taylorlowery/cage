#include "tools.h"
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

void test_read_file_returns_fixture_contents(void) {
    const char *args =
        "{\"filepath\":\"test/fixtures/the_last_hero.txt\"}";
    char *contents = NULL;

    int err = read_file(args, &contents);

    TEST_ASSERT_EQUAL_INT(0, err);
    TEST_ASSERT_NOT_NULL(contents);
    TEST_ASSERT_NOT_NULL(strstr(contents, "The wind blew out from Bergen"));
    TEST_ASSERT_NOT_NULL(strstr(contents, "You never loved your friends, my friends"));
    TEST_ASSERT_NOT_NULL(strstr(contents, "You never laughed in all your life as I shall laugh in death."));
    TEST_ASSERT_NOT_NULL(strstr(contents, "G. K. Chesterton"));

    free(contents);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_file_returns_fixture_contents);
    return UNITY_END();
}
