#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

// Parser state wraps the lexer and keeps one-token lookahead.
typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    int errors;
} Parser;

// Initializes parsing for a source buffer.
void parser_init(Parser *parser, const char *source);
// Parses a complete Pine translation unit into an AST program node.
ASTNode *parse_program(Parser *parser);
// Returns the number of parse errors collected so far.
int parser_error_count(Parser *parser);

#endif
