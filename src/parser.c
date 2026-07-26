#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Consumes the current token and loads the next token.
static void advance(Parser *parser) {
    parser->previous = parser->current;
    parser->current = lexer_next_token(&parser->lexer);
}

// Tests whether the current token has a particular type.
static int check(Parser *parser, TokenType type) {
    return parser->current.type == type;
}

// Peeks one token ahead by copying lexer state.
static Token peek_next_token(Parser *parser) {
    Lexer lexer = parser->lexer;
    return lexer_next_token(&lexer);
}

// Formats a token for human-readable parse diagnostics.
static void token_description(Token token, char *buffer, size_t size) {
    if (token.type == TOKEN_EOF) {
        snprintf(buffer, size, "end of file");
        return;
    }
    if (token.type == TOKEN_ERROR) {
        snprintf(buffer, size, "invalid character");
        return;
    }

    size_t length = token.length;
    if (length > 24) {
        length = 24;
    }
    snprintf(buffer, size, "'%.*s'", (int)length, token.lexeme);
}

static void print_parse_snippet(Parser *parser, int line, int column) {
    const char *start = parser->source;
    for (int current = 1; current < line && *start; current++) {
        const char *newline = strchr(start, '\n');
        if (!newline) return;
        start = newline + 1;
    }
    const char *end = strchr(start, '\n');
    if (!end) end = start + strlen(start);
    fprintf(stderr, "  %.*s\n  ", (int)(end - start), start);
    for (int i = 1; i < column; i++) fputc(start[i - 1] == '\t' ? '\t' : ' ', stderr);
    fprintf(stderr, "^\n");
}

// Reports a parse error and lets the parser continue where it can.
static void parse_error(Parser *parser, const char *message) {
    char found[48];
    token_description(parser->current, found, sizeof(found));
    fprintf(stderr, "%s:%d:%d: Pine parse error: %s; found %s\n",
            parser->source_path ? parser->source_path : "<input>",
            parser->current.line, parser->current.column, message, found);
    print_parse_snippet(parser, parser->current.line, parser->current.column);
    parser->errors++;
}

// Advances to a likely statement or declaration boundary after an error.
static void synchronize(Parser *parser) {
    while (!check(parser, TOKEN_EOF)) {
        if (parser->previous.type == TOKEN_SEMI || parser->previous.type == TOKEN_RBRACE) {
            return;
        }

        switch (parser->current.type) {
            case TOKEN_CONST:
            case TOKEN_PUB:
            case TOKEN_PRIVATE:
            case TOKEN_STRUCT:
            case TOKEN_ENUM:
            case TOKEN_IMPORT:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_SWITCH:
            case TOKEN_RETURN:
            case TOKEN_BREAK:
            case TOKEN_CONTINUE:
            case TOKEN_UNSAFE:
                return;
            default:
                advance(parser);
                break;
        }
    }
}

// Requires a token type, returns it, and advances.
static Token consume(Parser *parser, TokenType type, const char *message) {
    if (!check(parser, type)) {
        parse_error(parser, message);
        Token missing = {0};
        missing.type = type;
        missing.line = parser->current.line;
        missing.column = parser->current.column;
        return missing;
    }
    Token token = parser->current;
    advance(parser);
    return token;
}

// Copies a token lexeme into a NUL-terminated string.
static char *token_text(Token token) {
    char *text = malloc(token.length + 1);
    memcpy(text, token.lexeme, token.length);
    text[token.length] = '\0';
    return text;
}

// Tags a node with the source position of the token that introduced it.
static const char *active_source_path = "<input>";

static ASTNode *with_location(ASTNode *node, Token token) {
    ast_set_source_file(node, active_source_path);
    return ast_set_location(node, token.line, token.column);
}

// Copies a C string for temporary parser-owned names.
static char *copy_string(const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    memcpy(copy, text, length + 1);
    return copy;
}

// Appends one character to a heap string, used for pointer type spellings.
static char *append_char(char *text, char c) {
    size_t length = strlen(text);
    char *next = realloc(text, length + 2);
    next[length] = c;
    next[length + 1] = '\0';
    return next;
}

