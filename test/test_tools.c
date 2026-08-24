#include "tools.h"
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

void test_list_files_returns_directory_entries(void) {
    const char *args = "{\"directory_path\":\"test/fixtures\"}";
    char *listing = NULL;

    int err = list_files(args, &listing);

    TEST_ASSERT_EQUAL_INT(0, err);
    TEST_ASSERT_NOT_NULL(listing);
    TEST_ASSERT_NOT_NULL(strstr(listing, "the_last_hero.txt"));
    TEST_ASSERT_NULL(strstr(listing, "\n.\n"));
    free(listing);
}

void test_edit_file_replaces_unique_text(void) {
    char filepath[] = "test/fixtures/edit_file_XXXXXX";
    int file_descriptor = mkstemp(filepath);
    TEST_ASSERT_TRUE(file_descriptor >= 0);

    FILE *file = fdopen(file_descriptor, "w");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_TRUE(fputs("before\nkeep this line\n", file) >= 0);
    TEST_ASSERT_EQUAL_INT(0, fclose(file));

    char edit_args[1024];
    int edit_args_length = snprintf(
        edit_args, sizeof(edit_args),
        "{\"path\":\"%s\",\"old_str\":\"before\",\"new_str\":\"after\"}",
        filepath);
    TEST_ASSERT_TRUE(edit_args_length > 0);
    TEST_ASSERT_TRUE((size_t)edit_args_length < sizeof(edit_args));

    char *result = NULL;
    int err = edit_file(edit_args, &result);
    TEST_ASSERT_EQUAL_INT(0, err);
    TEST_ASSERT_EQUAL_STRING("OK", result);
    free(result);

    char read_args[1024];
    int read_args_length = snprintf(read_args, sizeof(read_args),
                                    "{\"filepath\":\"%s\"}", filepath);
    TEST_ASSERT_TRUE(read_args_length > 0);
    TEST_ASSERT_TRUE((size_t)read_args_length < sizeof(read_args));

    char *contents = NULL;
    err = read_file(read_args, &contents);
    TEST_ASSERT_EQUAL_INT(0, err);
    TEST_ASSERT_EQUAL_STRING("after\nkeep this line\n", contents);
    free(contents);

    TEST_ASSERT_EQUAL_INT(0, unlink(filepath));
}

void test_edit_file_writes_real_newlines(void) {
    char filepath[] = "test/fixtures/edit_file_XXXXXX";
    int file_descriptor = mkstemp(filepath);
    TEST_ASSERT_TRUE(file_descriptor >= 0);

    FILE *file = fdopen(file_descriptor, "wb");
    TEST_ASSERT_NOT_NULL(file);
    const char *initial_content = "prefix\nmiddle\nsuffix";
    size_t initial_len = strlen(initial_content);
    TEST_ASSERT_EQUAL_INT(initial_len, fwrite(initial_content, 1, initial_len, file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));

    char edit_args[1024];
    int edit_args_length = snprintf(
        edit_args, sizeof(edit_args),
        "{\"path\":\"%s\",\"old_str\":\"middle\",\"new_str\":\"middle\\nextra\"}",
        filepath);
    TEST_ASSERT_TRUE(edit_args_length > 0);
    TEST_ASSERT_TRUE((size_t)edit_args_length < sizeof(edit_args));

    char *result = NULL;
    int err = edit_file(edit_args, &result);
    TEST_ASSERT_EQUAL_INT(0, err);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("OK", result);
    free(result);

    FILE *verify = fopen(filepath, "rb");
    TEST_ASSERT_NOT_NULL(verify);
    char buf[64] = {0};
    size_t bytes_read = fread(buf, 1, sizeof(buf) - 1, verify);
    const char *expected = "prefix\nmiddle\nextra\nsuffix";
    size_t expected_len = strlen(expected);
    TEST_ASSERT_EQUAL_INT(expected_len, (int)bytes_read);
    TEST_ASSERT_EQUAL_INT(0, fclose(verify));

    TEST_ASSERT_EQUAL_MEMORY(expected, buf, expected_len);

    TEST_ASSERT_EQUAL_INT(0, unlink(filepath));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_file_returns_fixture_contents);
    RUN_TEST(test_list_files_returns_directory_entries);
    RUN_TEST(test_edit_file_replaces_unique_text);
    RUN_TEST(test_edit_file_writes_real_newlines);
    return UNITY_END();
}
