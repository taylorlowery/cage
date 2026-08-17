#include "agent.h"
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

static int greet(const char *name, char **out) {
    if (NULL == name || NULL == out) {
        return -1;
    }
    *out = NULL;
    // add one space for \0
    int to_be_written = snprintf(NULL, 0, "Howdy, %s!", name);
    if (to_be_written < 0) {
        return -1;
    }
    size_t required = (size_t)to_be_written + 1;
    *out = calloc(required, sizeof(char));
    if (NULL == *out) {
        return -1;
    }

    int actual_written = snprintf(*out, required, "Howdy, %s!", name);
    if (actual_written < 0 || actual_written != to_be_written) {
        free(*out);
        *out = NULL;
        return -1;
    }
    return 0;
}

void test_greet_tool(void) {
    char *output = NULL;
    TEST_ASSERT_EQUAL_INT(0, greet("Taylor", &output));
    TEST_ASSERT_EQUAL_STRING("Howdy, Taylor!", output);
}

void test_agent_receives_tool_set(void) {
    Tool tool = {
        .name = "greet",
        .description = "Greet a person by name.",
        .input_schema = "{\"type\":\"object\"}",
        .handler = greet,
    };
    ToolSet tools = {
        .tools = &tool,
        .tool_count = 1,
    };

    Agent *agent = new_agent("Test Agent", NULL, &tools, stdin, stdout, stderr);

    TEST_ASSERT_NOT_NULL(agent);
    TEST_ASSERT_EQUAL_PTR(&tools, agent->tools);
    TEST_ASSERT_NOT_NULL(agent->tools->tools[0].handler);

    free_agent(agent);
}

void test_call_tool_dispatch(void) {
    Tool tool = {
        .name = "greet",
        .description = "Greet a person by name.",
        .input_schema = "{\"type\":\"object\"}",
        .handler = greet,
    };
    ToolSet tools = {
        .tools = &tool,
        .tool_count = 1,
    };

    Agent *agent = new_agent("Test Agent", NULL, &tools, stdin, stdout, stderr);
    char *output = NULL;
    int err = call_tool(agent, "greet", "Tater", &output);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL_STRING("Howdy, Tater!", output);

    free_agent(agent);
}

typedef struct {
    size_t call_count;
} FakeProviderState;

static void fake_complete_inference(void *context, const Conversation *conversation,
                                    const ToolSet *tools, InferenceResponse *out) {
    FakeProviderState *state = context;
    TEST_ASSERT_NOT_NULL(tools);
    TEST_ASSERT_EQUAL_size_t(1, tools->tool_count);

    if (0 == state->call_count) {
        TEST_ASSERT_EQUAL_size_t(1, conversation->message_count);
        TEST_ASSERT_EQUAL(USER, conversation->messages[0].role);

        out->content_blocks = calloc(1, sizeof(Content));
        TEST_ASSERT_NOT_NULL(out->content_blocks);
        out->content_count = 1;
        out->content_blocks[0].type = TOOL_CALL;
        out->content_blocks[0].as.tool_call.id = strdup("toolu_test");
        out->content_blocks[0].as.tool_call.name = strdup("greet");
        out->content_blocks[0].as.tool_call.input = strdup("Taylor");
        out->stop_reason = strdup("tool_use");
    } else if (1 == state->call_count) {
        TEST_ASSERT_EQUAL_size_t(3, conversation->message_count);
        TEST_ASSERT_EQUAL(ASSISTANT, conversation->messages[1].role);
        TEST_ASSERT_EQUAL(TOOL_CALL,
                          conversation->messages[1].content_blocks[0].type);
        TEST_ASSERT_EQUAL_STRING("toolu_test",
                                 conversation->messages[1].content_blocks[0].as.tool_call.id);
        TEST_ASSERT_EQUAL(USER, conversation->messages[2].role);
        TEST_ASSERT_EQUAL(TOOL_RESULT,
                          conversation->messages[2].content_blocks[0].type);
        TEST_ASSERT_EQUAL_STRING("toolu_test",
                                 conversation->messages[2].content_blocks[0].as.tool_result.tool_use_id);
        TEST_ASSERT_EQUAL_STRING("Howdy, Taylor!",
                                 conversation->messages[2].content_blocks[0].as.tool_result.content);

        out->content_blocks = calloc(1, sizeof(Content));
        TEST_ASSERT_NOT_NULL(out->content_blocks);
        out->content_count = 1;
        out->content_blocks[0].type = TEXT;
        out->content_blocks[0].as.text.text = strdup("The greeting is complete.");
        out->stop_reason = strdup("end_turn");
    } else {
        TEST_FAIL_MESSAGE("fake provider called more than twice");
    }

    state->call_count++;
}

void test_run_tool_loop(void) {
    char input_data[] = "Please greet Taylor.\n";
    FILE *input = fmemopen(input_data, strlen(input_data), "r");
    FILE *output = tmpfile();
    FILE *errors = tmpfile();
    TEST_ASSERT_NOT_NULL(input);
    TEST_ASSERT_NOT_NULL(output);
    TEST_ASSERT_NOT_NULL(errors);

    Tool tool = {
        .name = "greet",
        .description = "Greet a person by name.",
        .input_schema = "{\"type\":\"object\"}",
        .handler = greet,
    };
    ToolSet tools = {
        .tools = &tool,
        .tool_count = 1,
    };
    FakeProviderState state = {0};
    InferenceProvider provider = {
        .provider_context = &state,
        .complete_inference = fake_complete_inference,
        .destroy_provider_context = NULL,
    };
    Agent *agent = new_agent("Test Agent", &provider, &tools, input, output, errors);
    TEST_ASSERT_NOT_NULL(agent);

    run(agent);

    TEST_ASSERT_EQUAL_size_t(2, state.call_count);
    char output_data[1024] = {0};
    rewind(output);
    size_t output_size = fread(output_data, 1, sizeof(output_data) - 1, output);
    output_data[output_size] = '\0';
    TEST_ASSERT_NOT_NULL(strstr(output_data, "The greeting is complete."));

    free_agent(agent);
    fclose(input);
    fclose(output);
    fclose(errors);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_greet_tool);
    RUN_TEST(test_agent_receives_tool_set);
    RUN_TEST(test_call_tool_dispatch);
    RUN_TEST(test_run_tool_loop);
    return UNITY_END();
}