// Prepends a short prefix to a heap string.
static char *prepend_text(char *text, const char *prefix) {
    size_t prefix_length = strlen(prefix);
    size_t text_length = strlen(text);
    char *next = malloc(prefix_length + text_length + 1);
    memcpy(next, prefix, prefix_length);
    memcpy(next + prefix_length, text, text_length + 1);
    free(text);
    return next;
}

// Identifies primitive type keywords.
static int is_type_token(TokenType type) {
    switch (type) {
        case TOKEN_U8:
        case TOKEN_U16:
        case TOKEN_U32:
        case TOKEN_U64:
        case TOKEN_I8:
        case TOKEN_I16:
        case TOKEN_I32:
        case TOKEN_I64:
        case TOKEN_BOOL:
        case TOKEN_CHAR:
        case TOKEN_STRING_TYPE:
        case TOKEN_FLOAT:
        case TOKEN_DOUBLE:
        case TOKEN_VOID:
            return 1;
        default:
            return 0;
    }
}

// Detects a declaration start, including user-defined type names.
static int is_type_start(Parser *parser) {
    return is_type_token(parser->current.type) ||
           (check(parser, TOKEN_LBRACKET) && peek_next_token(parser).type == TOKEN_RBRACKET) ||
           (check(parser, TOKEN_IDENT) && peek_next_token(parser).type == TOKEN_IDENT);
}

// Parses a type spelling, including slice prefixes and trailing pointer stars.
static char *parse_type_name(Parser *parser, const char *message) {
    int is_slice = 0;
    if (check(parser, TOKEN_LBRACKET)) {
        advance(parser);
        consume(parser, TOKEN_RBRACKET, "expected ']' in slice type");
        is_slice = 1;
    }

    if (!is_type_token(parser->current.type) && !check(parser, TOKEN_IDENT)) {
        parse_error(parser, message);
    }

    char *type = token_text(parser->current);
    advance(parser);

    if (is_slice) {
        type = prepend_text(type, "[]");
    }

    while (check(parser, TOKEN_STAR)) {
        type = append_char(type, '*');
        advance(parser);
    }

    if (check(parser, TOKEN_QUESTION)) {
        type = append_char(type, '?');
        advance(parser);
    }

    return type;
}

// Forward declarations cover recursive grammar rules.
static ASTNode *parse_expression(Parser *parser);
static ASTNode *parse_statement(Parser *parser);
static ASTNode *parse_block(Parser *parser);
static ASTNode *parse_var_decl(Parser *parser, char *var_type, int is_const);

void parser_init(Parser *parser, const char *source) {
    lexer_init(&parser->lexer, source);
    parser->previous = (Token){0};
    parser->current = lexer_next_token(&parser->lexer);
    parser->source = source;
    parser->source_path = "<input>";
    parser->errors = 0;
}

void parser_set_source_path(Parser *parser, const char *source_path) {
    parser->source_path = source_path ? source_path : "<input>";
    active_source_path = parser->source_path;
}

int parser_error_count(Parser *parser) {
    return parser->errors;
}

// Parses comma-separated call arguments.
static ASTNode *parse_arg_list(Parser *parser) {
    ASTNode *args = ast_make_block();

    if (!check(parser, TOKEN_RPAREN)) {
        do {
            ast_list_append(args, parse_expression(parser));
            if (!check(parser, TOKEN_COMMA)) {
                break;
            }
            advance(parser);
        } while (!check(parser, TOKEN_RPAREN));
    }

    return args;
}

