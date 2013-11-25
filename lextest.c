#include "lex.h"
#include <stdio.h>
#include <string.h>

struct lex_test {
    char* text;
    enum token_type types[6];
};

static struct lex_test tests[] = {
    {"", {END_OF_EXPRESSION}},
    {"*", {STAR, END_OF_EXPRESSION}},
    {"*=", {STAR_EQUAL, END_OF_EXPRESSION}},
    {"0x17", {LITERAL_OR_ID, END_OF_EXPRESSION}},
    {"0x17.fp1", {LITERAL_OR_ID, END_OF_EXPRESSION}},
    {"071", {LITERAL_OR_ID, END_OF_EXPRESSION}},
    {"09", {BOGUS, END_OF_EXPRESSION}},
    {"morx*=17", {LITERAL_OR_ID, STAR_EQUAL, LITERAL_OR_ID, END_OF_EXPRESSION}},
    {"sizeof int", {SIZEOF, LITERAL_OR_ID, END_OF_EXPRESSION}},
    {"sizeof_int", {LITERAL_OR_ID, END_OF_EXPRESSION}},
    {"sizeo sizeof", {LITERAL_OR_ID, SIZEOF, END_OF_EXPRESSION}},
    {"sizeof", {SIZEOF, END_OF_EXPRESSION}},
    {"a>>3", {LITERAL_OR_ID, RIGHT_SHIFT, LITERAL_OR_ID, END_OF_EXPRESSION}},
    {"a>>=3", {LITERAL_OR_ID, RIGHT_SHIFT_EQUAL, LITERAL_OR_ID, END_OF_EXPRESSION}},
    {"a->r", {LITERAL_OR_ID, ARROW, LITERAL_OR_ID, END_OF_EXPRESSION}},
    {"a\001", {LITERAL_OR_ID, BOGUS, END_OF_EXPRESSION}},
    {"\"a\\001\"", {LITERAL_OR_ID, END_OF_EXPRESSION}},
    {"\"a\"/*b*/+c", {LITERAL_OR_ID, PLUS, LITERAL_OR_ID, END_OF_EXPRESSION}},
    {"a.b", {LITERAL_OR_ID, DOT, LITERAL_OR_ID, END_OF_EXPRESSION}},
    {"(a0.b)", {OPEN_PAREN, LITERAL_OR_ID, DOT, LITERAL_OR_ID, CLOSE_PAREN, END_OF_EXPRESSION}},
    {0, {0}}
};

static int assert_token(const char* test, lex_buf* buf,
                        enum token_type expected_type,
                        const char* expected_value) {

    struct token token = get_next_token(buf);
    if (token.token_type != expected_type) {
        printf("Literal test %s failed: expected type %s but got %s\n", 
               test, token_names[expected_type], token_names[token.token_type]);
        return 1;
    }
    if (expected_value) {
        if (strcmp(expected_value, token.token_value) != 0) {
            printf("Literal test failed: expected value %s but got %s\n", 
                   expected_value, token.token_value);
            return 1;
        }
    } else if (token.token_value) {
            printf("Literal test failed: unexpected value %s\n", 
                   token.token_value);
            return 1;
    }

    return 0;
}

static int test_literals() {
    lex_buf buf;
    int failures = 0;

    const char* teststr = "\"a\001\\x\\\"\"";
    buf = start_lex(teststr);

    failures += assert_token(teststr, &buf, LITERAL_OR_ID, "\"a\001\\x\\\"\"");
    failures += assert_token(teststr, &buf, END_OF_EXPRESSION, 0);

    teststr = "\"a\001\\x\\\"\"*";
    buf = start_lex(teststr);

    failures += assert_token(teststr, &buf, LITERAL_OR_ID, "\"a\001\\x\\\"\"");
    failures += assert_token(teststr, &buf, STAR, 0);
    failures += assert_token(teststr, &buf, END_OF_EXPRESSION, 0);

    teststr = "\"a\001\\x\\\"\"*''";
    buf = start_lex(teststr);

    failures += assert_token(teststr, &buf, LITERAL_OR_ID, "\"a\001\\x\\\"\"");
    failures += assert_token(teststr, &buf, STAR, 0);
    failures += assert_token(teststr, &buf, LITERAL_OR_ID, "''");
    failures += assert_token(teststr, &buf, END_OF_EXPRESSION, 0);

    teststr = "\"a\001\\x\\\"\"*'\\''";
    buf = start_lex(teststr);

    failures += assert_token(teststr, &buf, LITERAL_OR_ID, "\"a\001\\x\\\"\"");
    failures += assert_token(teststr, &buf, STAR, 0);
    failures += assert_token(teststr, &buf, LITERAL_OR_ID, "'\\''");
    failures += assert_token(teststr, &buf, END_OF_EXPRESSION, 0);

    return failures;
}

int main() {
    int i = 0;
    int failures = test_literals();
    while (tests[i].text) {
        struct lex_test test = tests[i];
        lex_buf buf = start_lex(test.text);
        struct token token;
        int token_no = 0;
        while (1) {
            token = get_next_token(&buf);
            if (token.token_type != test.types[token_no]) {
                printf("Test failed: expected %s but got %s in token %d of test %s\n", 
                       token_names[test.types[token_no]],
                       token_names[token.token_type],
                       token_no,
                       test.text);
                ++failures;
                break;
                       
            }
            token_no += 1;
            if (token.token_type == END_OF_EXPRESSION) {
                break;
            }
        }
        ++i;
    }
    return failures > 0 ? 1 : 0;
}
