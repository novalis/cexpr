#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "lex.h"

const char *token_names[] = {

    "null","lit","(",")","[","]","{","}","!","%","^",
    "&","|","*","-","+","/","<<",">>","!=","%=","^=",
    "&=","|=","*=","-=","+=","/=","<<=",">>=","->",".",
    "=","==","<=",">=","<",">","&&","||","++","--","?",
    ":","sizeof",",","~","(typecast)",
    "(function call)", "subscript", "bogus", "eof"
};

struct token_spec {
    const char* text;
    enum token_type token_type;
};

struct token_spec token_specs[] = {
    {"(", OPEN_PAREN},
    {")",CLOSE_PAREN},
    {"[",OPEN_BRACKET},
    {"]",CLOSE_BRACKET},
    {"{",OPEN_CURLY},
    {"}",CLOSE_CURLY},
    {"!",BANG},
    {"!=",BANG_EQUAL},
    {"%",PERCENT},
    {"%=",PERCENT_EQUAL},
    {"^",CARET},
    {"^=",CARET_EQUAL},
    {"&",AMPERSAND},
    {"&&",DOUBLE_AMPERSAND},
    {"&=",AMPERSAND_EQUAL},
    {"|",BAR},
    {"||",DOUBLE_BAR},
    {"|=",BAR_EQUAL},
    {"*",STAR},
    {"*=",STAR_EQUAL},
    {"-",MINUS},
    {"--",DOUBLE_MINUS},
    {"-=",MINUS_EQUAL},
    {"->",ARROW},
    {".",DOT},
    {"+",PLUS},
    {"++",DOUBLE_PLUS},
    {"+=",PLUS_EQUAL},
    {"/",SLASH},
    {"/=",SLASH_EQUAL},
    {">", GT},
    {">>",RIGHT_SHIFT},
    {">>=",RIGHT_SHIFT_EQUAL},
    {">=",GTE},
    {"<", LT},
    {"<<",LEFT_SHIFT},
    {"<<=",LEFT_SHIFT_EQUAL},
    {"<=",LTE},
    {"=", ASSIGN},
    {"==", IS_EQUAL},
    {"?", QUESTION},
    {":", COLON},
    {",", COMMA},
    {"~", TILDE},
    {"", -1},
};

struct token_rule {
    struct token_rule *children;
    enum token_type token_type;
};

void add_child_token_rule(struct token_rule* rule, const char* text,
                          enum token_type token_type) {
    unsigned char c = *text;
    if (!c) {
        rule->token_type = token_type;
        return;
    }
    if (rule->children == 0) {
        rule->children = calloc(256, sizeof(struct token_rule));
    }
    add_child_token_rule(rule->children + c, text + 1, token_type);
}

struct token_rule* make_token_rules(struct token_spec* token_spec) {
    struct token_rule* rule = malloc(sizeof(struct token_rule));

    rule->children = calloc (256, sizeof(struct token_rule));
    while (*token_spec->text) {
        unsigned char c = token_spec->text[0];
        struct token_rule* child = &rule->children[c];
        child->token_type = token_spec->token_type;

        const char* text = token_spec->text;
        token_spec ++;
        while (*text == *token_spec->text) {
            add_child_token_rule(rule, token_spec->text, 
                                 token_spec->token_type);
            token_spec ++;
        }
    }
    return rule;
}

static struct token_rule* rules = 0;

lex_buf start_lex(const char* expr) {
    lex_buf buf = {.pos = expr};
    if (rules == 0) {
        rules = make_token_rules(token_specs);
    }
    return buf;
}

struct token get_next_token(lex_buf* buf) {
    const char* pos = buf->pos;

    struct token token;
    token.token_value = 0;

    while (1) {
        struct token_rule *rule = rules;
        while (rule->children && rule->children[(unsigned char)*pos].token_type) {
            rule = rule->children + (unsigned char)*pos++;
        }
        if (rule != rules) {
            token.token_type = rule->token_type;
            goto done;
        } else {
            switch (*pos) {
            case 0:
                token.token_type = END_OF_EXPRESSION;
                goto done;
            case ' ': case '\t': case '\v': case '\n':
                break;
            case '\'':
            case '"':
            {
                char delimiter = *pos;
                const char *start = pos;
                while (*++pos != delimiter) {
                    if (!*pos) {
                        token.token_type = BOGUS;
                        goto done;
                    } else if (*pos == '\\') {
                        pos ++;
                    }
                }
                pos++;
                token.token_value = strndup(start, pos - start);
                token.token_type = LITERAL_OR_ID;
                goto done;
            }
            case '_': case 'A': case 'a': case 'B': case 'b': case 'C': case 'c': case 'D': case 'd': case 'E': case 'e': case 'F': case 'f': case 'G': case 'g': case 'H': case 'h': case 'I': case 'i': case 'J': case 'j': case 'K': case 'k': case 'L': case 'l': case 'M': case 'm': case 'N': case 'n': case 'O': case 'o': case 'P': case 'p': case 'Q': case 'q': case 'R': case 'r': case 'S': case 's': case 'T': case 't': case 'U': case 'u': case 'V': case 'v': case 'W': case 'w': case 'X': case 'x': case 'Y': case 'y': case 'Z': case 'z': case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':

                if (strncmp(pos, "sizeof", 6) == 0 && 
                    !isalnum(pos[6]) && 
                    pos[6] != '_') {
                        token.token_type = SIZEOF;
                        pos += 6;
                        goto done;
                } else {
                    token.token_type = LITERAL_OR_ID;
                    const char *start = pos;
                    while (*pos && (isalnum(*pos) || *pos == '_')) {
                        pos++;
                    }
                    token.token_value = strndup(start, pos - start);
                    goto done;
                }
                break;
            default:
                token.token_value = strdup(pos);
                token.token_type = BOGUS;
                pos++;
                goto done;
            }
            ++pos;
        }
    }

done:
    buf->pos = pos;
    return token;

}
