#define _POSIX_C_SOURCE 200809L

#include "parse.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

struct parse_state {
    lex_buf *buf;
    struct token push_back;
};

static struct parse_result* parse_comma(struct parse_state *state);

static struct token get_next_parse_token(struct parse_state* state) {
    struct token ret;
    if (state->push_back.token_type) {
        ret = state->push_back;
        state->push_back.token_type = 0;
    } else {
        ret = get_next_token(state->buf);
    }
    return ret;
}

static void push_back(struct parse_state* state,
    struct token tok) {
    if (state->push_back.token_type) {
        printf("Programmer error: pushed back multiple tokens\n");
        exit(1);
    }
    state->push_back = tok;
}

static struct parse_state make_parse_state(lex_buf* buf) {
    return (struct parse_state) {.buf = buf, .push_back = {.token_type = 0}};
}

static int match(struct token token, enum token_type expected_type) {
    return token.token_type == expected_type;
}

static struct parse_result* error(const char* message, ...) {
    struct parse_result *result = malloc(sizeof(struct parse_result));
    //compute size
    va_list args;
    va_start (args, message);
    int size = vsnprintf(0, 0, message, args);
    va_end (args);
    char* errbuf = malloc(size + 1);
    va_start (args, message);
    vsnprintf(errbuf, size + 1, message, args);
    va_end (args);

    result->is_error = 1;
    result->error_message = errbuf;
    return result;
}
static void free_tree_node(struct parse_tree_node *node) {
    if (node->text) {
        free((void*)node->text);
        node->text = 0;
    }
    if(node->left_child) {
        free_tree_node(node->left_child);
        node->left_child = 0;
    }
    if(node->right_child) {
        free_tree_node(node->right_child);
        node->right_child = 0;
    }
}

static void free_result_tree(struct parse_result *result) {
    if (result->is_error) {
        free(result->error_message);
        return;
    }
    free_tree_node(result->node);
}

static struct parse_tree_node* make_node(enum token_type op, 
                                             struct parse_tree_node* left, 
                                             struct parse_tree_node* right) {

    struct parse_tree_node* node = calloc(1, sizeof(struct parse_tree_node));

    node->op = op;
    node->left_child = left;
    node->right_child = right;
    return node;
}

static struct parse_result* make_node_result(enum token_type op, 
                                             struct parse_tree_node* left, 
                                             struct parse_tree_node* right) {

    struct parse_result* result = malloc(sizeof(struct parse_result *));

    result->is_error = 0;
    result->node = make_node(op, left, right);
    return result;
}

static struct parse_tree_node* make_terminal_node(struct token token) {
    struct parse_tree_node* node = malloc(sizeof(struct parse_tree_node *));

    node->op = token.token_type;
    node->text = token.token_value;
    return node;
}

static struct parse_result* make_terminal_node_result(struct token token) {
    struct parse_result* result = malloc(sizeof(struct parse_result *));

    result->is_error = 0;
    result->node = make_terminal_node(token);
    return result;
}

static int is_assignop(struct token token) {
    switch (token.token_type) {
    case ASSIGN:
    case PERCENT_EQUAL:
    case CARET_EQUAL:
    case AMPERSAND_EQUAL:
    case BAR_EQUAL:
    case STAR_EQUAL:
    case MINUS_EQUAL:
    case PLUS_EQUAL:
    case SLASH_EQUAL:
    case LEFT_SHIFT_EQUAL:
    case RIGHT_SHIFT_EQUAL:
        return 1;
    default:
        return 0;
    }
}

static int is_unop(struct token token) {
    switch(token.token_type) {
    case AMPERSAND:
    case STAR:
    case PLUS:
    case MINUS:
    case BANG:
    case TILDE:
    case DOUBLE_PLUS:
    case DOUBLE_MINUS:
    case SIZEOF:
        return 1;
    default:
        return 0;
    }
}

static enum token_type binops[][5] = {
    {DOUBLE_BAR, 0},
    {DOUBLE_AMPERSAND, 0},
    {BAR, 0},
    {CARET, 0},

    {AMPERSAND, 0},
    {IS_EQUAL, BANG_EQUAL, 0},
    {LT, GT, LTE, GTE, 0},
    {LEFT_SHIFT, RIGHT_SHIFT, 0},