// Parses literals, identifiers, calls, grouping, and postfix operations.
static ASTNode *parse_primary(Parser *parser) {
    ASTNode *expr = NULL;

    if (check(parser, TOKEN_NUMBER)) {
        Token token = parser->current;
        char *text = token_text(token);
        long long value = strtoll(text, NULL, 10);
        free(text);
        advance(parser);
        expr = with_location(ast_make_number(value), token);
    } else if (check(parser, TOKEN_CHAR_LITERAL)) {
        Token token = parser->current;
        char *text = token_text(token);
        advance(parser);
        expr = with_location(ast_make_char_literal(text), token);
        free(text);
    } else if (check(parser, TOKEN_STRING)) {
        Token token = parser->current;
        char *text = token_text(token);
        advance(parser);
        expr = with_location(ast_make_string_literal(text), token);
        free(text);
    } else if (check(parser, TOKEN_NULL)) {
        Token token = parser->current;
        advance(parser);
        expr = with_location(ast_make_null_literal(), token);
    } else if (check(parser, TOKEN_TRUE) || check(parser, TOKEN_FALSE)) {
        Token token = parser->current;
        int value = check(parser, TOKEN_TRUE);
        advance(parser);
        expr = with_location(ast_make_bool_literal(value), token);
    } else if (check(parser, TOKEN_LBRACKET)) {
        Token token = parser->current;
        advance(parser);
        ASTNode *elements = ast_make_block();
        if (!check(parser, TOKEN_RBRACKET)) {
            do {
                ast_list_append(elements, parse_expression(parser));
                if (!check(parser, TOKEN_COMMA)) break;
                advance(parser);
            } while (!check(parser, TOKEN_RBRACKET));
        }
        consume(parser, TOKEN_RBRACKET, "expected ']' after array initializer");
        expr = with_location(ast_make_array_literal(elements), token);
    } else if (check(parser, TOKEN_IDENT)) {
        Token ident_token = parser->current;
        char *name = token_text(ident_token);
        advance(parser);

        if (check(parser, TOKEN_LPAREN)) {
            advance(parser);
            ASTNode *args = parse_arg_list(parser);
            consume(parser, TOKEN_RPAREN, "expected ')' after function call arguments");
            expr = with_location(ast_make_call(name, args), ident_token);
        } else if (check(parser, TOKEN_LBRACE)) {
            advance(parser);
            ASTNode *fields = ast_make_block();
            while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
                Token field_token = consume(parser, TOKEN_IDENT, "expected struct literal field name");
                char *field_name = token_text(field_token);
                consume(parser, TOKEN_COLON, "expected ':' after struct literal field name");
                ast_list_append(fields, with_location(ast_make_field_init(field_name, parse_expression(parser)), field_token));
                free(field_name);
                if (!check(parser, TOKEN_COMMA)) break;
                advance(parser);
            }
            consume(parser, TOKEN_RBRACE, "expected '}' after struct literal");
            expr = with_location(ast_make_struct_literal(name, fields), ident_token);
        } else {
            expr = with_location(ast_make_identifier(name), ident_token);
        }
        free(name);
    } else if (check(parser, TOKEN_LPAREN)) {
        advance(parser);
        expr = parse_expression(parser);
        consume(parser, TOKEN_RPAREN, "expected ')' after expression");
    } else {
        parse_error(parser, "expected expression");
        if (!check(parser, TOKEN_EOF)) {
            advance(parser);
        }
        return with_location(ast_make_number(0), parser->previous);
    }

    while (check(parser, TOKEN_DOT) || check(parser, TOKEN_LBRACKET)) {
        if (check(parser, TOKEN_DOT)) {
            Token op_token = parser->current;
            advance(parser);
            Token field_token = consume(parser, TOKEN_IDENT, "expected field name after '.'");
            char *field = token_text(field_token);
            expr = with_location(ast_make_field_expr(expr, field), op_token);
            free(field);
        } else {
            Token op_token = parser->current;
            advance(parser);
            ASTNode *index = parse_expression(parser);
            consume(parser, TOKEN_RBRACKET, "expected ']' after index expression");
            expr = with_location(ast_make_index_expr(expr, index), op_token);
        }
    }

    return expr;
}

// Parses prefix unary operators.
static ASTNode *parse_unary(Parser *parser) {
    if (check(parser, TOKEN_MINUS) || check(parser, TOKEN_BANG) ||
        check(parser, TOKEN_TILDE) || check(parser, TOKEN_STAR) ||
        check(parser, TOKEN_AND)) {
        Token op_token = parser->current;
        TokenType op = op_token.type;
        advance(parser);
        return with_location(ast_make_unary(op, parse_unary(parser)), op_token);
    }

    return parse_primary(parser);
}

