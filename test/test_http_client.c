#include "http_client.h"
#include <stdio.h>
#include <stdlib.h>
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"

// happy path test http client post
void test_post() {
    HTTPResponse *resp = post("http://www.example.com");

    free(resp);
}
