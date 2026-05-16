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
void test_http_client_request_get(void) {
    HTTPResponse *resp = http_request(
        HTTP_GET,
        "example.com",
        "80",
        NULL,
        NULL,
        stdout,
        stderr
    );

    TEST_ASSERT_NOT_NULL(resp);
    if (resp) {
        TEST_ASSERT_EQUAL_INT(200, resp->status_code);
    }
}

// happy path test for https get with TLS
void test_https_client_request_get(void) {
    HTTPResponse *resp = https_request(
        HTTP_GET,
        "example.com",
        "443",
        "/",
        NULL,
        stdout,
        stderr
    );

    TEST_ASSERT_NOT_NULL(resp);
    if (resp) {
        TEST_ASSERT_EQUAL_INT(200, resp->status_code);
    }
}

// implicitly validate response code parsing by querying an invalid endpoint.
// example.com returns a 405 when receivng a POST.
void test_post_to_invalid_endpoint_returns_error_status(void) {
    char *body = "{\"test\":\"data\"}";
    HTTPResponse *resp = http_request(
        HTTP_POST,
        "example.com",
        "80",
        "/post",
        body,
        stdout,
        stderr
    );

    TEST_ASSERT_NOT_NULL(resp);
    if (resp) {
        TEST_ASSERT_EQUAL_INT(405, resp->status_code);
        fprintf(stdout, "post response body: %s", resp->body);
        free(resp->body);
        free(resp);
    }

}

void test_http_client_request_post(void) {
    char *body = "{\"test\": \"data\"}";
    HTTPResponse *resp = http_request(
        HTTP_POST,
        "httpbin.org",
        "80",
        "/post",
        body,
        stdout,
        stderr
    );

    TEST_ASSERT_NOT_NULL(resp);
    if (resp) {
        TEST_ASSERT_EQUAL_INT(200, resp->status_code);
        fprintf(stdout, "post response body: %s", resp->body);
        free(resp->body);
        free(resp);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_http_client_request_get);
    RUN_TEST(test_https_client_request_get);
    RUN_TEST(test_http_client_request_post);
    RUN_TEST(test_post_to_invalid_endpoint_returns_error_status);
    return UNITY_END();
}
