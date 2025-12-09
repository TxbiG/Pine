#ifndef AST_H
#define AST_H

#include <stddef.h>

// ===== Node Types =====
typedef enum {
    AST_PROGRAM,
    AST_NUMBER,
    AST_STRING,
    AST_IDENTIFIER,
    AST_BINARY_EXPR,
    AST_VAR_DECL,
    AST_RETURN_STMT
} ASTNodeType;

// Forward declaration
typedef struct ASTNode ASTNode;

// ===== AST Node Struct =====
struct ASTNode {
    ASTNodeType type;

    union {
        // Literals
        struct {
            long value;
        } number;

        struct {
            char *name;
        } identifier;

        // Binary expressions: a + b, a * b, etc.
        struct {
            ASTNode *left;
            ASTNode *right;
            int op;             // token type or custom op enum
        } binary;

        // Variable declaration
        struct {
            char *name;
            ASTNode *value;
        } var_decl;

        // Return statement
        struct {
            ASTNode *value;
        } ret;
    };
};

// ===== AST API =====
ASTNode *ast_make_number(long value);
ASTNode *ast_make_identifier(const char *name);
ASTNode *ast_make_binary(ASTNode *left, int op, ASTNode *right);
ASTNode *ast_make_var_decl(const char *name, ASTNode *value);
ASTNode *ast_make_return(ASTNode *value);
void ast_free(ASTNode *node);

#endif
