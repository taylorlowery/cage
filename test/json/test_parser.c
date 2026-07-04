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

static void assert_parse_error(const char *input) {
    char err_buf[256] = {0};
    FILE *err_stream = fmemopen(err_buf, sizeof(err_buf), "w");
    Parser p;
    init_parser(&p, input, err_stream);
    JsonValue *v = parse_json(&p);
    fclose(err_stream);
    TEST_ASSERT_NULL_MESSAGE(v, input);
    TEST_ASSERT_TRUE_MESSAGE(p.had_error, input);
}

void test_truncated_inputs_return_null(void) {
    const char *inputs[] = {"{", "[", "\"", "nul", "tru", "fals"};
    for (size_t i = 0; i < sizeof(inputs)/sizeof(inputs[0]); i++) {
        assert_parse_error(inputs[i]);
    }
}

static void assert_parse_success(const char *input) {
    Parser p;
    init_parser(&p, input, stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL_MESSAGE(v, input);
    TEST_ASSERT_FALSE_MESSAGE(p.had_error, input);
    free_json_value(v);
}

void test_single_primitives(void) {
    const char *inputs[] = {"null", "true", "false", "42", "-1", "3.14", "\"hello\""};
    for (size_t i = 0; i < sizeof(inputs)/sizeof(inputs[0]); i++) {
        assert_parse_success(inputs[i]);
    }
}

void test_complex_object(void) {
    const char *json = "{\"name\": \"test\", \"values\": [1, 2.5, -3], \"flags\": {\"a\": true, \"b\": null}}";
    Parser p;
    init_parser(&p, json, stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_OBJECT, v->type);
    TEST_ASSERT(3 == v->as.object->count);
    free_json_value(v);
}

void test_array_of_primitives(void) {
    const char *json = "[1, \"two\", true, null, 3.14]";
    Parser p;
    init_parser(&p, json, stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_ARRAY, v->type);
    TEST_ASSERT(5 == v->as.array->count);
    free_json_value(v);
}

void test_trailing_comma_returns_null(void) {
    assert_parse_error("[1, 2,]");
}

void test_list_invalid_element_returns_null(void) {
    assert_parse_error("[1, invalid, 3]");
}

void test_non_string_key_returns_null(void) {
    const char *inputs[] = {"{42: \"value\"}", "{true: \"value\"}"};
    for (size_t i = 0; i < sizeof(inputs)/sizeof(inputs[0]); i++) {
        assert_parse_error(inputs[i]);
    }
}

void test_list_missing_bracket_returns_null(void) {
    assert_parse_error("[1, 2");
}

void test_object_missing_brace_returns_null(void) {
    assert_parse_error("{\"a\": 1");
}


void test_single_element_array(void) {
    const char *json = "[42]";
    Parser p;
    init_parser(&p, json, stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_ARRAY, v->type);
    TEST_ASSERT(1 == v->as.array->count);
    TEST_ASSERT(JSON_NUMBER == v->as.array->items[0].type);
    free_json_value(v);
}

void test_single_pair_object(void) {
    const char *json = "{\"key\": \"value\"}";
    Parser p;
    init_parser(&p, json, stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_OBJECT, v->type);
    TEST_ASSERT(1 == v->as.object->count);
    free_json_value(v);
}

void test_array_growth(void) {
    const char *json = "[1,2,3,4,5,6,7,8,9]";
    Parser p;
    init_parser(&p, json, stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_ARRAY, v->type);
    TEST_ASSERT(9 == v->as.array->count);
    TEST_ASSERT(v->as.array->capacity >= 9);
    free_json_value(v);
}

void test_object_growth(void) {
    const char *json = "{\"a\":1,\"b\":2,\"c\":3,\"d\":4,\"e\":5,\"f\":6,\"g\":7,\"h\":8,\"i\":9}";
    Parser p;
    init_parser(&p, json, stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_OBJECT, v->type);
    TEST_ASSERT(9 == v->as.object->count);
    TEST_ASSERT(v->as.object->capacity >= 9);
    free_json_value(v);
}

void test_deeply_nested(void) {
    const char *json = "{\"outer\": {\"inner\": [1, 2, 3]}}";
    Parser p;
    init_parser(&p, json, stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_OBJECT, v->type);
    free_json_value(v);
}

void test_empty_and_whitespace_returns_null(void) {
    const char *inputs[] = {"", "   "};
    for (size_t i = 0; i < sizeof(inputs)/sizeof(inputs[0]); i++) {
        assert_parse_error(inputs[i]);
    }
}

void test_extra_data_returns_null(void) {
    const char *inputs[] = {"true false", "{} extra"};
    for (size_t i = 0; i < sizeof(inputs)/sizeof(inputs[0]); i++) {
        assert_parse_error(inputs[i]);
    }
}

void test_json_like_string(void) {
    const char *json = "\"{\\\"nested\\\": true}\"";
    Parser p;
    init_parser(&p, json, stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_STRING, v->type);
    free_json_value(v);
}

void test_anthropic_response(void) {
    const char *json = "{"
        "\"id\": \"msg_01XFDUDYJgAACzvnptvVoYEL\","
        "\"type\": \"message\","
        "\"role\": \"assistant\","
        "\"content\": [{\"type\": \"text\", \"text\": \"Hello!\"}],"
        "\"model\": \"claude-opus-4-8\","
        "\"stop_reason\": \"end_turn\","
        "\"stop_sequence\": null,"
        "\"usage\": {\"input_tokens\": 12, \"output_tokens\": 6}"
    "}";
    Parser p;
    init_parser(&p, json, stdout);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_OBJECT, v->type);
    TEST_ASSERT(8 == v->as.object->count);
    free_json_value(v);
}

