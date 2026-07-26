#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_IDENT,
    TOKEN_NUMBER,
    TOKEN_CHAR_LITERAL,
    TOKEN_STRING,
    TOKEN_NULL,
    TOKEN_TRUE,
    TOKEN_FALSE,

    TOKEN_U8,
    TOKEN_U16,
    TOKEN_U32,
    TOKEN_U64,
    // Reserved for future wide-integer support. Not yet consumed by
    // the parser's type-name switch.
    TOKEN_U128,
    TOKEN_U256,

    TOKEN_I8,
    TOKEN_I16,
    TOKEN_I32,
    TOKEN_I64,
    // Reserved for future wide-integer support. Not yet consumed by
    // the parser's type-name switch.
    TOKEN_I128,
    TOKEN_I256,

    TOKEN_BOOL,
    TOKEN_CHAR,
    TOKEN_STRING_TYPE,
    TOKEN_FLOAT,
    TOKEN_DOUBLE,
    TOKEN_VOID,

    TOKEN_CONST,
    TOKEN_PUB,
    TOKEN_PRIVATE,
    // Reserved for future storage-class/qualifier support. Not yet
    // consumed by the parser.
    TOKEN_STATIC,
    TOKEN_INLINE,

    // Reserved for future data-type work (Phase 4 of the roadmap).
    // Not yet consumed by the parser.
    TOKEN_ENUM,
    TOKEN_STRUCT,
    TOKEN_CLASS,
    TOKEN_UNION,

    TOKEN_IMPORT,

    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_SWITCH,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_RETURN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_UNSAFE,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_BANG,
    TOKEN_ASSIGN,
    TOKEN_EQ,
    TOKEN_NEQ,
    TOKEN_LT,
    TOKEN_LTE,
    TOKEN_GT,
    TOKEN_GTE,
    TOKEN_AND,
    TOKEN_AND_AND,
    TOKEN_OR,
    TOKEN_OR_OR,
    TOKEN_XOR,
    TOKEN_TILDE,
    TOKEN_QUESTION,
    TOKEN_LSHIFT,
    TOKEN_RSHIFT,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_DOT,
    TOKEN_COMMA,
    TOKEN_SEMI,
    TOKEN_COLON,

    TOKEN_LDBRACKET,
    TOKEN_RDBRACKET,

    // Reserved for future inline-assembly support (Phase 5/6 of the
    // roadmap: "inline assembly requires unsafe"). Not yet consumed
    // by the parser.
    TOKEN_ASM,
    TOKEN_VOLATILE,
    TOKEN_ATTRIBUTE
} TokenType;

// A token points directly into the source buffer; it does not own `lexeme`.
typedef struct {
    TokenType type;
    const char *lexeme;
    size_t length;
    int line;
    int column;
} Token;

// Lexer state for a single source buffer, including current source position.
typedef struct {
    const char *src;
    size_t pos;
    int line;
    int column;
    size_t pending_error_pos;
    int pending_error_line;
    int pending_error_column;
    int has_pending_error;
} Lexer;

// Initializes the lexer before the first call to lexer_next_token.
void lexer_init(Lexer *lexer, const char *source);
// Returns the next token and advances the lexer.
Token lexer_next_token(Lexer *lexer);

#endif
