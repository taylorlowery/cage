#include "anthropic.h"
#include "json.h"
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_serialize_request_body_single_message(void) {
    char buf[8192];
    AnthropicContent content = {
        .type = ANTHROPIC_CONTENT_TEXT,
        .as.text.text = "Hello, Claude",
    };
    AnthropicMessage message = {
        .role = ANTHROPIC_ROLE_USER,
        .content = &content,
    };
    AnthropicRequest request = {
        .model = "claude-opus-4-8",
        .max_tokens = 1024,
        .messages = &message,
        .message_count = 1,
    };

    size_t cursor = serialize_anthropic_request(buf, sizeof(buf), &request);
    buf[cursor] = '\0';

    TEST_ASSERT_EQUAL_STRING(
        "{\"model\": \"claude-opus-4-8\", \"max_tokens\": 1024, \"messages\": "
        "[{ \"role\": \"user\", \"content\": \"Hello, Claude\" }]}",
        buf);
}

void test_serialize_request_body_multiple_messages(void) {
    char buf[8192];
    AnthropicContent content[4] = {
        {.type = ANTHROPIC_CONTENT_TEXT, .as.text.text = "Only respond in weird grunts"},
        {.type = ANTHROPIC_CONTENT_TEXT, .as.text.text = "Hello, Claude"},
        {.type = ANTHROPIC_CONTENT_TEXT, .as.text.text = "Hrrrmph! Gwaah-krrr! Blorg-flargh!"},
        {.type = ANTHROPIC_CONTENT_TEXT, .as.text.text = "Uuhhhhhh..."},
    };
    AnthropicMessage messages[4] = {
        {.role = ANTHROPIC_ROLE_SYSTEM, .content = &content[0]},
        {.role = ANTHROPIC_ROLE_USER, .content = &content[1]},
        {.role = ANTHROPIC_ROLE_ASSISTANT, .content = &content[2]},
        {.role = ANTHROPIC_ROLE_USER, .content = &content[3]},
    };
    AnthropicRequest request = {
        .model = "claude-opus-4-8",
        .max_tokens = 1024,
        .messages = messages,
        .message_count = 4,
    };

    size_t cursor = serialize_anthropic_request(buf, sizeof(buf), &request);
    buf[cursor] = '\0';

    TEST_ASSERT_EQUAL_STRING(
        "{\"model\": \"claude-opus-4-8\", \"max_tokens\": 1024, \"messages\": "
        "[{ \"role\": \"system\", \"content\": \"Only respond in weird grunts\" }, "
        "{ \"role\": \"user\", \"content\": \"Hello, Claude\" }, "
        "{ \"role\": \"assistant\", \"content\": \"Hrrrmph! Gwaah-krrr! Blorg-flargh!\" }, "
        "{ \"role\": \"user\", \"content\": \"Uuhhhhhh...\" }]}",
        buf);
}

void test_serialize_request_body_tool_use(void) {
    char buf[8192];
    AnthropicContent content = {
        .type = ANTHROPIC_CONTENT_TOOL_USE,
        .as.tool_use = {
            .id = "toolu_123",
            .name = "read_file",
            .input = "{\"path\":\"README.md\"}",
        },
    };
    AnthropicMessage message = {
        .role = ANTHROPIC_ROLE_ASSISTANT,
        .content = &content,
    };
    AnthropicRequest request = {
        .model = "claude-opus-4-8",
        .max_tokens = 1024,
        .messages = &message,
        .message_count = 1,
    };

    size_t cursor = serialize_anthropic_request(buf, sizeof(buf), &request);
    buf[cursor] = '\0';

    TEST_ASSERT_EQUAL_STRING(
        "{\"model\": \"claude-opus-4-8\", \"max_tokens\": 1024, \"messages\": "
        "[{ \"role\": \"assistant\", \"content\": [{\"type\": \"tool_use\", "
        "\"id\": \"toolu_123\", \"name\": \"read_file\", "
        "\"input\": {\"path\":\"README.md\"}}]}",
        buf);
}

void test_serialize_request_body_tool_result(void) {
    char buf[8192];
    AnthropicContent content = {
        .type = ANTHROPIC_CONTENT_TOOL_RESULT,
        .as.tool_result = {
            .tool_use_id = "toolu_123",
            .content = "file contents",
            .is_error = false,
        },
    };
    AnthropicMessage message = {
        .role = ANTHROPIC_ROLE_USER,
        .content = &content,
    };
    AnthropicRequest request = {
        .model = "claude-opus-4-8",
        .max_tokens = 1024,
        .messages = &message,
        .message_count = 1,
    };

    size_t cursor = serialize_anthropic_request(buf, sizeof(buf), &request);
    buf[cursor] = '\0';

    TEST_ASSERT_EQUAL_STRING(
        "{\"model\": \"claude-opus-4-8\", \"max_tokens\": 1024, \"messages\": "
        "[{ \"role\": \"user\", \"content\": [{\"type\": \"tool_result\", "
        "\"tool_use_id\": \"toolu_123\", \"is_error\": false, "
        "\"content\": \"file contents\" }]}]}",
        buf);
}

