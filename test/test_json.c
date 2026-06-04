#include "json.h"
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"


void setUp(void) {}

void tearDown(void) {}

void test_empty_object(void) {
    Parser p;
    init_parser(&p, "{}", stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JSON_OBJECT, v->type, "the returned value was not of the expected type");
    TEST_ASSERT(v->as.object->capacity == 0);
    TEST_ASSERT(v->as.object->count == 0);
    TEST_ASSERT_NULL(v->as.object->pairs);
    free_json_value(v);
}

void test_empty_array(void) {
    Parser p;
    init_parser(&p, "[]", stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JSON_ARRAY, v->type, "the returned value was not of the expected type");
    TEST_ASSERT(0 == v->as.array->count);
    TEST_ASSERT(0 == v->as.array->capacity);
    TEST_ASSERT_NULL(v->as.array->items);
    free_json_value(v);
}

void test_truncated_object_returns_null(void) {
    char err_buf[256] = {0};
    FILE *err_stream = fmemopen(err_buf, sizeof(err_buf), "w");
    Parser p;
    init_parser(&p, "{", err_stream);
    JsonValue *v = parse_json(&p);
    fclose(err_stream);
    TEST_ASSERT_NULL(v);
    TEST_ASSERT(p.had_error);
    TEST_ASSERT(err_buf[0] != '\0');
}

void test_array_with_multiple_empty_objects(void) {
    Parser p;
    init_parser(&p, "[{},{},{}]", stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT_MESSAGE(JSON_ARRAY, v->type, "the returned value was not of the expected type");
    TEST_ASSERT(3 == v->as.array->count);
    // 8 is the minimum allocated capacity
    TEST_ASSERT(8 == v->as.array->capacity);
    TEST_ASSERT_NOT_NULL(v->as.array->items);
    for (size_t i = 0; i < 3; i++) {
        JsonValue o = v->as.array->items[i];
        TEST_ASSERT_EQUAL_INT_MESSAGE(JSON_OBJECT, o.type, "the returned value was not of the expected type");
        TEST_ASSERT(o.as.object->capacity == 0);
        TEST_ASSERT(o.as.object->count == 0);
        TEST_ASSERT_NULL(o.as.object->pairs);
    }
    free_json_value(v);
}



int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_object);
    RUN_TEST(test_empty_array);
    RUN_TEST(test_truncated_object_returns_null);
    RUN_TEST(test_array_with_multiple_empty_objects);
    UNITY_END();
    return 0;
}
