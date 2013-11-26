#ifndef LEX_H
#define LEX_H

#include <obstack.h>

enum token_type {
    LITERAL_OR_ID=1,
    OPEN_PAREN,
    CLOSE_PAREN,
    OPEN_BRACKET,
    CLOSE_BRACKET,
    OPEN_CURLY,
    CLOSE_CURLY,
    BANG,
    PERCENT,
    CARET,
    AMPERSAND,
    BAR,
    STAR,
    MINUS,
    PLUS,
    SLASH,
    LEFT_SHIFT,
    RIGHT_SHIFT,

    BANG_EQUAL,
    PERCENT_EQUAL,
    CARET_EQUAL,
    AMPERSAND_EQUAL,
    BAR_EQUAL,
    STAR_EQUAL,
    MINUS_EQUAL,
    PLUS_EQUAL,
    SLASH_EQUAL,
    LEFT_SHIFT_EQUAL,
    RIGHT_SHIFT_EQUAL,

    ARROW,
    DOT,

    ASSIGN,

    IS_EQUAL,
    LTE,
    GTE,
    LT,
    GT,

    DOUBLE_AMPERSAND,
    DOUBLE_BAR,
    DOUBLE_PLUS,
    DOUBLE_MINUS,

    QUESTION,
    COLON,
    SIZEOF,
    COMMA,
    TILDE,

    /* hack -- not token types but we reuse these as parse tree node
     * types */
    TYPECAST,
    FUNCTION_CALL,
    SUBSCRIPT,
    REFERENCE,
    DEREFERENCE,
    UNARY_MINUS,

    /* not actually ever returned, but needed for correct lexing */
    START_COMMENT,

    BOGUS,
    END_OF_EXPRESSION
};

extern const char* token_names[];
extern const char* token_sigils[];

struct token {
    enum token_type token_type;
    //if non-0, owned by this token
    char *token_value;
};

typedef struct {
    const char* pos;
    struct obstack obstack;
} lex_buf;

lex_buf start_lex(const char* expr);
void done_lex(lex_buf lex_buf);

struct token get_next_token(lex_buf* buf);

#endif
