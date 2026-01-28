#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

// ===== Token Types =====
typedef enum {
    TOKEN_EOF,
    TOKEN_IDENT,
    TOKEN_NUMBER,
    TOKEN_STRING,

    // Keywords
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_SWITCH,
    TOKEN_RETURN,
    TOKEN_INT,

    // Operators / punctuation
    TOKEN_PLUS,    // +
    TOKEN_MINUS,   // -
    TOKEN_STAR,    // *
    TOKEN_SLASH,   // /
    TOKEN_ASSIGN,  // =
    TOKEN_EQ,      // ==
    TOKEN_NEQ,     // !=
    TOKEN_LPAREN,  // (
    TOKEN_RPAREN,  // )
    TOKEN_LBRACE,  // {
    TOKEN_RBRACE,  // }
    TOKEN_SEMI     // ;
} TokenType;

// ===== Token Struct =====
typedef struct {
    TokenType type;
    const char *lexeme;  // pointer into source or allocated
    size_t length;

    int line;
    int column;
} Token;

// ===== Lexer State =====
typedef struct {
    const char *src;
    size_t pos;
    int line;
    int column;
} Lexer;

// ===== API =====
void lexer_init(Lexer *lexer, const char *source);
Token lexer_next_token(Lexer *lexer);

#endif
