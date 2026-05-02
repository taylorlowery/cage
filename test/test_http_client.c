#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"

void setUp(void) {

}

void tearDown(void) {

}

// happy path test http client post
void test_post(void) {
    char *body = "{\"test\": \"data\"}";
    HTTPResponse *resp = post(
        HTTP_GET,
        "example.com",
        "80",
        NULL,
        body,
        strlen(body),
        stdout,
        stderr
    );

    TEST_ASSERT_NOT_NULL(resp);
    if (resp) {
        TEST_ASSERT_EQUAL_INT(200, resp->status_code);
        fprintf(stdout, "body: %s\n", resp->body);
        free(resp->body);
        free(resp);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_post);
    return UNITY_END();
}
