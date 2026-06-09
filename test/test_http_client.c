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
        0,
        NULL,
        stdout,
        stderr
    );

    TEST_ASSERT_NOT_NULL(resp);
    if (resp) {
        TEST_ASSERT_EQUAL_INT(200, resp->status_code);
        free(resp->body);
        free(resp);
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
        0,
        NULL,
        stdout,
        stderr
    );

    TEST_ASSERT_NOT_NULL(resp);
    if (resp) {
        TEST_ASSERT_EQUAL_INT(200, resp->status_code);
        free(resp->body);
        free(resp);
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
        NULL,
        0,
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
    HttpHeader headers[] = {
        { .key = "x-tenant-context", .value = "1337" },
        { .key = "accept", .value = "application/json" },
        { .key = "x-api-key", .value = "deadbeef" },
    };
    char *body = "{\"test\": \"data\"}";
    HTTPResponse *resp = http_request(
        HTTP_POST,
        "httpbin.org",
        "80",
        "/post",
        headers,
        3,
        body,
        stdout,
        stderr
    );

    TEST_ASSERT_NOT_NULL(resp);
    if (resp) {
        TEST_ASSERT_EQUAL_INT(200, resp->status_code);
        // TODO: parse the response headers as HttpHeader structs
        // validate headers present in response body as strings
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(resp->body, "\"x-api-key\": \"deadbeef\""), "x-api-key header not found in response body");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(resp->body, "\"x-tenant-context\": \"1337\""), "x-tenant-context header not found in response body");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(resp->body, "\"test\": \"data\""), "test header not found in response body");
        fprintf(stdout, "post response body: %s", resp->body);
        free(resp->body);
        free(resp);
    }
}

void test_https_client_request_post(void) {
    HttpHeader headers[] = {
        { .key = "x-tenant-context", .value = "1337" },
        { .key = "accept", .value = "application/json" },
        { .key = "x-api-key", .value = "deadbeef" },
    };
    char *body = "{\"test\": \"data\"}";
    HTTPResponse *resp = https_request(
        HTTP_POST,
        "httpbin.org",
        "443",
        "/post",
        headers,
        3,
        body,
        stdout,
        stderr
    );

    TEST_ASSERT_NOT_NULL(resp);
    if (resp) {
        TEST_ASSERT_EQUAL_INT(200, resp->status_code);
        // TODO: parse the response headers as HttpHeader structs
        // validate headers present in response body as strings
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(resp->body, "\"x-api-key\": \"deadbeef\""), "x-api-key header not found in response body");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(resp->body, "\"x-tenant-context\": \"1337\""), "x-tenant-context header not found in response body");
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(resp->body, "\"test\": \"data\""), "test header not found in response body");
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
    RUN_TEST(test_https_client_request_post);
    RUN_TEST(test_post_to_invalid_endpoint_returns_error_status);
    return UNITY_END();
}