// Parses multiplicative operators.
static ASTNode *parse_factor(Parser *parser) {
    ASTNode *expr = parse_unary(parser);

    while (check(parser, TOKEN_STAR) || check(parser, TOKEN_SLASH) || check(parser, TOKEN_PERCENT)) {
        Token op_token = parser->current;
        TokenType op = op_token.type;
        advance(parser);
        expr = with_location(ast_make_binary(expr, op, parse_unary(parser)), op_token);
    }

    return expr;
}

// Parses additive operators.
static ASTNode *parse_term(Parser *parser) {
    ASTNode *expr = parse_factor(parser);

    while (check(parser, TOKEN_PLUS) || check(parser, TOKEN_MINUS)) {
        Token op_token = parser->current;
        TokenType op = op_token.type;
        advance(parser);
        expr = with_location(ast_make_binary(expr, op, parse_factor(parser)), op_token);
    }

    return expr;
}

// Parses bit shifts.
static ASTNode *parse_shift(Parser *parser) {
    ASTNode *expr = parse_term(parser);

    while (check(parser, TOKEN_LSHIFT) || check(parser, TOKEN_RSHIFT)) {
        Token op_token = parser->current;
        TokenType op = op_token.type;
        advance(parser);
        expr = with_location(ast_make_binary(expr, op, parse_term(parser)), op_token);
    }

    return expr;
}

// Parses equality and ordered comparisons.
static ASTNode *parse_comparison(Parser *parser) {
    ASTNode *expr = parse_shift(parser);

    while (check(parser, TOKEN_LT) || check(parser, TOKEN_LTE) ||
           check(parser, TOKEN_GT) || check(parser, TOKEN_GTE) ||
           check(parser, TOKEN_EQ) || check(parser, TOKEN_NEQ)) {
        Token op_token = parser->current;
        TokenType op = op_token.type;
        advance(parser);
        expr = with_location(ast_make_binary(expr, op, parse_shift(parser)), op_token);
    }

    return expr;
}

// Parses bitwise AND.
static ASTNode *parse_bitwise_and(Parser *parser) {
    ASTNode *expr = parse_comparison(parser);

    while (check(parser, TOKEN_AND)) {
        Token op_token = parser->current;
        TokenType op = op_token.type;
        advance(parser);
        expr = with_location(ast_make_binary(expr, op, parse_comparison(parser)), op_token);
    }

    return expr;
}

// Parses bitwise XOR.
static ASTNode *parse_bitwise_xor(Parser *parser) {
    ASTNode *expr = parse_bitwise_and(parser);

    while (check(parser, TOKEN_XOR)) {
        Token op_token = parser->current;
        TokenType op = op_token.type;
        advance(parser);
        expr = with_location(ast_make_binary(expr, op, parse_bitwise_and(parser)), op_token);
    }

    return expr;
}

// Parses bitwise OR.
static ASTNode *parse_bitwise_or(Parser *parser) {
    ASTNode *expr = parse_bitwise_xor(parser);

    while (check(parser, TOKEN_OR)) {
        Token op_token = parser->current;
        TokenType op = op_token.type;
        advance(parser);
        expr = with_location(ast_make_binary(expr, op, parse_bitwise_xor(parser)), op_token);
    }

    return expr;
}

// Parses logical AND.
static ASTNode *parse_logical_and(Parser *parser) {
    ASTNode *expr = parse_bitwise_or(parser);

    while (check(parser, TOKEN_AND_AND)) {
        Token op_token = parser->current;
        TokenType op = op_token.type;
        advance(parser);
        expr = with_location(ast_make_binary(expr, op, parse_bitwise_or(parser)), op_token);
    }

    return expr;
}

// Parses logical OR.
static ASTNode *parse_logical_or(Parser *parser) {
    ASTNode *expr = parse_logical_and(parser);

    while (check(parser, TOKEN_OR_OR)) {
        Token op_token = parser->current;
        TokenType op = op_token.type;
        advance(parser);
        expr = with_location(ast_make_binary(expr, op, parse_logical_and(parser)), op_token);
    }

    return expr;
}

// Parses the lowest-precedence expression level.
static ASTNode *parse_expression(Parser *parser) {
    return parse_logical_or(parser);
}

