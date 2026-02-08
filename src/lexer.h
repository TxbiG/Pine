#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

// ===== Token Types =====
typedef enum {
    TOKEN_EOF,
    TOKEN_IDENT,
    TOKEN_NUMBER,
    TOKEN_STRING,

    TOKEN_U8,          // uint8_t
    TOKEN_U16,         // uint16_t
    TOKEN_U32,         // uint32_t
    TOKEN_U64,         // uint64_t
    TOKEN_U128,        // uint128_t
    TOKEN_U256,        // uint256_t
    
    TOKEN_I8,          // int8_t
    TOKEN_I16,         // int16_t
    TOKEN_I32,         // int32_t
    TOKEN_I64,         // int64_t
    TOKEN_I128,        // int128_t
    TOKEN_I256,        // int256_t

    TOKEN_BOOL,        // bool
    TOKEN_CHAR,        // char
    TOKEN_STRING_TYPE, // string
    TOKEN_FLOAT,       // float
    TOKEN_DOUBLE,      // double
    TOKEN_VOID,        // void

    TOKEN_STATIC,      // static
    TOKEN_CONST,       // const
    TOKEN_INLINE,      // inline

    TOKEN_ENUM,        // enum
    TOKEN_STRUCT,      // struct
    TOKEN_UNION,       // union
    TOKEN_CLASS,       // class
    TOKEN_TYPEDEF,     // typedef

    TOKEN_PRAGMA,    // #pragma
    TOKEN_IMPORT,    // #import

    TOKEN_MACRO_DEFINE,    // #define
    TOKEN_MACRO_DEFINED,   // defined(expression)
    TOKEN_MACRO_IF,        // #if
    TOKEN_MACRO_ELSE,      // #else
    TOKEN_MACRO_ELIF,      // #elif
    TOKEN_MACRO_ENDIF,     // #endif

    // Keywords
    TOKEN_IF,                // if (expression)
    TOKEN_ELSE,              // else
    TOKEN_WHILE,             // while(expression)
    TOKEN_SWITCH,            // switch(expression)
    TOKEN_RETURN,            // return
    TOKEN_INT,               // 

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
