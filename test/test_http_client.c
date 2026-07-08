#include "http_client.h"
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
        free_http_response(resp);
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
        free_http_response(resp);
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
        free_http_response(resp);
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
        TEST_ASSERT_NOT_NULL_MESSAGE(strcasestr(resp->body, "\"x-api-key\": \"deadbeef\""), "x-api-key header not found in response body");
        TEST_ASSERT_NOT_NULL_MESSAGE(strcasestr(resp->body, "\"x-tenant-context\": \"1337\""), "x-tenant-context header not found in response body");
        TEST_ASSERT_NOT_NULL_MESSAGE(strcasestr(resp->body, "\"test\": \"data\""), "test header not found in response body");
        free_http_response(resp);
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
        TEST_ASSERT_NOT_NULL_MESSAGE(strcasestr(resp->body, "\"x-api-key\": \"deadbeef\""), "x-api-key header not found in response body");
        TEST_ASSERT_NOT_NULL_MESSAGE(strcasestr(resp->body, "\"x-tenant-context\": \"1337\""), "x-tenant-context header not found in response body");
        TEST_ASSERT_NOT_NULL_MESSAGE(strcasestr(resp->body, "\"test\": \"data\""), "test header not found in response body");
        free_http_response(resp);
    }
}

// Test that parse_response_body_to_http_response correctly parses
// a raw HTTP response with headers and body.
void test_parse_response_body_parses_headers_and_body(void) {
    const char *raw_response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "X-Request-Id: abc-123\r\n"
        "X-Custom: hello-world\r\n"
        "\r\n"
        "{\"ok\":true}";

    HTTPResponse *resp = parse_response_body_to_http_response(raw_response, stderr);

    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(200, resp->status_code);
    TEST_ASSERT_EQUAL_size_t(3, resp->header_count);

    TEST_ASSERT_EQUAL_STRING("Content-Type", resp->headers[0].key);
    TEST_ASSERT_EQUAL_STRING("application/json", resp->headers[0].value);

    TEST_ASSERT_EQUAL_STRING("X-Request-Id", resp->headers[1].key);
    TEST_ASSERT_EQUAL_STRING("abc-123", resp->headers[1].value);

    TEST_ASSERT_EQUAL_STRING("X-Custom", resp->headers[2].key);
    TEST_ASSERT_EQUAL_STRING("hello-world", resp->headers[2].value);

    TEST_ASSERT_NOT_NULL(resp->body);
    TEST_ASSERT_EQUAL_STRING("{\"ok\":true}", resp->body);

    free_http_response(resp);
}

// Test that parse_response_body_to_http_response correctly handles
// more headers than the initial capacity (8), triggering realloc.
void test_parse_response_body_header_resizing(void) {
    const char *raw_response =
        "HTTP/1.1 200 OK\r\n"
        "X-Header-01: val01\r\n"
        "X-Header-02: val02\r\n"
        "X-Header-03: val03\r\n"
        "X-Header-04: val04\r\n"
        "X-Header-05: val05\r\n"
        "X-Header-06: val06\r\n"
        "X-Header-07: val07\r\n"
        "X-Header-08: val08\r\n"
        "X-Header-09: val09\r\n"
        "X-Header-10: val10\r\n"
        "\r\n"
        "done";

    HTTPResponse *resp = parse_response_body_to_http_response(raw_response, stderr);

    TEST_ASSERT_NOT_NULL(resp);
    TEST_ASSERT_EQUAL_INT(200, resp->status_code);
    TEST_ASSERT_EQUAL_size_t(10, resp->header_count);

    // Verify headers survived the realloc
    TEST_ASSERT_EQUAL_STRING("X-Header-01", resp->headers[0].key);
    TEST_ASSERT_EQUAL_STRING("val01", resp->headers[0].value);

    TEST_ASSERT_EQUAL_STRING("X-Header-05", resp->headers[4].key);
    TEST_ASSERT_EQUAL_STRING("val05", resp->headers[4].value);

    TEST_ASSERT_EQUAL_STRING("X-Header-10", resp->headers[9].key);
    TEST_ASSERT_EQUAL_STRING("val10", resp->headers[9].value);

    TEST_ASSERT_NOT_NULL(resp->body);
    TEST_ASSERT_EQUAL_STRING("done", resp->body);

    free_http_response(resp);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_response_body_parses_headers_and_body);
    RUN_TEST(test_parse_response_body_header_resizing);
    RUN_TEST(test_http_client_request_get);
    RUN_TEST(test_https_client_request_get);
    RUN_TEST(test_http_client_request_post);
    RUN_TEST(test_https_client_request_post);
    RUN_TEST(test_post_to_invalid_endpoint_returns_error_status);
    return UNITY_END();
}
