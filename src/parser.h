#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

// Parser state wraps the lexer and keeps one-token lookahead.
typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    const char *source;
    const char *source_path;
    int errors;
} Parser;

// Initializes parsing for a source buffer.
void parser_init(Parser *parser, const char *source);
void parser_set_source_path(Parser *parser, const char *source_path);
// Parses a complete Pine translation unit into an AST program node.
ASTNode *parse_program(Parser *parser);
// Returns the number of parse errors collected so far.
int parser_error_count(Parser *parser);

#endif
