#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

struct parse_tree_node;

struct parse_tree_node {
    enum token_type op;
    const char* text;
    struct parse_tree_node* left_child;
    struct parse_tree_node* right_child;
};

struct parse_result {
    int is_error;
    union {
        struct parse_tree_node* node;
        char* error_message;
    };
};

struct parse_result* parse(const char* string);

#endif