// Parses a variable declaration after the type has already been consumed.
static ASTNode *parse_var_decl(Parser *parser, char *var_type, int is_const) {
    Token name_token = consume(parser, TOKEN_IDENT, "expected variable name");
    char *name = token_text(name_token);
    size_t array_size = 0;
    ASTNode *value = NULL;

    if (check(parser, TOKEN_LBRACKET)) {
        advance(parser);
        Token size_token = consume(parser, TOKEN_NUMBER, "expected array size");
        char *size_text = token_text(size_token);
        long long parsed_size = strtoll(size_text, NULL, 10);
        free(size_text);

        if (parsed_size <= 0) {
            parse_error(parser, "array size must be greater than zero");
            parsed_size = 1;
        }

        array_size = (size_t)parsed_size;
        consume(parser, TOKEN_RBRACKET, "expected ']' after array size");
    }

    if (check(parser, TOKEN_ASSIGN)) {
        advance(parser);
        value = parse_expression(parser);
    }

    consume(parser, TOKEN_SEMI, "expected ';' after variable declaration");
    ASTNode *decl = with_location(ast_make_var_decl(var_type, name, array_size, is_const, value), name_token);
    free(name);
    return decl;
}

// Parses a top-level enum declaration with optional explicit integer values.
static ASTNode *parse_enum_decl(Parser *parser) {
    Token enum_token = consume(parser, TOKEN_ENUM, "expected 'enum'");
    Token name_token = consume(parser, TOKEN_IDENT, "expected enum name");
    char *name = token_text(name_token);
    ASTNode *values = ast_make_block();
    long next_value = 0;
    consume(parser, TOKEN_LBRACE, "expected '{' after enum name");
    while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
        Token value_token = consume(parser, TOKEN_IDENT, "expected enum value name");
        char *value_name = token_text(value_token);
        long long value = next_value;
        if (check(parser, TOKEN_ASSIGN)) {
            advance(parser);
            Token number = consume(parser, TOKEN_NUMBER, "expected integer enum value");
            char *number_text = token_text(number);
            value = strtoll(number_text, NULL, 10);
            free(number_text);
        }
        ast_list_append(values, with_location(ast_make_enum_value(value_name, value), value_token));
        free(value_name);
        next_value = value + 1;
        if (!check(parser, TOKEN_COMMA)) break;
        advance(parser);
    }
    consume(parser, TOKEN_RBRACE, "expected '}' after enum values");
    ASTNode *decl = with_location(ast_make_enum_decl(name, values), enum_token);
    free(name);
    return decl;
}

// Parses a top-level struct declaration.
static ASTNode *parse_struct_decl(Parser *parser) {
    consume(parser, TOKEN_STRUCT, "expected 'struct'");
    Token name_token = consume(parser, TOKEN_IDENT, "expected struct name");
    char *name = token_text(name_token);
    ASTNode *fields = ast_make_block();

    consume(parser, TOKEN_LBRACE, "expected '{' after struct name");
    while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
        char *field_type = parse_type_name(parser, "expected field type");
        Token field_name_token = consume(parser, TOKEN_IDENT, "expected field name");
        char *field_name = token_text(field_name_token);
        consume(parser, TOKEN_SEMI, "expected ';' after field declaration");
        ast_list_append(fields, ast_make_field_decl(field_type, field_name));
        free(field_type);
        free(field_name);
    }
    consume(parser, TOKEN_RBRACE, "expected '}' after struct fields");

    ASTNode *decl = with_location(ast_make_struct_decl(name, fields), name_token);
    free(name);
    return decl;
}

// Parses a return statement.
static ASTNode *parse_return(Parser *parser) {
    Token return_token = consume(parser, TOKEN_RETURN, "expected 'return'");
    ASTNode *value = NULL;
    if (!check(parser, TOKEN_SEMI)) {
        value = parse_expression(parser);
    }
    consume(parser, TOKEN_SEMI, "expected ';' after return");
    return with_location(ast_make_return(value), return_token);
}

// Parses a while loop.
static ASTNode *parse_while(Parser *parser) {
    Token while_token = consume(parser, TOKEN_WHILE, "expected 'while'");
    consume(parser, TOKEN_LPAREN, "expected '(' after while");
    ASTNode *condition = parse_expression(parser);
    consume(parser, TOKEN_RPAREN, "expected ')' after while condition");
    return with_location(ast_make_while(condition, parse_block(parser)), while_token);
}

