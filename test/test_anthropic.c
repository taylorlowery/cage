#include "anthropic.h"
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
    NULL,
    "claude-opus-4-8",
    1024,
    &message,
    1  
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
    NULL,
    "claude-opus-4-8",
    1024,
    &message[0],
    4
  };

  size_t cursor = serialize_request_body(buf, 8192, &request);
  buf[cursor] = '\0';
  TEST_ASSERT_EQUAL_STRING(expected, buf);
}
int main(void) {
    UNITY_BEGIN();

    // add tests here
    RUN_TEST(test_serialize_request_body_single_message);
    RUN_TEST(test_serialize_request_body_multiple_messages);
    
    UNITY_END();
    return 0;
}
