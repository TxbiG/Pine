#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

// ===== Token Types =====
typedef enum {
    TOKEN_EOF,
    TOKEN_IDENT,
    TOKEN_NUMBER,
    TOKEN_STRING,

    TOKEN_U8,
    TOKEN_U16,
    TOKEN_U32,
    TOKEN_U64,
    TOKEN_U128,
    TOKEN_U256,
    
    TOKEN_I8,
    TOKEN_I16,
    TOKEN_I32,
    TOKEN_I64,
    TOKEN_I128,
    TOKEN_I256,

    TOKEN_BOOL,
    TOKEN_CHAR,
    TOKEN_STRING_TYPE,
    TOKEN_FLOAT,
    TOKEN_DOUBLE,
    TOKEN_VOID,

    TOKEN_STATIC,
    TOKEN_CONST,
    TOKEN_INLINE,

    TOKEN_ENUM,
    TOKEN_STRUCT,
    TOKEN_UNION,
    TOKEN_CLASS,
    TOKEN_TYPEDEF,

    TOKEN_PRAGMA,

    TOKEN_MACRO_DEFINE,
    TOKEN_MACRO_DEFINED,
    TOKEN_MACRO_IF,
    TOKEN_MACRO_ELSE,
    TOKEN_MACRO_ENDIF,

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
    TOKEN_SEMI,    // ;

    TOKEN_LDBRACKET, // [[
    TOKEN_RDBRACKET  // ]]

    TOKEN_ASM,       // __asm__
    TOKEN_VOLATILE,  // __volatile__
    TOKEN_ATTRIBUTE  // __attribute__
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