// Parses either a simple assignment or an expression statement payload.
static ASTNode *parse_assignment_or_expr(Parser *parser, const char *message) {
    ASTNode *expr = parse_expression(parser);

    if (check(parser, TOKEN_ASSIGN)) {
        int valid_target = expr->type == AST_IDENTIFIER ||
                           expr->type == AST_FIELD_EXPR ||
                           expr->type == AST_INDEX_EXPR ||
                           (expr->type == AST_UNARY_EXPR && expr->unary.op == TOKEN_STAR);
        if (!valid_target) {
            parse_error(parser, message);
        }
        advance(parser);
        ASTNode *value = parse_expression(parser);
        return ast_set_location(ast_make_assign(expr, value), expr->line, expr->column);
    }

    return ast_set_location(ast_make_expr_stmt(expr), expr->line, expr->column);
}

// Parses a C-style for loop.
static ASTNode *parse_for(Parser *parser) {
    Token for_token = consume(parser, TOKEN_FOR, "expected 'for'");
    consume(parser, TOKEN_LPAREN, "expected '(' after for");

    ASTNode *init = NULL;
    ASTNode *condition = NULL;
    ASTNode *step = NULL;

    if (check(parser, TOKEN_SEMI)) {
        advance(parser);
    } else if (is_type_start(parser)) {
        char *type = parse_type_name(parser, "expected for initializer type");
        init = parse_var_decl(parser, type, 0);
        free(type);
    } else {
        init = parse_assignment_or_expr(parser, "invalid assignment in for initializer");
        consume(parser, TOKEN_SEMI, "expected ';' after for initializer");
    }

    if (!check(parser, TOKEN_SEMI)) {
        condition = parse_expression(parser);
    }
    consume(parser, TOKEN_SEMI, "expected ';' after for condition");

    if (!check(parser, TOKEN_RPAREN)) {
        step = parse_assignment_or_expr(parser, "invalid assignment in for step");
    }
    consume(parser, TOKEN_RPAREN, "expected ')' after for clauses");

    return with_location(ast_make_for(init, condition, step, parse_block(parser)), for_token);
}

// Parses a switch statement and its case/default labels.
static ASTNode *parse_switch(Parser *parser) {
    Token switch_token = consume(parser, TOKEN_SWITCH, "expected 'switch'");
    consume(parser, TOKEN_LPAREN, "expected '(' after switch");
    ASTNode *expr = parse_expression(parser);
    consume(parser, TOKEN_RPAREN, "expected ')' after switch expression");
    consume(parser, TOKEN_LBRACE, "expected '{' after switch expression");

    ASTNode *cases = ast_make_block();
    while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
        int is_default = 0;
        long long value = 0;
        Token label_token = parser->current;

        if (check(parser, TOKEN_CASE)) {
            advance(parser);
            Token value_token = consume(parser, TOKEN_NUMBER, "expected number after case");
            label_token = value_token;
            char *text = token_text(value_token);
            value = strtoll(text, NULL, 10);
            free(text);
        } else if (check(parser, TOKEN_DEFAULT)) {
            is_default = 1;
            advance(parser);
        } else {
            parse_error(parser, "expected case or default label");
            synchronize(parser);
            break;
        }

        consume(parser, TOKEN_COLON, "expected ':' after switch label");
        ASTNode *body = ast_make_block();
        while (!check(parser, TOKEN_CASE) && !check(parser, TOKEN_DEFAULT) &&
               !check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
            ast_list_append(body, parse_statement(parser));
        }
        ast_list_append(cases, with_location(ast_make_case(value, is_default, body), label_token));
    }

    consume(parser, TOKEN_RBRACE, "expected '}' after switch cases");
    return with_location(ast_make_switch(expr, cases), switch_token);
}

// Parses an unsafe block.
static ASTNode *parse_unsafe(Parser *parser) {
    Token unsafe_token = consume(parser, TOKEN_UNSAFE, "expected 'unsafe'");
    return with_location(ast_make_unsafe(parse_block(parser)), unsafe_token);
}

