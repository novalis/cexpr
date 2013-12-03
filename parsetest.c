#include "parse.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct testspec {
    const char* input;
    const char* output;
};

struct testspec specs[] = {
    {"a", "a"},
    {"(a)", "a"},
    {"a+b", "(a+b)"},
    {"a*b", "(a*b)"},

    {"a*b+c", "((a*b)+c)"},
    {"a+b*c", "(a+(b*c))"},
    {"a*b*c", "((a*b)*c)"},

    {"a,b,c", "((a,b),c)"},

    {"a=b=c", "(a=(b=c))"},
    {"a?b:c?d:e", "(a?(b:(c?(d:e))))"},

    {"(float)5", "((float)5)"},

    {"*(int *)p += 5", "(*(((int *)p))+=5)"},
    {"*(int *)p += -5", "(*(((int *)p))+=-(5))"},

    {"*&a", "*(&(a))"},
    {"*f(b)", "*(f(b))"},

    {"f()", "f()"},

    {"*a(-*b,c,(d))", "*(a(-(*(b)),c,d))"},
    {"*a(-*b,c,(d))", "*(a(-(*(b)),c,d))"},

    {"a+sizeof(unsigned int*)", "(a+sizeof(unsigned int *))"},
    {"a+sizeof b", "(a+sizeof(b))"},

    {"t[(int)b]", "t[((int)b)]"},

    {"t.a.b", "((t.a).b)"},
    {"t->a.b->c", "(((t->a).b)->c)"},

    {"*a+5", "(*(a)+5)"},
    {"*t.a", "*((t.a))"},

    {"(*t).a", "(*(t).a)"},

    {"++ a", "++(a)"},
    {"-- a", "--(a)"},
    {"a++", "(a++)"},
    {"a--", "(a--)"},

    {"--- a", "--(-(a))"},

    {"~a", "~(a)"},
    {"p = 0x17.fp1", "(p=0x17.fp1)"},
    {"p = .1", "(p=.1)"},
    {0, 0}
};

struct testspec expected_failures[] = {
    {"\001", "Bogus token '\001'"},
    {"(mumble\001", "Bogus token '\001'"},
    {")", "Unexpected )"},
    {"(a", "Missing ) parsing parenthesized expression"},
    {"(unsigned int *", "Missing ) in (assumed) typecast"},
    {"--(unsigned int *", "Missing ) in (assumed) typecast"},
    {"sizeof(int", "Missing ) in sizeof"},
    {"(unsigned int *)a)", "Unexpected ) at end of input"},
    {"a[", "Missing ] at end of input"},
    {"a[5", "Missing ]"},
    {"a[5,", "Unexpected eof"},
    {"f(,", "Missing ) while parsing function call"},
    {"((", "Unexpected eof"},
    {"a.*", "Expected identifier before * token"},
    {"-&a.*", "Expected identifier before * token"},
    {"(int)\001", "Bogus token '\001'"},
    {"a?b:c?d", "Missing : in ?: ternary op (found eof)"},
    {"a?b:c?d*", "Unexpected eof"},
    {"a=*", "Unexpected eof"},
    {"a 1", "Unexpected literal"},
    {"(foo)a", "Unexpected literal"},
    {"b)", "Unexpected ) at end of input"},
    {"baz((foo)a)", "Unexpected literal while parsing function call"},
    {"", "Empty expression"},
    {0, 0}
};

char* typenames[] = {"foo", "charmander"};
int test_typenames() {
    int bad = 0;
    char* test = "(charmander)a";
    struct parse_result* result = parse(test, 0);
    if (!result->is_error) {
        int len = strlen(test);
        char* buf = malloc(len * 3 + 1);
        write_tree_to_string(result->node, buf);
        printf("Failed to fail parsing %s: %s\n", test, buf);
        bad++;
        free(buf);
    }
    free_parse_result_contents(result);
    free(result);

    result = parse(test, typenames);
    if (result->is_error) {
        printf("Failed to parse typecast to typedef with hint: %s", result->error_message);
        bad++;
    }
    free_parse_result_contents(result);
    free(result);

    return bad;
    
}

int test_parse_failures() {
    int bad = 0;
    for (struct testspec* spec = expected_failures; spec->input; spec++) {
        struct parse_result* result = parse(spec->input, 0);
        if (result->is_error) {
            if (strcmp(result->error_message, spec->output)) {
                printf("Wrong error message parsing %s:\n"
                       "expected: '%s'\n"
                       "got:      '%s'\n",
                       spec->input,
                       spec->output,
                       result->error_message);
                bad ++;
            }
        } else {
            int len = strlen(spec->input);
            char* buf = malloc(len * 3 + 1);
            write_tree_to_string(result->node, buf);
            printf("Failed to fail parsing %s: %s\n", spec->input, buf);
            bad++;
            free(buf);
        }
        free_parse_result_contents(result);
        free(result);
    }
    return bad;
}

int main() {
    int bad = 0;

    for (struct testspec* spec = specs; spec->input; spec++) {
        int len = strlen(spec->input);
        char* buf = malloc(len * 3 + 1);
        struct parse_result* result = parse(spec->input, 0);
        if (result->is_error) {
            printf("Failed to parse %s: %s\n", spec->input, result->error_message);
            goto end_of_loop;
        }
        write_tree_to_string(result->node, buf);
        if (strcmp(buf, spec->output)) {
            bad++;
            printf("Bad parse of %s: expected %s, got %s\n", spec->input, spec->output, buf);
        }
    end_of_loop:
        free_parse_result_contents(result);
        free(result);
        free(buf);
        continue;
    }

    bad += test_parse_failures();
    if (bad) {
        printf ("%d failed tests\n", bad);
        return 1;
    }
    return 0;
}
