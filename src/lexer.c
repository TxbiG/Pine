#include "lexer.h"

#include <ctype.h>
#include <string.h>

typedef struct {
    const char *name;
    TokenType type;
} Keyword;

static Keyword keywords[] = {
    {"u8", TOKEN_U8},
    {"u16", TOKEN_U16},
    {"u32", TOKEN_U32},
    {"u64", TOKEN_U64},
    {"u128", TOKEN_U128},
    {"u256", TOKEN_U256},
    {"i8", TOKEN_I8},
    {"i16", TOKEN_I16},
    {"i32", TOKEN_I32},
    {"i64", TOKEN_I64},
    {"i128", TOKEN_I128},
    {"i256", TOKEN_I256},
    {"bool", TOKEN_BOOL},
    {"char", TOKEN_CHAR},
    {"string", TOKEN_STRING_TYPE},
    {"float", TOKEN_FLOAT},
    {"double", TOKEN_DOUBLE},
    {"void", TOKEN_VOID},
    {"const", TOKEN_CONST},
    {"pub", TOKEN_PUB},
    {"private", TOKEN_PRIVATE},
    {"static", TOKEN_STATIC},
    {"inline", TOKEN_INLINE},
    {"enum", TOKEN_ENUM},
    {"struct", TOKEN_STRUCT},
    {"class", TOKEN_CLASS},
    {"union", TOKEN_UNION},
    {"import", TOKEN_IMPORT},
    {"null", TOKEN_NULL},
    {"true", TOKEN_TRUE},
    {"false", TOKEN_FALSE},
    {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},
    {"while", TOKEN_WHILE},
    {"for", TOKEN_FOR},
    {"switch", TOKEN_SWITCH},
    {"case", TOKEN_CASE},
    {"default", TOKEN_DEFAULT},
    {"return", TOKEN_RETURN},
    {"break", TOKEN_BREAK},
    {"continue", TOKEN_CONTINUE},
    {"unsafe", TOKEN_UNSAFE},
    {"__asm__", TOKEN_ASM},
    {"__volatile__", TOKEN_VOLATILE},
    {"__attribute__", TOKEN_ATTRIBUTE},
    {NULL, TOKEN_EOF}
};

// Builds a token whose lexeme spans from `start` to the current lexer position.
static Token make_token(Lexer *lexer, TokenType type, size_t start, int line, int column) {
    Token token;
    token.type = type;
    token.lexeme = lexer->src + start;
    token.length = lexer->pos - start;
    token.line = line;
    token.column = column;
    return token;
}

// Returns the current character without consuming it.
static char peek(Lexer *lexer) {
    return lexer->src[lexer->pos];
}

// Returns the following character, or NUL at end of source.
static char peek_next(Lexer *lexer) {
    if (lexer->src[lexer->pos] == '\0') {
        return '\0';
    }
    return lexer->src[lexer->pos + 1];
}