// Parses if/else control flow.
static ASTNode *parse_if(Parser *parser) {
    Token if_token = consume(parser, TOKEN_IF, "expected 'if'");
    consume(parser, TOKEN_LPAREN, "expected '(' after if");
    ASTNode *condition = parse_expression(parser);
    consume(parser, TOKEN_RPAREN, "expected ')' after if condition");

    ASTNode *then_block = parse_block(parser);
    ASTNode *else_block = NULL;

    if (check(parser, TOKEN_ELSE)) {
        advance(parser);
        else_block = parse_block(parser);
    }

    return with_location(ast_make_if(condition, then_block, else_block), if_token);
}

// Parses a standalone assignment or expression followed by a semicolon.
static ASTNode *parse_expression_statement(Parser *parser) {
    ASTNode *stmt = parse_assignment_or_expr(parser, "invalid assignment statement");
    consume(parser, TOKEN_SEMI, "expected ';' after expression");
    return stmt;
}

// Dispatches to the correct statement parser for the current token.
static ASTNode *parse_statement(Parser *parser) {
    if (check(parser, TOKEN_RETURN)) {
        return parse_return(parser);
    }

    if (check(parser, TOKEN_IF)) {
        return parse_if(parser);
    }

    if (check(parser, TOKEN_WHILE)) {
        return parse_while(parser);
    }

    if (check(parser, TOKEN_FOR)) {
        return parse_for(parser);
    }

    if (check(parser, TOKEN_SWITCH)) {
        return parse_switch(parser);
    }

    if (check(parser, TOKEN_BREAK)) {
        Token break_token = parser->current;
        advance(parser);
        consume(parser, TOKEN_SEMI, "expected ';' after break");
        return with_location(ast_make_break(), break_token);
    }

    if (check(parser, TOKEN_CONTINUE)) {
        Token continue_token = parser->current;
        advance(parser);
        consume(parser, TOKEN_SEMI, "expected ';' after continue");
        return with_location(ast_make_continue(), continue_token);
    }

    if (check(parser, TOKEN_UNSAFE)) {
        return parse_unsafe(parser);
    }

    if (is_type_start(parser)) {
        char *type = parse_type_name(parser, "expected type");
        ASTNode *decl = parse_var_decl(parser, type, 0);
        free(type);
        return decl;
    }

    return parse_expression_statement(parser);
}

// Parses a braced list of statements.
static ASTNode *parse_block(Parser *parser) {
    Token brace_token = consume(parser, TOKEN_LBRACE, "expected '{'");
    ASTNode *block = with_location(ast_make_block(), brace_token);

    while (!check(parser, TOKEN_RBRACE) && !check(parser, TOKEN_EOF)) {
        ast_list_append(block, parse_statement(parser));
    }

    consume(parser, TOKEN_RBRACE, "expected '}'");
    return block;
}

// Parses function parameters.
static ASTNode *parse_params(Parser *parser) {
    ASTNode *params = ast_make_block();

    if (!check(parser, TOKEN_RPAREN)) {
        do {
            char *type = parse_type_name(parser, "expected parameter type");

            Token name_token = consume(parser, TOKEN_IDENT, "expected parameter name");
            char *name = token_text(name_token);
            size_t array_size = 0;
            if (check(parser, TOKEN_LBRACKET)) {
                advance(parser);
                Token size_token = consume(parser, TOKEN_NUMBER, "expected parameter array size");
                char *size_text = token_text(size_token);
                long long parsed_size = strtoll(size_text, NULL, 10);
                free(size_text);
                if (parsed_size <= 0) {
                    parse_error(parser, "parameter array size must be greater than zero");
                    parsed_size = 1;
                }
                array_size = (size_t)parsed_size;
                consume(parser, TOKEN_RBRACKET, "expected ']' after parameter array size");
            }
            ast_list_append(params, with_location(ast_make_param(type, name, array_size), name_token));

            free(type);
            free(name);

            if (!check(parser, TOKEN_COMMA)) {
                break;
            }
            advance(parser);
        } while (!check(parser, TOKEN_RPAREN));
    }

    return params;
}

