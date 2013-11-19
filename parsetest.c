#include "parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct testspec {
    const char* input;
    const char* parenthesized;
};

struct testspec specs[] = {
    {"a", "a"},
    {"(a)", "a"},
    {"a+b", "(a+b)"},
    {"a*b", "(a*b)"},

    {"a*b+c", "((a*b)+c)"},
    {"a+b*c", "(a+(b*c))"},
    {"a*b*c", "((a*b)*c)"},

    {"a=b=c", "(a=(b=c))"},
    {"a?b:c?d:e", "(a?(b:(c?(d:e))))"},

    {"(float)5", "((float)5)"},

    {"*(int *)p += 5", "((*((int *)p))+=5)"},
    {"*(int *)p += -5", "((*((int *)p))+=(-5))"},

    {"*&a", "(*(&a))"},
    {"*a(b)", "(*a(b))"},

    {"*a(-*b,c,(d))", "(*a((-(*b)),c,d))"},
    {"*a(-*b,c,(d))", "(*a((-(*b)),c,d))"},

    {0, 0}
};

int main(int argc, char** argv) {
    int bad = 0;
    for (struct testspec* spec = specs; spec->input; spec++) {
        int len = strlen(spec->input);
        char* buf = malloc(len * 3 + 1);
        struct parse_result* result = parse(spec->input);
        if (result->is_error) {
            printf("Failed to parse %s: %s\n", spec->input, result->error_message);
            continue;
        }
        write_tree_to_string(result->node, buf);
        if (strcmp(buf, spec->parenthesized)) {
            bad++;
            printf("Bad parse of %s: expected %s, got %s\n", spec->input, spec->parenthesized, buf);
        }
        free(buf);
    }
    if (bad) {
        printf ("%d failed tests\n", bad);
        return 1;
    }
    return 0;
}
