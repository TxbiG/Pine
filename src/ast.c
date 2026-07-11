#include "ast.h"

#include <stdlib.h>
#include <string.h>

// Allocates and zero-initializes a node with the given tag.
static ASTNode *make_node(ASTNodeType type) {
    ASTNode *node = calloc(1, sizeof(ASTNode));
    node->type = type;
    return node;
}

ASTNode *ast_set_location(ASTNode *node, int line, int column) {
    if (node) {
        node->line = line;
        node->column = column;
    }
    return node;
}

// Copies parser-owned text into AST-owned storage.
static char *copy_text(const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    memcpy(copy, text, length + 1);
    return copy;
}

ASTNode *ast_make_program(void) {
    return make_node(AST_PROGRAM);
}

ASTNode *ast_make_import_decl(const char *path) {
    ASTNode *node = make_node(AST_IMPORT_DECL);
    node->import_decl.path = copy_text(path);
    return node;
}

ASTNode *ast_make_block(void) {
    return make_node(AST_BLOCK);
}

ASTNode *ast_make_struct_decl(const char *name, ASTNode *fields) {
    ASTNode *node = make_node(AST_STRUCT_DECL);
    node->struct_decl.name = copy_text(name);
    node->struct_decl.fields = fields;
    return node;
}

ASTNode *ast_make_field_decl(const char *field_type, const char *name) {
    ASTNode *node = make_node(AST_FIELD_DECL);
    node->field_decl.field_type = copy_text(field_type);
    node->field_decl.name = copy_text(name);
    return node;
}

ASTNode *ast_make_function(const char *return_type, const char *name, ASTNode *params, ASTNode *body) {
    ASTNode *node = make_node(AST_FUNCTION);
    node->function.return_type = copy_text(return_type);
    node->function.name = copy_text(name);
    node->function.params = params;
    node->function.body = body;
    return node;
}

ASTNode *ast_make_param(const char *param_type, const char *name) {
    ASTNode *node = make_node(AST_PARAM);
    node->param.param_type = copy_text(param_type);
    node->param.name = copy_text(name);
    return node;
}

ASTNode *ast_make_var_decl(const char *var_type, const char *name, size_t array_size, int is_const, ASTNode *value) {
    ASTNode *node = make_node(AST_VAR_DECL);
    node->var_decl.var_type = copy_text(var_type);
    node->var_decl.name = copy_text(name);
    node->var_decl.array_size = array_size;
    node->var_decl.is_const = is_const;
    node->var_decl.value = value;
    return node;
}

ASTNode *ast_make_assign(const char *name, ASTNode *value) {
    ASTNode *node = make_node(AST_ASSIGN_STMT);
    node->assign.name = copy_text(name);
    node->assign.value = value;
    return node;
}

ASTNode *ast_make_expr_stmt(ASTNode *expr) {
    ASTNode *node = make_node(AST_EXPR_STMT);
    node->expr_stmt.expr = expr;
    return node;
}

ASTNode *ast_make_return(ASTNode *value) {
    ASTNode *node = make_node(AST_RETURN_STMT);
    node->ret.value = value;
    return node;
}

ASTNode *ast_make_if(ASTNode *condition, ASTNode *then_block, ASTNode *else_block) {
    ASTNode *node = make_node(AST_IF_STMT);
    node->if_stmt.condition = condition;
    node->if_stmt.then_block = then_block;
    node->if_stmt.else_block = else_block;
    return node;
}

ASTNode *ast_make_while(ASTNode *condition, ASTNode *body) {
    ASTNode *node = make_node(AST_WHILE_STMT);
    node->while_stmt.condition = condition;
    node->while_stmt.body = body;
    return node;
}

ASTNode *ast_make_for(ASTNode *init, ASTNode *condition, ASTNode *step, ASTNode *body) {
    ASTNode *node = make_node(AST_FOR_STMT);
    node->for_stmt.init = init;
    node->for_stmt.condition = condition;
    node->for_stmt.step = step;
    node->for_stmt.body = body;
    return node;
}

ASTNode *ast_make_switch(ASTNode *expr, ASTNode *cases) {
    ASTNode *node = make_node(AST_SWITCH_STMT);
    node->switch_stmt.expr = expr;
    node->switch_stmt.cases = cases;
    return node;
}

ASTNode *ast_make_case(long value, int is_default, ASTNode *body) {
    ASTNode *node = make_node(AST_CASE_STMT);
    node->case_stmt.value = value;
    node->case_stmt.is_default = is_default;
    node->case_stmt.body = body;
    return node;
}

ASTNode *ast_make_break(void) {
    return make_node(AST_BREAK_STMT);
}

ASTNode *ast_make_continue(void) {
    return make_node(AST_CONTINUE_STMT);
}

ASTNode *ast_make_unsafe(ASTNode *body) {
    ASTNode *node = make_node(AST_UNSAFE_BLOCK);
    node->unsafe_block.body = body;
    return node;
}

ASTNode *ast_make_number(long value) {
    ASTNode *node = make_node(AST_NUMBER);
    node->number.value = value;
    return node;
}

