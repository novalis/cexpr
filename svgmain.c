#include "svg.h"
#include "parse.h"
#include <stdio.h>
#include <stdlib.h>


static int dump_tree(const char* expr) {

    struct parse_result* result = parse(expr);
    if (result->is_error) {
        printf("Error: %s\n", result->error_message);
    } else {
        char* svg = parse_tree_to_svg(result->node);
        printf("%s", svg);
        free(svg);
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

