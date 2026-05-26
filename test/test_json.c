#include "json.h"
#include "vendor/unity/unity.h"
#include "vendor/unity/unity_internals.h"
#include <stdio.h>


void setUp(void) {

}

void tearDown(void) {

}

void assert_token(Scanner *scanner, TokenType expected_tokentype, char *expected_value) {
    Token t = scanToken(scanner);
    TEST_ASSERT_EQUAL(expected_tokentype, t.type);
    if (NULL != expected_value) {
        TEST_ASSERT_EQUAL_STRING_LEN(expected_value, t.start, t.length);
    }
}

void test_lexer_invalid_keyword(void) {
    Scanner s;
    initScanner(&s, "invalid_keyword");
    assert_token(&s, TOKEN_ERROR, "invalid keyword");
}

void test_lexer_null_keyword(void) {
    Scanner s;
    initScanner(&s, "null");
    Token t = scanToken(&s);
    TEST_ASSERT_EQUAL(TOKEN_NULL, t.type);
}

void test_lexer_eof(void) {
    Scanner s;
    initScanner(&s, "null");
    assert_token(&s, TOKEN_NULL, "null");
    assert_token(&s, TOKEN_EOF, NULL);
}

void test_lexer_empty_input(void) {
    Scanner s;
    initScanner(&s, "");
    assert_token(&s, TOKEN_EOF, NULL);
}

void test_lexer_zero(void) {
    Scanner s;
    initScanner(&s, "0");
    assert_token(&s, TOKEN_NUMBER, "0");
}

void test_lexer_positive_int(void) {
    Scanner s;
    initScanner(&s, "1337");
    assert_token(&s, TOKEN_NUMBER, "1337");
}

void test_lexer_negative_int(void) {
    Scanner s;
    initScanner(&s, "-1337");
    assert_token(&s, TOKEN_NUMBER, "-1337");
}

void test_lexer_float(void) {
    Scanner s;
    initScanner(&s, "13.37");
    assert_token(&s, TOKEN_NUMBER, "13.37");
}

void test_lexer_negative_float(void) {
    Scanner s;
    initScanner(&s, "-13.37");
    assert_token(&s, TOKEN_NUMBER, "-13.37");
}

void test_lexer_invalid_decimals(void) {
    Scanner s;
    initScanner(&s, "13.37.5");
    assert_token(&s, TOKEN_ERROR, NULL);
}

void test_lexer_escaped_quote(void) {
    Scanner s;
    initScanner(&s, "\"Howdy, \"Pilgrim!\"");
    assert_token(&s, TOKEN_STRING, "\"Howdy, \"Pilgrim!\"");
}

void test_lexer_escaped_backslash(void) {
    Scanner s;
    initScanner(&s, "\"Howdy, \\Pilgrim!\"");
    assert_token(&s, TOKEN_STRING, "\"Howdy, \\Pilgrim!\"");
}

void test_lexer_unterminated_string(void) {
    Scanner s;
    initScanner(&s, "\"howdy");
    assert_token(&s, TOKEN_ERROR, "unterminated string");
}


void test_lexer_minimal_json(void) {
    Scanner s;
    initScanner(&s, "{\"key\": \"value\"}");
    assert_token(&s, TOKEN_LEFTBRACE, "{");
    assert_token(&s, TOKEN_STRING, "\"key\"");
    assert_token(&s, TOKEN_COLON, ":");
    assert_token(&s, TOKEN_STRING, "\"value\"");
    assert_token(&s, TOKEN_RIGHTBRACE, "}");
    assert_token(&s, TOKEN_EOF, NULL);
}


int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_lexer_empty_input);
    RUN_TEST(test_lexer_null_keyword);
    RUN_TEST(test_lexer_invalid_keyword);
    RUN_TEST(test_lexer_eof);
    RUN_TEST(test_lexer_zero);
    RUN_TEST(test_lexer_positive_int);
    RUN_TEST(test_lexer_negative_int);
    RUN_TEST(test_lexer_float);
    RUN_TEST(test_lexer_negative_float);
    RUN_TEST(test_lexer_invalid_decimals);
    RUN_TEST(test_lexer_escaped_quote);
    RUN_TEST(test_lexer_escaped_backslash);
    RUN_TEST(test_lexer_unterminated_string);
    RUN_TEST(test_lexer_minimal_json);
    UNITY_END();
    return 0;
}
