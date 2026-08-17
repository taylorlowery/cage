#include "anthropic.h"
#include "agent.h"
#include "provider.h"
#include "tools.h"

int main(void) {
    AnthropicContext *ctx = create_anthropic_context(NULL, NULL);
    if (NULL == ctx) {
        fprintf(stderr, "Can't create an agent with a null provider context!\nSaiyonara.\n");
        return -1;
    }

    InferenceProvider p = {.provider_context = ctx,
                           .complete_inference = anthropic_complete_inference,
                           .destroy_provider_context = free_anthropic_context};
    ToolSet tools = get_default_tool_set();

    Agent *agent = new_agent("Cagey", &p, &tools, stdin, stdout, stderr);
    if (NULL == agent) {
        free_anthropic_context(ctx);
        return -1;
    }

    run(agent);
    free_agent(agent);

    return 0;
}