// Parses a function after its return type and name have been consumed.
static ASTNode *parse_function_after_name(Parser *parser, char *type_text, Token name) {
    consume(parser, TOKEN_LPAREN, "expected '(' after function name");
    ASTNode *params = parse_params(parser);
    consume(parser, TOKEN_RPAREN, "expected ')' after function parameters");

    char *name_text = token_text(name);
    ASTNode *body = parse_block(parser);
    ASTNode *fn = with_location(ast_make_function(type_text, name_text, params, body), name);

    free(type_text);
    free(name_text);
    return fn;
}

// Parses `import foo.bar;` into a dotted module path.
static ASTNode *parse_import(Parser *parser) {
    Token import_token = consume(parser, TOKEN_IMPORT, "expected 'import'");
    Token first = consume(parser, TOKEN_IDENT, "expected module name after import");
    char *path = token_text(first);

    while (check(parser, TOKEN_DOT)) {
        advance(parser);
        Token part = consume(parser, TOKEN_IDENT, "expected module path segment after '.'");
        char *part_text = token_text(part);
        path = append_char(path, '.');
        size_t path_length = strlen(path);
        size_t part_length = strlen(part_text);
        char *next = realloc(path, path_length + part_length + 1);
        memcpy(next + path_length, part_text, part_length + 1);
        path = next;
        free(part_text);
    }

    consume(parser, TOKEN_SEMI, "expected ';' after import");
    ASTNode *import = with_location(ast_make_import_decl(path), import_token);
    free(path);
    return import;
}

// Parses a top-level function, global variable, or global constant.
static ASTNode *parse_global_or_function(Parser *parser) {
    int is_const = 0;
    if (check(parser, TOKEN_CONST)) {
        is_const = 1;
        advance(parser);
    }

    char *type_text = parse_type_name(parser, "expected type");
    Token name = consume(parser, TOKEN_IDENT, "expected name");

    if (!is_const && check(parser, TOKEN_LPAREN)) {
        return parse_function_after_name(parser, type_text, name);
    }

    char *name_text = token_text(name);
    size_t array_size = 0;
    ASTNode *value = NULL;

    if (check(parser, TOKEN_LBRACKET)) {
        advance(parser);
        Token size_token = consume(parser, TOKEN_NUMBER, "expected array size");
        char *size_text = token_text(size_token);
        long long parsed_size = strtoll(size_text, NULL, 10);
        free(size_text);

        if (parsed_size <= 0) {
            parse_error(parser, "array size must be greater than zero");
            parsed_size = 1;
        }

        array_size = (size_t)parsed_size;
        consume(parser, TOKEN_RBRACKET, "expected ']' after array size");
    }

    if (check(parser, TOKEN_ASSIGN)) {
        advance(parser);
        value = parse_expression(parser);
    }

    consume(parser, TOKEN_SEMI, "expected ';' after global declaration");
    ASTNode *decl = with_location(ast_make_var_decl(type_text, name_text, array_size, is_const, value), name);
    free(type_text);
    free(name_text);
    return decl;
}

ASTNode *parse_program(Parser *parser) {
    ASTNode *program = ast_make_program();

    while (!check(parser, TOKEN_EOF)) {
        if (check(parser, TOKEN_ERROR)) {
            parse_error(parser, "unexpected or unterminated token");
            advance(parser);
            synchronize(parser);
            continue;
        }

        int is_public = 0;
        if (check(parser, TOKEN_PUB) || check(parser, TOKEN_PRIVATE)) {
            is_public = check(parser, TOKEN_PUB);
            advance(parser);
        }

        ASTNode *decl = NULL;
        if (check(parser, TOKEN_ENUM)) {
            decl = parse_enum_decl(parser);
        } else if (check(parser, TOKEN_STRUCT)) {
            decl = parse_struct_decl(parser);
        } else if (check(parser, TOKEN_IMPORT)) {
            if (is_public) parse_error(parser, "imports cannot be public");
            decl = parse_import(parser);
        } else if (check(parser, TOKEN_CONST) || is_type_token(parser->current.type) || check(parser, TOKEN_IDENT)) {
            decl = parse_global_or_function(parser);
        } else {
            parse_error(parser, "expected declaration");
            advance(parser);
            synchronize(parser);
            continue;
        }

        ast_set_public(decl, is_public);
        ast_list_append(program, decl);
        if (parser->errors > 0) synchronize(parser);
    }

    return program;
}