void test_string_keys_and_values_have_no_quotes(void) {
    const char *json = "{\"model\":\"claude-haiku-4-5-20251001\",\"id\":\"msg_01Awgi17AdAU3HWie4bCDfQ7\",\"type\":\"message\",\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"Howdy!\"}],\"stop_reason\":\"end_turn\",\"stop_sequence\":null,\"stop_details\":null,\"usage\":{\"input_tokens\":11,\"output_tokens\":24}}";
    Parser p;
    init_parser(&p, json, stderr);
    JsonValue *v = parse_json(&p);
    TEST_ASSERT_NOT_NULL(v);
    TEST_ASSERT_EQUAL_INT(JSON_OBJECT, v->type);
    TEST_ASSERT_EQUAL_INT(9, v->as.object->count);

    TEST_ASSERT_EQUAL_STRING("model", v->as.object->pairs[0].key);
    TEST_ASSERT_EQUAL_STRING("claude-haiku-4-5-20251001", v->as.object->pairs[0].value->as.string);

    TEST_ASSERT_EQUAL_STRING("id", v->as.object->pairs[1].key);
    TEST_ASSERT_EQUAL_STRING("msg_01Awgi17AdAU3HWie4bCDfQ7", v->as.object->pairs[1].value->as.string);

    TEST_ASSERT_EQUAL_STRING("type", v->as.object->pairs[2].key);
    TEST_ASSERT_EQUAL_STRING("message", v->as.object->pairs[2].value->as.string);

    TEST_ASSERT_EQUAL_STRING("role", v->as.object->pairs[3].key);
    TEST_ASSERT_EQUAL_STRING("assistant", v->as.object->pairs[3].value->as.string);

    TEST_ASSERT_EQUAL_STRING("content", v->as.object->pairs[4].key);
    TEST_ASSERT_EQUAL_INT(JSON_ARRAY, v->as.object->pairs[4].value->type);

    JsonObject *content_obj = v->as.object->pairs[4].value->as.array->items[0].as.object;
    TEST_ASSERT_EQUAL_STRING("type", content_obj->pairs[0].key);
    TEST_ASSERT_EQUAL_STRING("text", content_obj->pairs[0].value->as.string);
    TEST_ASSERT_EQUAL_STRING("text", content_obj->pairs[1].key);
    TEST_ASSERT_EQUAL_STRING("Howdy!", content_obj->pairs[1].value->as.string);

    TEST_ASSERT_EQUAL_STRING("stop_reason", v->as.object->pairs[5].key);
    TEST_ASSERT_EQUAL_STRING("end_turn", v->as.object->pairs[5].value->as.string);

    TEST_ASSERT_EQUAL_STRING("stop_sequence", v->as.object->pairs[6].key);
    TEST_ASSERT_EQUAL_INT(JSON_NULL, v->as.object->pairs[6].value->type);

    TEST_ASSERT_EQUAL_STRING("stop_details", v->as.object->pairs[7].key);
    TEST_ASSERT_EQUAL_INT(JSON_NULL, v->as.object->pairs[7].value->type);

    TEST_ASSERT_EQUAL_STRING("usage", v->as.object->pairs[8].key);
    TEST_ASSERT_EQUAL_INT(JSON_OBJECT, v->as.object->pairs[8].value->type);

    free_json_value(v);
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
    RUN_TEST(test_single_primitives);
    RUN_TEST(test_complex_object);
    RUN_TEST(test_array_of_primitives);
    RUN_TEST(test_truncated_inputs_return_null);
    RUN_TEST(test_trailing_comma_returns_null);
    RUN_TEST(test_list_invalid_element_returns_null);
    RUN_TEST(test_non_string_key_returns_null);
    RUN_TEST(test_list_missing_bracket_returns_null);
    RUN_TEST(test_object_missing_brace_returns_null);
    RUN_TEST(test_single_element_array);
    RUN_TEST(test_single_pair_object);
    RUN_TEST(test_array_growth);
    RUN_TEST(test_object_growth);
    RUN_TEST(test_deeply_nested);
    RUN_TEST(test_empty_and_whitespace_returns_null);
    RUN_TEST(test_extra_data_returns_null);
    RUN_TEST(test_json_like_string);
    RUN_TEST(test_anthropic_response);
    RUN_TEST(test_string_keys_and_values_have_no_quotes);
    RUN_TEST(test_array_with_multiple_empty_objects);
    UNITY_END();
    return 0;
}