// Consumes one character and updates line/column tracking.
static char advance(Lexer *lexer) {
    char c = lexer->src[lexer->pos++];
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

// Consumes and returns a one-character token.
static Token make_single(Lexer *lexer, TokenType type, int line, int column) {
    size_t start = lexer->pos;
    advance(lexer);
    return make_token(lexer, type, start, line, column);
}

// Skips whitespace plus line and block comments.
static void skip_space_and_comments(Lexer *lexer) {
    for (;;) {
        char c = peek(lexer);
        if (isspace((unsigned char)c)) {
            advance(lexer);
            continue;
        }
        if (c == '/' && peek_next(lexer) == '/') {
            while (peek(lexer) != '\0' && peek(lexer) != '\n') {
                advance(lexer);
            }
            continue;
        }
        if (c == '/' && peek_next(lexer) == '*') {
            size_t start = lexer->pos;
            int line = lexer->line;
            int column = lexer->column;
            int closed = 0;
            advance(lexer);
            advance(lexer);
            while (peek(lexer) != '\0') {
                if (peek(lexer) == '*' && peek_next(lexer) == '/') {
                    advance(lexer);
                    advance(lexer);
                    closed = 1;
                    break;
                }
                advance(lexer);
            }
            if (!closed) {
                lexer->pending_error_pos = start;
                lexer->pending_error_line = line;
                lexer->pending_error_column = column;
                lexer->has_pending_error = 1;
                return;
            }
            continue;
        }
        break;
    }
}

// Maps identifier text to a keyword token when it is reserved.
static TokenType keyword_type(const char *text, size_t length) {
    for (int i = 0; keywords[i].name != NULL; i++) {
        if (strlen(keywords[i].name) == length &&
            strncmp(keywords[i].name, text, length) == 0) {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENT;
}

void lexer_init(Lexer *lexer, const char *source) {
    lexer->src = source;
    lexer->pos = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->pending_error_pos = 0;
    lexer->pending_error_line = 0;
    lexer->pending_error_column = 0;
    lexer->has_pending_error = 0;
}

Token lexer_next_token(Lexer *lexer) {
    skip_space_and_comments(lexer);
    if (lexer->has_pending_error) {
        Token token;
        token.type = TOKEN_ERROR;
        token.lexeme = lexer->src + lexer->pending_error_pos;
        token.length = lexer->pos - lexer->pending_error_pos;
        token.line = lexer->pending_error_line;
        token.column = lexer->pending_error_column;
        lexer->has_pending_error = 0;
        return token;
    }

    int line = lexer->line;
    int column = lexer->column;
    size_t start = lexer->pos;
    char c = peek(lexer);

    if (c == '\0') {
        return make_token(lexer, TOKEN_EOF, start, line, column);
    }

    if (isalpha((unsigned char)c) || c == '_') {
        while (isalnum((unsigned char)peek(lexer)) || peek(lexer) == '_') {
            advance(lexer);
        }
        return make_token(lexer, keyword_type(lexer->src + start, lexer->pos - start), start, line, column);
    }

    if (isdigit((unsigned char)c)) {
        while (isdigit((unsigned char)peek(lexer))) {
            advance(lexer);
        }
        return make_token(lexer, TOKEN_NUMBER, start, line, column);
    }

    if (c == '"') {
        advance(lexer);
        while (peek(lexer) != '\0' && peek(lexer) != '"') {
            if (peek(lexer) == '\\') {
                advance(lexer);
                if (peek(lexer) == '\0') {
                    return make_token(lexer, TOKEN_ERROR, start, line, column);
                }
            }
            advance(lexer);
        }
        if (peek(lexer) != '"') {
            return make_token(lexer, TOKEN_ERROR, start, line, column);
        }
        advance(lexer);
        return make_token(lexer, TOKEN_STRING, start, line, column);
    }

    if (c == '\'') {
        advance(lexer);
        if (peek(lexer) == '\\') {
            advance(lexer);
            if (peek(lexer) == '\0') {
                return make_token(lexer, TOKEN_ERROR, start, line, column);
            }
            advance(lexer);
        } else if (peek(lexer) != '\0' && peek(lexer) != '\'') {
            advance(lexer);
        } else {
            return make_token(lexer, TOKEN_ERROR, start, line, column);
        }
        if (peek(lexer) != '\'') {
            return make_token(lexer, TOKEN_ERROR, start, line, column);
        }
        advance(lexer);
        return make_token(lexer, TOKEN_CHAR_LITERAL, start, line, column);
    }

    if (c == '=' && peek_next(lexer) == '=') {
        advance(lexer);
        advance(lexer);
        return make_token(lexer, TOKEN_EQ, start, line, column);
    }
    if (c == '!' && peek_next(lexer) == '=') {
        advance(lexer);
        advance(lexer);
        return make_token(lexer, TOKEN_NEQ, start, line, column);
    }
    if (c == '&' && peek_next(lexer) == '&') {
        advance(lexer);
        advance(lexer);
        return make_token(lexer, TOKEN_AND_AND, start, line, column);
    }
    if (c == '|' && peek_next(lexer) == '|') {
        advance(lexer);
        advance(lexer);
        return make_token(lexer, TOKEN_OR_OR, start, line, column);
    }
    if (c == '<' && peek_next(lexer) == '<') {
        advance(lexer);
        advance(lexer);
        return make_token(lexer, TOKEN_LSHIFT, start, line, column);
    }
    if (c == '>' && peek_next(lexer) == '>') {
        advance(lexer);
        advance(lexer);
        return make_token(lexer, TOKEN_RSHIFT, start, line, column);
    }
    if (c == '<' && peek_next(lexer) == '=') {
        advance(lexer);
        advance(lexer);
        return make_token(lexer, TOKEN_LTE, start, line, column);
    }
    if (c == '>' && peek_next(lexer) == '=') {
        advance(lexer);
        advance(lexer);
        return make_token(lexer, TOKEN_GTE, start, line, column);
    }
    if (c == '[' && peek_next(lexer) == '[') {
        advance(lexer);
        advance(lexer);
        return make_token(lexer, TOKEN_LDBRACKET, start, line, column);
    }
    if (c == ']' && peek_next(lexer) == ']') {
        advance(lexer);
        advance(lexer);
        return make_token(lexer, TOKEN_RDBRACKET, start, line, column);
    }

    switch (c) {
        case '+': return make_single(lexer, TOKEN_PLUS, line, column);
        case '-': return make_single(lexer, TOKEN_MINUS, line, column);
        case '*': return make_single(lexer, TOKEN_STAR, line, column);
        case '/': return make_single(lexer, TOKEN_SLASH, line, column);
        case '%': return make_single(lexer, TOKEN_PERCENT, line, column);
        case '!': return make_single(lexer, TOKEN_BANG, line, column);
        case '&': return make_single(lexer, TOKEN_AND, line, column);
        case '|': return make_single(lexer, TOKEN_OR, line, column);
        case '^': return make_single(lexer, TOKEN_XOR, line, column);
        case '~': return make_single(lexer, TOKEN_TILDE, line, column);
        case '?': return make_single(lexer, TOKEN_QUESTION, line, column);
        case '=': return make_single(lexer, TOKEN_ASSIGN, line, column);
        case '<': return make_single(lexer, TOKEN_LT, line, column);
        case '>': return make_single(lexer, TOKEN_GT, line, column);
        case '(': return make_single(lexer, TOKEN_LPAREN, line, column);
        case ')': return make_single(lexer, TOKEN_RPAREN, line, column);
        case '{': return make_single(lexer, TOKEN_LBRACE, line, column);
        case '}': return make_single(lexer, TOKEN_RBRACE, line, column);
        case '[': return make_single(lexer, TOKEN_LBRACKET, line, column);
        case ']': return make_single(lexer, TOKEN_RBRACKET, line, column);
        case '.': return make_single(lexer, TOKEN_DOT, line, column);
        case ',': return make_single(lexer, TOKEN_COMMA, line, column);
        case ';': return make_single(lexer, TOKEN_SEMI, line, column);
        case ':': return make_single(lexer, TOKEN_COLON, line, column);
        default: return make_single(lexer, TOKEN_ERROR, line, column);
    }
}
