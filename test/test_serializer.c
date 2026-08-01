#include "json.h"
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_serialize_null(void) {
    JsonValue value = {.type = JSON_NULL};
    char *serialized = json_value_to_string(&value);

    TEST_ASSERT_NOT_NULL(serialized);
    TEST_ASSERT_EQUAL_STRING("null", serialized);
    free(serialized);
}

void test_serialize_booleans(void) {
    JsonValue true_value = {.type = JSON_BOOL, .as.boolean = true};
    JsonValue false_value = {.type = JSON_BOOL, .as.boolean = false};
    char *serialized_true = json_value_to_string(&true_value);
    char *serialized_false = json_value_to_string(&false_value);

    TEST_ASSERT_NOT_NULL(serialized_true);
    TEST_ASSERT_NOT_NULL(serialized_false);
    TEST_ASSERT_EQUAL_STRING("true", serialized_true);
    TEST_ASSERT_EQUAL_STRING("false", serialized_false);

    free(serialized_true);
    free(serialized_false);
}

void test_serialize_number(void) {
    JsonValue value = {.type = JSON_NUMBER, .as.number = 42.5};
    char *serialized = json_value_to_string(&value);

    TEST_ASSERT_NOT_NULL(serialized);
    TEST_ASSERT_EQUAL_STRING("42.5", serialized);
    free(serialized);
}

void test_serialize_escaped_string(void) {
    JsonValue value = {
        .type = JSON_STRING,
        .as.string = "say \"hello\"\\next\nline\tindented\rreturn",
    };
    char *serialized = json_value_to_string(&value);

    TEST_ASSERT_NOT_NULL(serialized);
    TEST_ASSERT_EQUAL_STRING(
        "\"say \\\"hello\\\"\\\\next\\nline\\tindented\\rreturn\"", serialized);
    free(serialized);
}

void test_serialize_array(void) {
    JsonValue items[3] = {
        {.type = JSON_NULL},
        {.type = JSON_BOOL, .as.boolean = true},
        {.type = JSON_STRING, .as.string = "hello"},
    };
    JsonArray array = {.items = items, .count = 3, .capacity = 3};
    JsonValue value = {.type = JSON_ARRAY, .as.array = &array};
    char *serialized = json_value_to_string(&value);

    TEST_ASSERT_NOT_NULL(serialized);
    TEST_ASSERT_EQUAL_STRING("[null,true,\"hello\"]", serialized);
    free(serialized);
}

void test_serialize_object(void) {
    JsonValue message = {.type = JSON_STRING, .as.string = "hello"};
    JsonValue count = {.type = JSON_NUMBER, .as.number = 2};
    JsonPair pairs[2] = {
        {.key = "message", .value = &message},
        {.key = "count", .value = &count},
    };
    JsonObject object = {.pairs = pairs, .count = 2, .capacity = 2};
    JsonValue value = {.type = JSON_OBJECT, .as.object = &object};
    char *serialized = json_value_to_string(&value);

    TEST_ASSERT_NOT_NULL(serialized);
    TEST_ASSERT_EQUAL_STRING("{\"message\":\"hello\",\"count\":2}", serialized);
    free(serialized);
}

void test_serialize_nested_value(void) {
    JsonValue enabled = {.type = JSON_BOOL, .as.boolean = true};
    JsonValue name = {.type = JSON_STRING, .as.string = "Cage"};
    JsonPair object_pairs[2] = {
        {.key = "enabled", .value = &enabled},
        {.key = "name", .value = &name},
    };
    JsonObject object = {.pairs = object_pairs, .count = 2, .capacity = 2};
    JsonValue object_value = {.type = JSON_OBJECT, .as.object = &object};

    JsonValue items[2] = {
        {.type = JSON_NUMBER, .as.number = 7},
        object_value,
    };
    JsonArray array = {.items = items, .count = 2, .capacity = 2};
    JsonValue value = {.type = JSON_ARRAY, .as.array = &array};
    char *serialized = json_value_to_string(&value);

    TEST_ASSERT_NOT_NULL(serialized);
    TEST_ASSERT_EQUAL_STRING("[7,{\"enabled\":true,\"name\":\"Cage\"}]", serialized);
    free(serialized);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_serialize_null);
    RUN_TEST(test_serialize_booleans);
    RUN_TEST(test_serialize_number);
    RUN_TEST(test_serialize_escaped_string);
    RUN_TEST(test_serialize_array);
    RUN_TEST(test_serialize_object);
    RUN_TEST(test_serialize_nested_value);
    return UNITY_END();
}
