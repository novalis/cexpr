#define _POSIX_C_SOURCE 200809L

#include "parse.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "obstack_helper.h"

#define PARSE_PUSHBACK_BUF_SIZE 2

#define HANDLE_BOGUS_TOKEN(tok)                                 \
    {{                                                          \
        struct token __tok = (tok);                             \
        if (__tok.token_type == BOGUS) {                        \
            error(state, "Bogus token '%s'", __tok.token_value);\
            return 0;                                           \
        }                                                       \
    }}                                                          \

struct parse_state {
    lex_buf *buf;
    struct token push_back[PARSE_PUSHBACK_BUF_SIZE];
    char* error_message;
    struct obstack* obstack;
    char** typename_starters;
};

/*
   These are C's built-in types (or at least the first words thereof)
 */
static char* typename_starters[] = {
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
    "struct",
    "time_t",
    "unsigned",
    0
};

static struct parse_tree_node* parse_comma(struct parse_state *state);
static struct parse_tree_node* parse_assignop(struct parse_state *state);

static struct token get_next_parse_token(struct parse_state* state) {

    for (int i = PARSE_PUSHBACK_BUF_SIZE - 1; i >= 0; --i) {
        if (state->push_back[i].token_type) {
            struct token ret = state->push_back[i];
            state->push_back[i].token_type = 0;
            return ret;
        }
    }

    return get_next_token(state->buf);

}

static void push_back(struct parse_state* state,
    struct token tok) {
    if (state->push_back[PARSE_PUSHBACK_BUF_SIZE-1].token_type) {
        printf("Programmer error: pushed back too many tokens\n");
        exit(1);
    }
    for (int i = 0; i < PARSE_PUSHBACK_BUF_SIZE; ++i) {
        if (state->push_back[i].token_type == 0) {
            state->push_back[i] = tok;
            break;
        }
    }
}

static int count_typenames(char** typenames) {
    if (typenames == 0) {
        return 0;
    }
    int i = 0;
    for (char** name = typenames; *name; ++name) {
        ++i;
    }
    return i;
}

static void copy_typenames(char** dest, char** src) {
    if (!src) {
        return;
    }
    while ((*dest++ = *src++)) {}
}

static struct parse_state make_parse_state(lex_buf* buf, char** typenames) {
    struct parse_state state = {.buf = buf, .error_message = 0};
    for (int i = 0; i < PARSE_PUSHBACK_BUF_SIZE; ++i) {
        state.push_back[i].token_type = 0;
    }
    state.obstack = malloc(sizeof(struct obstack));
    obstack_init(state.obstack);

    int custom_typenames = count_typenames(typenames);
    int n_typenames = custom_typenames + count_typenames(typename_starters);

    state.typename_starters = malloc((n_typenames + 1) * sizeof(char*));
    copy_typenames(state.typename_starters, typenames);
    copy_typenames(state.typename_starters + custom_typenames,
                   typename_starters);
    state.typename_starters[n_typenames] = 0;

    return state;
}

static int match(struct token token, enum token_type expected_type) {
    return token.token_type == expected_type;
}

static void error(struct parse_state* state, const char* message, ...) {

    //compute size
    va_list args;
    va_start (args, message);
    int size = vsnprintf(0, 0, message, args);
    va_end (args);
    char* errbuf = malloc(size + 1);
    va_start (args, message);
    vsnprintf(errbuf, size + 1, message, args);
    va_end (args);

    state->error_message = errbuf;
}

void free_parse_result_contents(struct parse_result *result) {
    if (result->obstack) {
        obstack_free(result->obstack, 0);
        free(result->obstack);
        result->obstack = 0;
    }
    if (result->is_error) {
        free(result->error_message);
        result->error_message = 0;
    }
}

/* In the event of an error, we need to free the parse
   tree nodes that we have allocated */
