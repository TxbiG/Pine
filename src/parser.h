#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

// ===== Parser State =====
typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
} Parser;

// ===== API =====
void parser_init(Parser *parser, const char *source);
ASTNode *parse_program(Parser *parser);

#endif