ASTNode *ast_make_char_literal(const char *text) {
    ASTNode *node = make_node(AST_CHAR_LITERAL);
    node->literal.text = copy_text(text);
    return node;
}

ASTNode *ast_make_string_literal(const char *text) {
    ASTNode *node = make_node(AST_STRING_LITERAL);
    node->literal.text = copy_text(text);
    return node;
}

ASTNode *ast_make_null_literal(void) {
    return make_node(AST_NULL_LITERAL);
}

ASTNode *ast_make_identifier(const char *name) {
    ASTNode *node = make_node(AST_IDENTIFIER);
    node->identifier.name = copy_text(name);
    return node;
}

ASTNode *ast_make_unary(int op, ASTNode *operand) {
    ASTNode *node = make_node(AST_UNARY_EXPR);
    node->unary.op = op;
    node->unary.operand = operand;
    return node;
}

ASTNode *ast_make_binary(ASTNode *left, int op, ASTNode *right) {
    ASTNode *node = make_node(AST_BINARY_EXPR);
    node->binary.left = left;
    node->binary.op = op;
    node->binary.right = right;
    return node;
}

ASTNode *ast_make_field_expr(ASTNode *object, const char *field) {
    ASTNode *node = make_node(AST_FIELD_EXPR);
    node->field.object = object;
    node->field.field = copy_text(field);
    return node;
}

ASTNode *ast_make_index_expr(ASTNode *object, ASTNode *index) {
    ASTNode *node = make_node(AST_INDEX_EXPR);
    node->index.object = object;
    node->index.index = index;
    return node;
}

ASTNode *ast_make_call(const char *name, ASTNode *args) {
    ASTNode *node = make_node(AST_CALL_EXPR);
    node->call.name = copy_text(name);
    node->call.args = args;
    return node;
}

void ast_list_append(ASTNode *list, ASTNode *item) {
    size_t next = list->list.count + 1;
    list->list.items = realloc(list->list.items, next * sizeof(ASTNode *));
    list->list.items[list->list.count] = item;
    list->list.count = next;
}

void ast_free(ASTNode *node) {
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_PROGRAM:
        case AST_BLOCK:
            for (size_t i = 0; i < node->list.count; i++) {
                ast_free(node->list.items[i]);
            }
            free(node->list.items);
            break;
        case AST_IMPORT_DECL:
            free(node->import_decl.path);
            break;
        case AST_STRUCT_DECL:
            free(node->struct_decl.name);
            ast_free(node->struct_decl.fields);
            break;
        case AST_FIELD_DECL:
            free(node->field_decl.field_type);
            free(node->field_decl.name);
            break;
        case AST_FUNCTION:
            free(node->function.return_type);
            free(node->function.name);
            ast_free(node->function.params);
            ast_free(node->function.body);
            break;
        case AST_PARAM:
            free(node->param.param_type);
            free(node->param.name);
            break;
        case AST_VAR_DECL:
            free(node->var_decl.var_type);
            free(node->var_decl.name);
            ast_free(node->var_decl.value);
            break;
        case AST_ASSIGN_STMT:
            free(node->assign.name);
            ast_free(node->assign.value);
            break;
        case AST_EXPR_STMT:
            ast_free(node->expr_stmt.expr);
            break;
        case AST_RETURN_STMT:
            ast_free(node->ret.value);
            break;
        case AST_IF_STMT:
            ast_free(node->if_stmt.condition);
            ast_free(node->if_stmt.then_block);
            ast_free(node->if_stmt.else_block);
            break;
        case AST_WHILE_STMT:
            ast_free(node->while_stmt.condition);
            ast_free(node->while_stmt.body);
            break;
        case AST_FOR_STMT:
            ast_free(node->for_stmt.init);
            ast_free(node->for_stmt.condition);
            ast_free(node->for_stmt.step);
            ast_free(node->for_stmt.body);
            break;
        case AST_SWITCH_STMT:
            ast_free(node->switch_stmt.expr);
            ast_free(node->switch_stmt.cases);
            break;
        case AST_CASE_STMT:
            ast_free(node->case_stmt.body);
            break;
        case AST_BREAK_STMT:
        case AST_CONTINUE_STMT:
            break;
        case AST_UNSAFE_BLOCK:
            ast_free(node->unsafe_block.body);
            break;
        case AST_IDENTIFIER:
            free(node->identifier.name);
            break;
        case AST_CHAR_LITERAL:
        case AST_STRING_LITERAL:
            free(node->literal.text);
            break;
        case AST_NULL_LITERAL:
            break;
        case AST_UNARY_EXPR:
            ast_free(node->unary.operand);
            break;
        case AST_BINARY_EXPR:
            ast_free(node->binary.left);
            ast_free(node->binary.right);
            break;
        case AST_FIELD_EXPR:
            ast_free(node->field.object);
            free(node->field.field);
            break;
        case AST_INDEX_EXPR:
            ast_free(node->index.object);
            ast_free(node->index.index);
            break;
        case AST_CALL_EXPR:
            free(node->call.name);
            ast_free(node->call.args);
            break;
        case AST_NUMBER:
            break;
    }

    free(node);
}