static void free_parse_state(struct parse_state* state) {
    obstack_free(state->obstack, 0);
    free(state->obstack);
    state->obstack = 0;
    free(state->typename_starters);
    state->typename_starters = 0;
}

static struct parse_tree_node* make_binary_node(struct parse_state* state,
                                         enum token_type op,
                                         struct parse_tree_node* left,
                                         struct parse_tree_node* right) {

    struct parse_tree_node* node;
    node = obstack_alloc(state->obstack, sizeof(struct parse_tree_node));

    node->text = 0;
    node->op = op;
    node->next_sibling = 0;
    node->first_child = left;
    left->next_sibling = right;
    return node;
}

static struct parse_tree_node* make_terminal_node(struct parse_state* state,
                                                  struct token token) {

    struct parse_tree_node* node;
    node = obstack_alloc(state->obstack, sizeof(struct parse_tree_node));

    node->first_child = node->next_sibling = 0;
    node->op = token.token_type;
    if (token.token_value) {
        node->text = obstack_strdup(state->obstack, token.token_value);
    } else {
        node->text = 0;
    }
    return node;
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

static int is_typename_first_word(struct parse_state* state,
                                  struct token token) {
    if (token.token_type != LITERAL_OR_ID) {
        return 0;
    }
    for (char** type = state->typename_starters; *type; ++type) {
        if (strcmp(*type, token.token_value) == 0) {
            return 1;
        }
    }
    return 0;
}

static void append_token(struct obstack* obstack, struct token token) {
    const char* token_str;
    if (token.token_value) {
        token_str = token.token_value;
    } else {
        token_str = token_names[token.token_type];
    }

    if (obstack_object_size(obstack)) {
        obstack_1grow(obstack, ' ');
    }
    obstack_grow(obstack, token_str, strlen(token_str));

}

/* () [] . -> expr++ expr--	 */
static struct parse_tree_node* parse_primary_expression(struct parse_state *state) {
    struct token tok = get_next_parse_token(state);
    HANDLE_BOGUS_TOKEN(tok);
    struct parse_tree_node* node = 0;
    switch(tok.token_type) {
    case OPEN_PAREN:
        node = parse_comma(state);
        if (!node) {
            return 0;
        }
        tok = get_next_parse_token(state);
        HANDLE_BOGUS_TOKEN(tok);
        if (tok.token_type != CLOSE_PAREN) {
            error(state, "Missing ) parsing parenthesized expression");
            return 0;
        }
        break;
    case LITERAL_OR_ID:
        node = make_terminal_node(state, tok);
        break;
    default:
        error(state, "Unexpected %s", token_names[tok.token_type]);
        return 0;
    }

    bool more = true;
    while (more) {
        tok = get_next_parse_token(state);
        HANDLE_BOGUS_TOKEN(tok);
        switch(tok.token_type) {

        case OPEN_BRACKET:
        {
            tok = get_next_parse_token(state);
            push_back(state, tok);
            if (match(tok, END_OF_EXPRESSION)) {
                error(state, "Missing ] at end of input");
                return 0;
            }

            struct parse_tree_node* right_node = parse_comma(state);
            if (!right_node) {
                return 0;
            }

            tok = get_next_parse_token(state);
            HANDLE_BOGUS_TOKEN(tok);
            if (!match(tok, CLOSE_BRACKET)) {
                error(state, "Missing ]");
                return 0;
            }

            node = make_binary_node(state, SUBSCRIPT, node, right_node);
        }
        break;
        case OPEN_PAREN:
        {
            struct parse_tree_node* prev_child = node;
            node = make_binary_node(state, FUNCTION_CALL, node, 0);
            while (1) {
                tok = get_next_parse_token(state);
                HANDLE_BOGUS_TOKEN(tok);

                struct parse_tree_node* next_node;
                if (tok.token_type == OPEN_PAREN) {
                    push_back(state, tok);
                    next_node = parse_primary_expression(state);
                } else if (tok.token_type == CLOSE_PAREN) {
                    break;
                } else {
                    push_back(state, tok);
                    next_node = parse_assignop(state);
                }

                tok = get_next_parse_token(state);
                HANDLE_BOGUS_TOKEN(tok);
                if (!match(tok, COMMA)) {
                    if (match(tok, CLOSE_PAREN)) {
                        prev_child->next_sibling = next_node;
                        prev_child = next_node;
                        break;
                    } else {
                        error(state,
                              "Unexpected %s while parsing function call",
                              token_names[tok.token_type]);
                        return 0;
                    }
                }

                if (next_node == 0) {
                    error(state, "Missing ) while parsing function call");
                    return 0;
                }
                prev_child->next_sibling = next_node;
                prev_child = next_node;
            }
        }
        break;
        case ARROW:
        case DOT:
        {
            enum token_type op = tok.token_type;
            tok = get_next_parse_token(state);
            HANDLE_BOGUS_TOKEN(tok);
            if (!match(tok, LITERAL_OR_ID)) {
                error(state, "Expected identifier before %s token",
                      token_names[tok.token_type]);
                return 0;
            }
            node = make_binary_node(state, op, node, make_terminal_node(state, tok));
            break;
        }
        case DOUBLE_PLUS:
            node = make_binary_node(state, POSTINCREMENT, node, 0);
            break;
        case DOUBLE_MINUS:
            node = make_binary_node(state, POSTDECREMENT, node, 0);
            break;
        default:
            //not part of a primary expression
            more = false;
            push_back(state, tok);
            break;
        }
    }
    return node;
}

static char* parse_parenthesized_typename(struct token tok, struct parse_state* state) {
    //parse until close-paren
    while (tok.token_type != CLOSE_PAREN) {
        if (tok.token_type == END_OF_EXPRESSION) {
            char* typename = obstack_finish(state->obstack);
            obstack_free(state->obstack, typename);
            return 0;
        }
        append_token(state->obstack, tok);
        tok = get_next_parse_token(state);
        HANDLE_BOGUS_TOKEN(tok);
    }
    obstack_1grow(state->obstack, 0);
    return obstack_finish(state->obstack);
}

/*
   * & + - ! ~ ++expr --expr (typecast) sizeof
 */
static struct parse_tree_node* parse_unop(struct parse_state *state) {

    struct parse_tree_node* node;

    struct token tok = get_next_parse_token(state);
    HANDLE_BOGUS_TOKEN(tok);
    if (tok.token_type == SIZEOF) {
        struct token sizeof_tok = tok;
        tok = get_next_parse_token(state);
        HANDLE_BOGUS_TOKEN(tok);
        if (tok.token_type == OPEN_PAREN) {
            //sizeof(typename)
            tok = get_next_parse_token(state);
            HANDLE_BOGUS_TOKEN(tok);
            char* typename = parse_parenthesized_typename(tok, state);
            if (!typename) {
                error(state, "Missing ) in sizeof");
                return 0;
            }
            node = make_terminal_node(state, sizeof_tok);
            node->text = typename;
        } else {
            //sizeof var
            node = make_terminal_node(state, sizeof_tok);
            node->text = obstack_strdup(state->obstack, tok.token_value);
        }

    } else if (is_unop(tok)) {
        struct parse_tree_node* child_node = parse_unop(state);
        if (!child_node) {
            return 0;
        }
        enum token_type token_type;
        switch (tok.token_type) {
        case STAR:
            token_type = DEREFERENCE;
            break;
        case AMPERSAND:
            token_type = REFERENCE;
            break;
        case MINUS:
            token_type = UNARY_MINUS;
            break;
        case DOUBLE_PLUS:
            token_type = PREINCREMENT;
            break;
        case DOUBLE_MINUS:
            token_type = PREDECREMENT;
            break;
        default:
            token_type = tok.token_type;
            break;
        }
        node = make_binary_node(state, token_type, child_node, 0);
    } else if (tok.token_type == OPEN_PAREN) {
        //handle typecasts via hack
        struct token next_tok = get_next_parse_token(state);
        HANDLE_BOGUS_TOKEN(next_tok);
        if (is_typename_first_word(state, next_tok)) {
            char* typename = parse_parenthesized_typename(next_tok, state);
            if (!typename) {
                error(state, "Missing ) in (assumed) typecast");
                return 0;
            }

            struct parse_tree_node* child_node = parse_unop(state);
            if (!child_node) {
                return 0;
            }
            node = make_binary_node(state, TYPECAST, child_node, 0);
            node->text = typename;
        } else {
            push_back(state, next_tok);
            push_back(state, tok);
            //handle parenthesized expression
            node = parse_primary_expression(state);
            if (!node) {
                return 0;
            }
        }
    } else {
        push_back(state, tok);
        node = parse_primary_expression(state);
    }

    return node;
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
static struct parse_tree_node* parse_binop(struct parse_state *state, int level) {
    struct parse_tree_node* node;
    if (binops[level][0] == 0) {
        node = parse_unop(state);
        return node;
    } else {
        node = parse_binop(state, level + 1);
        if (!node) {
            return 0;
        }
    }
    while(1) {
        struct token tok = get_next_parse_token(state);
        HANDLE_BOGUS_TOKEN(tok);
        if (tok.token_type == END_OF_EXPRESSION) {
            return node;
        }
        else if (tok.token_type == CLOSE_PAREN ||
                 tok.token_type == CLOSE_BRACKET) {
            push_back(state, tok);
            return node;
        }
        if (!is_level_binop(tok, level)) {
            push_back(state, tok);
            return node;
        }
        struct parse_tree_node *right_node = parse_binop(state, level + 1);
        if (!right_node) {
            return 0;
        }
        node = make_binary_node(state, tok.token_type, node, right_node);
    }
}

static struct parse_tree_node* parse_ternop(struct parse_state *state) {
    struct parse_tree_node *left_node = parse_binop(state, 0);
    if (!left_node) {
        return 0;
    }

    struct token tok = get_next_parse_token(state);
    HANDLE_BOGUS_TOKEN(tok);
    if (tok.token_type != QUESTION) {
        push_back(state, tok);
        return left_node;
    }

    struct parse_tree_node *mid_node = parse_ternop(state);
    if (!mid_node) {
        return 0;
    }

    tok = get_next_parse_token(state);
    HANDLE_BOGUS_TOKEN(tok);
    if (tok.token_type != COLON) {
        error(state, "Missing : in ?: ternary op (found %s)",
              token_names[tok.token_type]);
        return 0;
    }
    struct parse_tree_node* right_node = parse_ternop(state);
    if (!right_node) {
        return 0;
    }

    struct parse_tree_node* new_right = make_binary_node(state, COLON, mid_node, right_node);
    return make_binary_node(state, QUESTION, left_node, new_right);
}

static struct parse_tree_node* parse_assignop(struct parse_state *state) {
    struct parse_tree_node *node = parse_ternop(state);
    if (!node) {
        return 0;
    }

    struct token tok = get_next_parse_token(state);
    if (!is_assignop(tok)) {
        push_back(state, tok);
        return node;
    }

    struct parse_tree_node *right_node = parse_assignop(state);
    if (!right_node) {
        return 0;
    }

    node = make_binary_node(state, tok.token_type, node, right_node);
    return node;
}

static struct parse_tree_node* parse_comma(struct parse_state *state) {
    struct parse_tree_node *node = parse_assignop(state);
    if (!node)
        return 0;
    while (1) {
        struct token tok = get_next_parse_token(state);

        if (tok.token_type == END_OF_EXPRESSION ||
            tok.token_type == CLOSE_PAREN ||
            tok.token_type == CLOSE_BRACKET) {
            push_back(state, tok);
            return node;
        }
        if (!match(tok, COMMA)) {
            error(state, "Unexpected %s", token_names[tok.token_type]);
            return 0;
        }
        struct parse_tree_node *right_node = parse_assignop(state);
        if (!right_node) {
            return 0;
        }
        node = make_binary_node(state, COMMA, node, right_node);
    }
}

static bool is_empty(struct parse_state* state) {
    struct token tok = get_next_parse_token(state);
    bool is_empty = match(tok, END_OF_EXPRESSION);
    push_back(state, tok);
    return is_empty;
}

struct parse_result* parse(const char* string, char** typenames) {
    lex_buf lex_buf = start_lex(string);
    struct parse_state state = make_parse_state(&lex_buf, typenames);

    struct parse_tree_node* node = 0;
    if (is_empty(&state)) {
        state.error_message = strdup("Empty expression");
    } else {
        node = parse_comma(&state);
    }
    struct parse_result* result = malloc(sizeof(struct parse_result));
    result->obstack = 0;
    if (!node) {
        result->is_error = true;
        result->error_message = state.error_message;
        free_parse_state(&state);
    } else {
        struct token tok = get_next_parse_token(&state);
        if (tok.token_type == END_OF_EXPRESSION) {
            result->is_error = false;
            result->node = node;
            result->obstack = state.obstack;
        } else {
            error(&state, "Unexpected %s at end of input",
                  token_names[tok.token_type]);
            result->is_error = true;
            result->error_message = state.error_message;
            free_parse_state(&state);
        }
    }

    done_lex(lex_buf);
    return result;
}

/*
  Returns a pointer to the new end of the string.  Assumes
  that the buffer has enough space for the string.
 */
char* write_tree_to_string(struct parse_tree_node* node, char* buf) {
    switch (node->op) {
    case DEREFERENCE:
    case REFERENCE:
    case UNARY_MINUS:
    case TILDE:
    case BANG:
    case PREINCREMENT:
    case PREDECREMENT:
        /* unary ops */
        assert (node->first_child);
        buf += sprintf(buf, "%s(", token_sigils[node->op]);
        buf = write_tree_to_string(node->first_child, buf);
        buf += sprintf(buf, ")");
        break;

    case TYPECAST:
        buf += sprintf(buf, "((%s)", node->text);
        assert(node->first_child);
        assert(!node->first_child->next_sibling);
        buf = write_tree_to_string(node->first_child, buf);
        buf += sprintf(buf, ")");
        break;

    case FUNCTION_CALL:
    {
        assert(node->first_child);
        buf = write_tree_to_string(node->first_child, buf);
        buf += sprintf(buf, "(");
        struct parse_tree_node* cur = node->first_child->next_sibling;
        while (cur) {
            buf = write_tree_to_string(cur, buf);
            cur = cur->next_sibling;
            if (cur) {
                buf += sprintf(buf, ",");
            }
        }
        buf += sprintf(buf, ")");
    }
    break;

    case SUBSCRIPT:
        assert(node->first_child);
        assert(node->first_child->next_sibling);
        buf = write_tree_to_string(node->first_child, buf);
        buf += sprintf(buf, "[");
        buf = write_tree_to_string(node->first_child->next_sibling, buf);
        buf += sprintf(buf, "]");
        break;

    case SIZEOF:
        buf += sprintf(buf, "sizeof(%s)", node->text);
        break;

    case LITERAL_OR_ID:
        buf += sprintf(buf, "%s", node->text);
        break;

    default:
        buf += sprintf(buf, "(");
        if (node->first_child) {
            buf = write_tree_to_string(node->first_child, buf);
        }
        buf += sprintf(buf, "%s", token_sigils[node->op]);
        struct parse_tree_node* cur = node->first_child->next_sibling;
        while (cur) {
            buf = write_tree_to_string(cur, buf);
            cur = cur->next_sibling;
        }
        buf += sprintf(buf, ")");
        break;
    }
    return buf;
}
