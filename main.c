#include <assert.h>
#include <stdio.h>
#include "parse.h"
#include "lex.h"

void dump_tree(struct parse_tree_node* node) {
    if (!node) {
        printf("[nil]");
        return;
    }

    switch (node->op) {
    case COMMA:
        assert (node->left_child);
        assert (node->right_child);
        
        dump_tree(node->left_child);
        printf(",");
        dump_tree(node->right_child);
        break;

    case TYPECAST:
        printf("((%s)", node->text);
        assert(!node->left_child);
        assert(node->right_child);
        dump_tree(node->right_child);
        printf(")");
        break;
    case FUNCTION_CALL:
        assert(node->left_child);
        assert(node->right_child);
        dump_tree(node->left_child);
        printf("(");
        dump_tree(node->right_child);
        printf(")");
        break;

    case LITERAL_OR_ID:
        printf("%s", node->text);
        break;
    default:
        printf("(");
        if (node->left_child) {
            dump_tree(node->left_child);
        }
        printf("%s", token_names[node->op]);
        if (node->right_child) {
            dump_tree(node->right_child);
        }
        printf(")");
        break;
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Error: must supply a single argument\n");
        return 2;
    }
    struct parse_result* result = parse(argv[1]);
    if (result->is_error) {
        printf("Error: %s\n", result->error_message);
    } else {
        dump_tree(result->node);
        printf("\n");
    }

    return result->is_error;
}