    {PLUS, MINUS, 0},
    {STAR, SLASH, PERCENT, 0},
    {0}
};

/* HACK: assume parenthesized expressions starting
 * with one of these are type casts
 */
static const char* type_name_starters[] = {
    "bool",
    "char",
    "double",
    "float",
    "int",
    "long",
    "off_t",
    "ptrdiff_t",
    "signed",
    "short",
    "size_t",
    "time_t",
    "unsigned",
    0
};

static int is_type_word(struct token token) {
    if (token.token_type != LITERAL_OR_ID) {
        return 0;
    }
    for (const char** type = type_name_starters; *type; ++type) {
        if (strcmp(*type, token.token_value) == 0) {
            return 1;
        }
    }
    return 0;
}

static char* append_token(char* existing, struct token token) {
    const char* token_str;
    if (token.token_value) {
        token_str = token.token_value;
    } else {
        token_str = token_names[token.token_type];
    }
    if (!existing) {
        return strdup(token_str);
    }
    int token_str_len = strlen(token_str);
    int existing_len = strlen(existing);
    char* new_str = realloc(existing, existing_len + token_str_len + 2);
    new_str[existing_len] = ' ';
    strcpy(new_str + existing_len + 1, token_str);
    return new_str;

}

static enum token_type closer(enum token_type opener) {
    if (opener == OPEN_PAREN) {
        return CLOSE_PAREN;
    } else if (opener == OPEN_BRACKET) {
        return CLOSE_BRACKET;
    } else {
        return -1;
    }
}

/* () [] . -> expr++ expr--	 */
static struct parse_result* parse_primary_expression(struct parse_state *state) {
    struct token tok = get_next_parse_token(state);
    struct parse_result* result = 0;
    switch(tok.token_type) {
    case OPEN_PAREN:
        result = parse_comma(state);
        tok = get_next_parse_token(state);
        if (tok.token_type != CLOSE_PAREN) {
            free_result_tree(result);
            return error("Missing ) parsing parenthesized expression");
        }
        break;
    case LITERAL_OR_ID:
        result = make_terminal_node_result(tok);
        break;
    default:
        result = error("Unable to parse primary expression (token type is %s)", token_names[tok.token_type]);
        return result;
    }
    
    bool more = true;
    while (more) {
        tok = get_next_parse_token(state);
        switch(tok.token_type) {
        case OPEN_PAREN:
        case OPEN_BRACKET:
        {
            struct parse_result* right_result = parse_comma(state);
            enum token_type close = closer(tok.token_type);
            tok = get_next_parse_token(state);
            if (tok.token_type != close) {
                free_result_tree(result);
                free_result_tree(right_result);
                return error("Missing %s", token_names[close]);
            }

            struct parse_tree_node* old_node = result->node;
            free(result);
            if (right_result->is_error) {
                free_tree_node(old_node);
                return right_result;
            }
            enum token_type op = close == CLOSE_PAREN ? FUNCTION_CALL : SUBSCRIPT;
            result = make_node_result(op, old_node, right_result->node);
            free(right_result);
            break;
        }
        case ARROW:
        case DOT:
        {
            enum token_type op = tok.token_type;
            tok = get_next_parse_token(state);
            if (tok.token_type != LITERAL_OR_ID) {
                free_result_tree(result);
                return error("expected identifier before '%s' token", token_names[tok.token_type]);
            }
            struct parse_tree_node* old_node = result->node;
            free(result);
            result = make_node_result(op, old_node, make_terminal_node(tok));
            break;
        }
        case DOUBLE_PLUS:
        case DOUBLE_MINUS:
        {
            struct parse_tree_node* old_node = result->node;
            free(result);
            result = make_node_result(tok.token_type, old_node, 0);
            break;
        }

        default:
            //not part of a primary expression
            more = false;
            push_back(state, tok);
            break;
        }
    }
    return result;
}

static char* parse_parenthesized_typename(struct token tok, struct parse_state* state) {
    //parse until close-paren
    char* typename = 0;
    while (tok.token_type != CLOSE_PAREN) {
        if (tok.token_type == END_OF_EXPRESSION) {
            return 0;
        }
        typename = append_token(typename, tok);
        tok = get_next_parse_token(state);
    }
    return typename;
}