void test_serialize_request_body_tool_definition(void) {
    char buf[8192];
    AnthropicTool tool = {
        .name = "read_file",
        .description = "Read the contents of a relative file path.",
        .input_schema =
            "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},"
            "\"required\":[\"path\"]}",
    };
    AnthropicRequest request = {
        .model = "claude-opus-4-8",
        .max_tokens = 1024,
        .tools = &tool,
        .tool_count = 1,
    };

    size_t cursor = serialize_anthropic_request(buf, sizeof(buf), &request);
    buf[cursor] = '\0';

    TEST_ASSERT_EQUAL_STRING(
        "{\"model\": \"claude-opus-4-8\", \"max_tokens\": 1024, \"tools\": ["
        "{\"name\":\"read_file\",\"description\":\"Read the contents of a relative file path.\","
        "\"input_schema\": {\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},"
        "\"required\":[\"path\"]}}]}",
        buf);
}

void test_deserialize_text_response(void) {
    const char *json =
        "{\"model\":\"claude-haiku-4-5-20251001\","
        "\"id\":\"msg_01Awgi17AdAU3HWie4bCDfQ7\","
        "\"type\":\"message\",\"role\":\"assistant\","
        "\"content\":[{\"type\":\"text\",\"text\":\"Howdy!\"}],"
        "\"stop_reason\":\"end_turn\",\"stop_sequence\":null,"
        "\"usage\":{\"input_tokens\":11,\"output_tokens\":24}}";
    Parser parser;
    init_parser(&parser, json, stderr);
    JsonValue *parsed = parse_json(&parser);

    TEST_ASSERT_NOT_NULL(parsed);
    AnthropicResponse *response = deserialize_anthropic_response(parsed, stderr);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL_STRING("msg_01Awgi17AdAU3HWie4bCDfQ7", response->id);
    TEST_ASSERT_EQUAL_STRING("end_turn", response->stop_reason);
    TEST_ASSERT_EQUAL_size_t(1, response->content_count);
    TEST_ASSERT_EQUAL_INT(ANTHROPIC_CONTENT_TEXT, response->content[0].type);
    TEST_ASSERT_EQUAL_STRING("Howdy!", response->content[0].as.text.text);

    free_anthropic_response(response);
    free_json_value(parsed);
}

void test_deserialize_tool_use_response(void) {
    const char *json =
        "{\"type\":\"message\",\"role\":\"assistant\","
        "\"content\":[{\"id\":\"toolu_123\",\"type\":\"tool_use\","
        "\"name\":\"read_file\","
        "\"input\":{\"path\":\"README.md\"}}],"
        "\"stop_reason\":\"tool_use\"}";
    Parser parser;
    init_parser(&parser, json, stderr);
    JsonValue *parsed = parse_json(&parser);

    TEST_ASSERT_NOT_NULL(parsed);
    AnthropicResponse *response = deserialize_anthropic_response(parsed, stderr);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL_STRING("tool_use", response->stop_reason);
    TEST_ASSERT_EQUAL_size_t(1, response->content_count);
    TEST_ASSERT_EQUAL_INT(ANTHROPIC_CONTENT_TOOL_USE, response->content[0].type);
    TEST_ASSERT_EQUAL_STRING("toolu_123", response->content[0].as.tool_use.id);
    TEST_ASSERT_EQUAL_STRING("read_file", response->content[0].as.tool_use.name);
    TEST_ASSERT_EQUAL_STRING("{\"path\":\"README.md\"}",
                             response->content[0].as.tool_use.input);

    free_anthropic_response(response);
    free_json_value(parsed);
}

void test_deserialize_tool_result_response(void) {
    const char *json =
        "{\"type\":\"message\",\"role\":\"user\",\"content\":["
        "{\"type\":\"tool_result\",\"tool_use_id\":\"toolu_123\","
        "\"content\":\"file contents\",\"is_error\":false}]}";
    Parser parser;
    init_parser(&parser, json, stderr);
    JsonValue *parsed = parse_json(&parser);

    TEST_ASSERT_NOT_NULL(parsed);
    AnthropicResponse *response = deserialize_anthropic_response(parsed, stderr);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_EQUAL_size_t(1, response->content_count);
    TEST_ASSERT_EQUAL_INT(ANTHROPIC_CONTENT_TOOL_RESULT, response->content[0].type);
    TEST_ASSERT_EQUAL_STRING("toolu_123", response->content[0].as.tool_result.tool_use_id);
    TEST_ASSERT_EQUAL_STRING("file contents", response->content[0].as.tool_result.content);
    TEST_ASSERT_FALSE(response->content[0].as.tool_result.is_error);

    free_anthropic_response(response);
    free_json_value(parsed);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_serialize_request_body_single_message);
    RUN_TEST(test_serialize_request_body_multiple_messages);
    RUN_TEST(test_serialize_request_body_tool_use);
    RUN_TEST(test_serialize_request_body_tool_result);
    RUN_TEST(test_serialize_request_body_tool_definition);
    RUN_TEST(test_deserialize_text_response);
    RUN_TEST(test_deserialize_tool_use_response);
    RUN_TEST(test_deserialize_tool_result_response);
    return UNITY_END();
}
