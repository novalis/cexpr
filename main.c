#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"
#include "lex.h"

static int dump_tree(const char* expr) {

    struct parse_result* result = parse(expr);
    if (result->is_error) {
        printf("Error: %s\n", result->error_message);
    } else {
        char* buf = malloc(strlen(expr) * 3 + 1);
        write_tree_to_string(result->node, buf);
        printf("%s\n", buf);
    }

    return result->is_error;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Error: must supply a single argument\n");
        return 2;
    }
    return dump_tree(argv[1]);
}
