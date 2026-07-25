#include "anthropic.h"
#include "agent.h"
#include "provider.h"

int main(void) {
    AnthropicContext *ctx = create_anthropic_context(NULL, NULL);
    InferenceProvider p = {.provider_context = ctx,
                           .complete_inference = anthropic_complete_inference,
                           .destroy_provider_context = free_anthropic_context};

    Agent *agent = new_agent("Cagey", &p, stdin, stdout, stderr);

    run(agent);
}