/* 
   * & + - ! ~ ++expr --expr (typecast) sizeof
 */
static struct parse_result* parse_unop(struct parse_state *state) {

    struct parse_result* result;

    struct token tok = get_next_parse_token(state);
    if (tok.token_type == SIZEOF) {
        tok = get_next_parse_token(state);
        if (tok.token_type == OPEN_PAREN) {
            //sizeof(typename)
            tok = get_next_parse_token(state);
            char* typename = parse_parenthesized_typename(tok, state);
            if (!typename) {
                return error("Found EOF when parsing (assumed) typecast");
            }
            result = make_node_result(SIZEOF, 0, 0);
            result->node->text = typename;
        } else {
            //sizeof var
            result = make_node_result(SIZEOF, 0, 0);
            result->node->text = strdup(tok.token_value);
        }

    } else if (is_unop(tok)) {
        struct parse_result* right_result = parse_unop(state);
        if (right_result->is_error) {
            return right_result;
        }
        struct parse_tree_node* right_node = right_result->node;
        free(right_result);
        result = make_node_result(tok.token_type, 0, right_node);
    } else if (tok.token_type == OPEN_PAREN) {
        //handle typecasts via hack
        tok = get_next_parse_token(state);
        if (is_type_word(tok)) {
            char* typename = parse_parenthesized_typename(tok, state);
            if (!typename) {
                return error("Found EOF when parsing (assumed) typecast");
            }
            
            struct parse_result* right_result = parse_unop(state);
            if (right_result->is_error) {
                return right_result;
            }
            struct parse_tree_node* right_node = right_result->node;
            free(right_result);
            result = make_node_result(TYPECAST, 0, right_node);
            result->node->text = typename;
        } else {
            push_back(state, tok);
            //handle parenthesized expression
            result = parse_comma(state);
            if (result->is_error) {
                return result;
            }
            tok = get_next_parse_token(state);
            if (tok.token_type != CLOSE_PAREN) {
                free_result_tree(result);
                return error("Missing ) parsing parenthesized expression");
            }
        }
    } else {
        push_back(state, tok);
        result = parse_primary_expression(state);
    }

    return result;
}

static int is_level_binop(struct token token, int level) {
    int i = 0;
    while (binops[level][i]) {
        if (token.token_type == binops[level][i++]) {
            return 1;
        }
    }
    return 0;
}

/*
 * Binary operations are all parsed the same way, but they're divided
 * into a bunch of different precedence levels.
 */
static struct parse_result* parse_binop(struct parse_state *state, int level) {
    struct parse_result* result;
    if (binops[level][0] == 0) {
        result = parse_unop(state);
        return result;
    } else {
        result = parse_binop(state, level + 1);
        if (result->is_error) {
            return result;
        }
    }
    while(1) {
        struct token tok = get_next_parse_token(state);
        if (tok.token_type == END_OF_EXPRESSION) {
            return result;
        }
        else if (tok.token_type == CLOSE_PAREN || 
                 tok.token_type == CLOSE_BRACKET) {
            push_back(state, tok);
            return result;
        }
        if (!is_level_binop(tok, level)) {
            push_back(state, tok);
            return result;
        }
        struct parse_result *right_result;
        right_result = parse_binop(state, level + 1);

        if (right_result->is_error) {
            return right_result;
        }
        struct parse_tree_node* node = result->node;
        free(result);
        result = make_node_result(tok.token_type, node, right_result->node);
        free(right_result);
    }
}

static struct parse_result* parse_ternop(struct parse_state *state) {
    struct parse_result *left_result = parse_binop(state, 0);
    if (left_result->is_error) {
        return left_result;
    }

    struct token tok = get_next_parse_token(state);
    if (tok.token_type != QUESTION) {
        push_back(state, tok);
        return left_result;
    }

    struct parse_result *mid_result = parse_ternop(state);
    if (mid_result->is_error) {
        free_result_tree(left_result);
        return mid_result;
    }

