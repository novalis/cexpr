#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

struct parse_tree_node;

struct parse_tree_node {
    enum token_type op;
    const char* text;
    struct parse_tree_node* first_child;
    struct parse_tree_node* next_sibling;
};

struct parse_result {
    int is_error;
    union {
        struct parse_tree_node* node;
        char* error_message;
    };
};

struct parse_result* parse(const char* string);

char* write_tree_to_string(struct parse_tree_node* node, char* buf);

void free_result_tree(struct parse_result *result);

#endif
