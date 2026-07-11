#ifndef AST_H
#define AST_H

#include <stddef.h>

typedef enum {
    AST_PROGRAM,
    AST_IMPORT_DECL,
    AST_STRUCT_DECL,
    AST_FIELD_DECL,
    AST_FUNCTION,
    AST_PARAM,
    AST_BLOCK,
    AST_VAR_DECL,
    AST_ASSIGN_STMT,
    AST_EXPR_STMT,
    AST_RETURN_STMT,
    AST_IF_STMT,
    AST_WHILE_STMT,
    AST_FOR_STMT,
    AST_SWITCH_STMT,
    AST_CASE_STMT,
    AST_BREAK_STMT,
    AST_CONTINUE_STMT,
    AST_UNSAFE_BLOCK,
    AST_NUMBER,
    AST_CHAR_LITERAL,
    AST_STRING_LITERAL,
    AST_NULL_LITERAL,
    AST_IDENTIFIER,
    AST_UNARY_EXPR,
    AST_BINARY_EXPR,
    AST_FIELD_EXPR,
    AST_INDEX_EXPR,
    AST_CALL_EXPR
} ASTNodeType;

// Forward declaration so AST nodes can refer to child nodes recursively.
typedef struct ASTNode ASTNode;

// Tagged union for every syntax tree node Pine currently understands.
// The active union member is selected by `type`; `line` and `column`
// are best-effort source locations used by diagnostics.
struct ASTNode {
    ASTNodeType type;
    int line;
    int column;
    union {
        struct {
            ASTNode **items;
            size_t count;
        } list;
        struct {
            char *path;
        } import_decl;
        struct {
            char *name;
            ASTNode *fields;
        } struct_decl;
        struct {
            char *field_type;
            char *name;
        } field_decl;
        struct {
            char *return_type;
            char *name;
            ASTNode *params;
            ASTNode *body;
        } function;
        struct {
            char *param_type;
            char *name;
        } param;
        struct {
            char *var_type;
            char *name;
            size_t array_size;
            int is_const;
            ASTNode *value;
        } var_decl;
        struct {
            char *name;
            ASTNode *value;
        } assign;
        struct {
            ASTNode *expr;
        } expr_stmt;
        struct {
            ASTNode *value;
        } ret;
        struct {
            ASTNode *condition;
            ASTNode *then_block;
            ASTNode *else_block;
        } if_stmt;
        struct {
            ASTNode *condition;
            ASTNode *body;
        } while_stmt;
        struct {
            ASTNode *init;
            ASTNode *condition;
            ASTNode *step;
            ASTNode *body;
        } for_stmt;
        struct {
            ASTNode *expr;
            ASTNode *cases;
        } switch_stmt;
        struct {
            long value;
            int is_default;
            ASTNode *body;
        } case_stmt;
        struct {
            ASTNode *body;
        } unsafe_block;
        struct {
            long value;
        } number;
        struct {
            char *text;
        } literal;
        struct {
            char *name;
        } identifier;
        struct {
            int op;
            ASTNode *operand;
        } unary;
        struct {
            ASTNode *left;
            int op;
            ASTNode *right;
        } binary;
        struct {
            ASTNode *object;
            char *field;
        } field;
        struct {
            ASTNode *object;
            ASTNode *index;
            size_t checked_length;
            int checked_is_slice;
        } index;
        struct {
            char *name;
            ASTNode *args;
        } call;
    };
};

// Creates a top-level program list node.
ASTNode *ast_make_program(void);
// Creates a top-level import declaration for a dotted module path.
ASTNode *ast_make_import_decl(const char *path);
// Creates a generic list/block node used for statement lists, params, and cases.
ASTNode *ast_make_block(void);
// Creates a top-level struct declaration with a list of field declarations.
ASTNode *ast_make_struct_decl(const char *name, ASTNode *fields);
// Creates one field entry inside a struct declaration.
ASTNode *ast_make_field_decl(const char *field_type, const char *name);
// Creates a function declaration with params and a body block.
ASTNode *ast_make_function(const char *return_type, const char *name, ASTNode *params, ASTNode *body);
// Creates a function parameter declaration.
ASTNode *ast_make_param(const char *param_type, const char *name);
// Creates a local or global variable declaration, optionally const or fixed-array.
ASTNode *ast_make_var_decl(const char *var_type, const char *name, size_t array_size, int is_const, ASTNode *value);
// Creates an assignment statement targeting a named variable.
ASTNode *ast_make_assign(const char *name, ASTNode *value);
// Wraps an expression so it can appear as a statement.
ASTNode *ast_make_expr_stmt(ASTNode *expr);
// Creates a return statement.
ASTNode *ast_make_return(ASTNode *value);
// Creates an if/else statement; `else_block` may be NULL.
ASTNode *ast_make_if(ASTNode *condition, ASTNode *then_block, ASTNode *else_block);
// Creates a while loop statement.
ASTNode *ast_make_while(ASTNode *condition, ASTNode *body);
// Creates a C-style for loop with optional init, condition, and step nodes.
ASTNode *ast_make_for(ASTNode *init, ASTNode *condition, ASTNode *step, ASTNode *body);
// Creates a switch statement with a list of case/default nodes.
ASTNode *ast_make_switch(ASTNode *expr, ASTNode *cases);
// Creates a switch case or default block; `is_default` selects default.
ASTNode *ast_make_case(long value, int is_default, ASTNode *body);
// Creates loop/switch control flow statements.
ASTNode *ast_make_break(void);
ASTNode *ast_make_continue(void);
// Creates an unsafe block node, used by semantic checks for pointer dereference.
ASTNode *ast_make_unsafe(ASTNode *body);
// Creates literal expression nodes.
ASTNode *ast_make_number(long value);
ASTNode *ast_make_char_literal(const char *text);
ASTNode *ast_make_string_literal(const char *text);
// Creates a null literal expression.
ASTNode *ast_make_null_literal(void);
// Creates a name reference expression.
ASTNode *ast_make_identifier(const char *name);
// Creates unary and binary operator expressions. Operator values are TokenType ints.
ASTNode *ast_make_unary(int op, ASTNode *operand);
ASTNode *ast_make_binary(ASTNode *left, int op, ASTNode *right);
// Creates postfix expressions for field and fixed-array access.
ASTNode *ast_make_field_expr(ASTNode *object, const char *field);
ASTNode *ast_make_index_expr(ASTNode *object, ASTNode *index);
// Creates a function call expression with an argument list node.
ASTNode *ast_make_call(const char *name, ASTNode *args);
// Attaches source coordinates to a node and returns the same node.
ASTNode *ast_set_location(ASTNode *node, int line, int column);
// Appends an item to any AST list/block node.
void ast_list_append(ASTNode *list, ASTNode *item);
// Recursively releases an AST node and all owned children/strings.
void ast_free(ASTNode *node);

#endif