    tok = get_next_parse_token(state);
    if (tok.token_type != COLON) {
        free_result_tree(left_result);
        free_result_tree(mid_result);
        struct parse_result *result = 
            error("Missing : in ?: ternary op (found %s)", 
                  token_names[tok.token_type]);
        return result;
    }
    struct parse_result *right_result = parse_ternop(state);
    if (right_result->is_error) {
        free_result_tree(left_result);
        free_result_tree(mid_result);
        return right_result;
    }
    
    //result is a new ? node with left-side result
    //right-side a new : node with mid, right
    //we hope

    struct parse_tree_node *left_node = left_result->node;
    struct parse_tree_node *mid_node = mid_result->node;
    struct parse_tree_node *right_node = right_result->node;
    free(left_result);
    free(mid_result);
    free(right_result);

    struct parse_tree_node *new_right = make_node(COLON, mid_node, right_node);
    struct parse_result* result = make_node_result(QUESTION, left_node, new_right);
    return result;
}

static struct parse_result* parse_assignop(struct parse_state *state) {
    struct parse_result *result = parse_ternop(state);
    if (result->is_error) 
        return result;

    struct token tok = get_next_parse_token(state);
    if (!is_assignop(tok)) {
        push_back(state, tok);
        return result;
    }

    struct parse_result *right_result = parse_assignop(state);
    if (right_result->is_error) {
        free_result_tree(result);
        return right_result;
    }

    struct parse_tree_node *left_node = result->node;
    free(result);
    result = make_node_result(tok.token_type, left_node, right_result->node);
    free(right_result);
    return result;
}

static struct parse_result* parse_comma(struct parse_state *state) {
    struct parse_result *result = parse_assignop(state);
    if (result->is_error) 
        return result;
    while (1) {
        struct token tok = get_next_parse_token(state);
        if (tok.token_type == END_OF_EXPRESSION ||
            tok.token_type == CLOSE_PAREN || 
            tok.token_type == CLOSE_BRACKET) {
            push_back(state, tok);
            return result;
        }
        if (!match(tok, COMMA)) {
            free_result_tree(result);
            return error("Expected comma, got %s", token_names[tok.token_type]);
        }
        struct parse_result *right_result = parse_assignop(state);
        if (right_result->is_error) {
            free_result_tree(result);
            return right_result;
        }
        result = make_node_result(COMMA, result->node, right_result->node);
    }
}

struct parse_result* parse(const char* string) {
    lex_buf lex_buf = start_lex(string);
    struct parse_state state = make_parse_state(&lex_buf);
    struct parse_result* result = parse_comma(&state);
    struct token tok = get_next_parse_token(&state);
    if (tok.token_type != END_OF_EXPRESSION) {
        free_result_tree(result);
        return error("Unparsed portion of expression starts with %s", 
                     token_names[tok.token_type]);
    }
    return result;
}

/*
  Returns a pointer to the new end of the string.  Assumes
  that the buffer has enough space for the string.
 */
char* write_tree_to_string(struct parse_tree_node* node, char* buf) {
    switch (node->op) {
    case COMMA:
        assert (node->left_child);
        assert (node->right_child);
        
        buf = write_tree_to_string(node->left_child, buf);
        buf += sprintf(buf, ",");
        buf = write_tree_to_string(node->right_child, buf);
        break;

    case TYPECAST:
        buf += sprintf(buf, "((%s)", node->text);
        assert(!node->left_child);
        assert(node->right_child);
        buf = write_tree_to_string(node->right_child, buf);
        buf += sprintf(buf, ")");
        break;
    case FUNCTION_CALL:
        assert(node->left_child);
        assert(node->right_child);
        buf = write_tree_to_string(node->left_child, buf);
        buf += sprintf(buf, "(");
        buf = write_tree_to_string(node->right_child, buf);
        buf += sprintf(buf, ")");
        break;
    case SIZEOF:
        buf += sprintf(buf, "sizeof(%s)", node->text);
        break;
    case LITERAL_OR_ID:
        buf += sprintf(buf, "%s", node->text);
        break;
    default:
        buf += sprintf(buf, "(");
        if (node->left_child) {
            buf = write_tree_to_string(node->left_child, buf);
        }
        buf += sprintf(buf, "%s", token_names[node->op]);
        if (node->right_child) {
            buf = write_tree_to_string(node->right_child, buf);
        }
        buf += sprintf(buf, ")");
        break;
    }
    return buf;
}
