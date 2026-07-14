#include "anthropic.h"
#include "json.h"
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"


void setUp(void) {}

void tearDown(void) {}

void test_serialize_request_body_single_message(void) {
  char buf[8192];

  char *expected = "{\"model\": \"claude-opus-4-8\", \"max_tokens\": 1024, \"messages\": [{ \"role\": \"user\", \"content\": \"Hello, Claude\" }]}";

  AnthropicMessage message = {
    .role = ROLE_USER,
    .content = "Hello, Claude"
  };

  AnthropicRequest request = {
    .headers = NULL,
    .model = "claude-opus-4-8",
    .max_tokens = 1024,
    .messages = &message,
    .message_count = 1,
    .system = NULL,
    .tools = NULL,
    .tool_count = 0,
  };

  size_t cursor = serialize_request_body(buf, 8192, &request);
  buf[cursor] = '\0';
  TEST_ASSERT_EQUAL_STRING(expected, buf);
}


void test_serialize_request_body_multiple_messages(void) {
  char buf[8192];

  char *expected = "{\"model\": \"claude-opus-4-8\", \"max_tokens\": 1024, \"messages\": [{ \"role\": \"system\", \"content\": \"Only respond in weird grunts\" }, { \"role\": \"user\", \"content\": \"Hello, Claude\" }, { \"role\": \"assistant\", \"content\": \"Hrrrmph! Gwaah-krrr! Blorg-flargh!\" }, { \"role\": \"user\", \"content\": \"Uuhhhhhh...\" }]}";

  AnthropicMessage message[4] = {
    {
      .role = ROLE_SYSTEM,
      .content = "Only respond in weird grunts"
    },
    {
      .role = ROLE_USER,
      .content = "Hello, Claude"
    },
    {
      .role = ROLE_ASSISTANT,
      .content = "Hrrrmph! Gwaah-krrr! Blorg-flargh!"
    },
    {
      .role = ROLE_USER,
      .content = "Uuhhhhhh..."
    }
  };

  AnthropicRequest request = {
    .headers = NULL,
    .model = "claude-opus-4-8",
    .max_tokens = 1024,
    .messages = &message[0],
    .message_count = 4,
    .system = NULL,
    .tools = NULL,
    .tool_count = 0,
  };

  size_t cursor = serialize_request_body(buf, 8192, &request);
  buf[cursor] = '\0';
  TEST_ASSERT_EQUAL_STRING(expected, buf);
}

void test_deserialize_json(void) {
    const char *json = "{\"model\":\"claude-haiku-4-5-20251001\",\"id\":\"msg_01Awgi17AdAU3HWie4bCDfQ7\",\"type\":\"message\",\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"Howdy! \xf0\x9f\x91\x8b How's it going? What can I help you with today?\"}],\"stop_reason\":\"end_turn\",\"stop_sequence\":null,\"stop_details\":null,\"usage\":{\"input_tokens\":11,\"cache_creation_input_tokens\":0,\"cache_read_input_tokens\":0,\"cache_creation\":{\"ephemeral_5m_input_tokens\":0,\"ephemeral_1h_input_tokens\":0},\"output_tokens\":24,\"service_tier\":\"standard\",\"inference_geo\":\"not_available\"}}";
    Parser p;
    init_parser(&p, json, stderr);
    JsonValue *json_parsed = parse_json(&p);

    TEST_ASSERT_NOT_NULL_MESSAGE(json_parsed, "parse_json returned NULL");
    TEST_ASSERT_EQUAL_INT_MESSAGE(JSON_OBJECT, json_parsed->type, "parsed value is not a JSON object");
    TEST_ASSERT_EQUAL_INT_MESSAGE(9, json_parsed->as.object->count, "expected 9 top-level fields");

    AnthropicResponse *resp = deserialize_response(json_parsed, stderr);
    TEST_ASSERT_NOT_NULL_MESSAGE(resp, "deserialize_response returned NULL");
    TEST_ASSERT_EQUAL_STRING("msg_01Awgi17AdAU3HWie4bCDfQ7", resp->id);
    TEST_ASSERT_EQUAL_STRING("message", resp->type);
    TEST_ASSERT_EQUAL_STRING("assistant", resp->role);
    TEST_ASSERT_EQUAL_STRING("claude-haiku-4-5-20251001", resp->model);
    TEST_ASSERT_EQUAL_STRING("end_turn", resp->stop_reason);

    TEST_ASSERT_NOT_NULL_MESSAGE(resp->content, "resp->content is NULL - content field not processed");
    TEST_ASSERT_EQUAL_STRING("text", resp->content[0].type);
    TEST_ASSERT_EQUAL_STRING("Howdy! \xf0\x9f\x91\x8b How's it going? What can I help you with today?", resp->content[0].text);

    TEST_ASSERT_EQUAL_size_t(11, resp->usage.input_tokens);
    TEST_ASSERT_EQUAL_size_t(24, resp->usage.output_tokens);

    TEST_ASSERT_NULL(resp->stop_sequence);

    TEST_ASSERT_EQUAL(1, resp->content_count);

    free(resp->content);
    free(resp);
    free_json_value(json_parsed);
}

void test_run(void) {
  Run();
}


int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_serialize_request_body_single_message);
    RUN_TEST(test_serialize_request_body_multiple_messages);
    RUN_TEST(test_deserialize_json);
    RUN_TEST(test_run);
    UNITY_END();
    return 0;
}
