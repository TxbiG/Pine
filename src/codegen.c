#include "codegen.h"

#include "lexer.h"

#include <string.h>

// Maps a Pine base type spelling to the corresponding C type spelling.
static const char *c_type(const char *pine_type) {
    if (strcmp(pine_type, "u8") == 0) return "uint8_t";
    if (strcmp(pine_type, "u16") == 0) return "uint16_t";
    if (strcmp(pine_type, "u32") == 0) return "uint32_t";
    if (strcmp(pine_type, "u64") == 0) return "uint64_t";
    if (strcmp(pine_type, "i8") == 0) return "int8_t";
    if (strcmp(pine_type, "i16") == 0) return "int16_t";
    if (strcmp(pine_type, "i32") == 0) return "int32_t";
    if (strcmp(pine_type, "i64") == 0) return "int64_t";
    if (strcmp(pine_type, "bool") == 0) return "bool";
    if (strcmp(pine_type, "char") == 0) return "char";
    if (strcmp(pine_type, "string") == 0) return "char*";
    if (strcmp(pine_type, "float") == 0) return "float";
    if (strcmp(pine_type, "double") == 0) return "double";
    if (strcmp(pine_type, "void") == 0) return "void";
    return pine_type;
}

// Emits the C typedef name used for a Pine slice type.
static void emit_slice_type_name(const char *pine_type, FILE *out) {
    fprintf(out, "pine_slice_");
    for (const char *c = pine_type + 2; *c; c++) {
        if (*c == '*') {
            fprintf(out, "ptr");
        } else {
            fputc(*c, out);
        }
    }
}

// Emits a possibly pointer-qualified Pine type as C.
static void emit_c_type(const char *pine_type, FILE *out) {
    if (strncmp(pine_type, "[]", 2) == 0) {
        emit_slice_type_name(pine_type, out);
        return;
    }

    size_t length = strlen(pine_type);
    if (length > 0 && pine_type[length - 1] == '?') {
        length--;
    }
    size_t pointer_count = 0;

    while (length > 0 && pine_type[length - 1] == '*') {
        pointer_count++;
        length--;
    }

    char base[128];
    if (length >= sizeof(base)) {
        length = sizeof(base) - 1;
    }

    memcpy(base, pine_type, length);
    base[length] = '\0';

    fprintf(out, "%s", c_type(base));
    for (size_t i = 0; i < pointer_count; i++) {
        fprintf(out, "*");
    }
}

// Emits the standard primitive slice typedefs currently supported by the C backend.
static void emit_slice_typedefs(FILE *out) {
    const char *types[] = {
        "u8", "u16", "u32", "u64",
        "i8", "i16", "i32", "i64",
        "bool", "char", "float", "double",
        NULL
    };

    for (int i = 0; types[i]; i++) {
        fprintf(out, "typedef struct pine_slice_%s {\n", types[i]);
        fprintf(out, "    %s *data;\n", c_type(types[i]));
        fprintf(out, "    size_t length;\n");
        fprintf(out, "} pine_slice_%s;\n\n", types[i]);
    }
}

static void emit_slice_access_helpers(FILE *out) {
    const char *types[] = {
        "u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64",
        "bool", "char", "float", "double", NULL
    };
    for (int i = 0; types[i]; i++) {
        fprintf(out, "static %s *pine_slice_%s_at(pine_slice_%s slice, int64_t index) {\n",
                c_type(types[i]), types[i], types[i]);
        fprintf(out, "    return &slice.data[pine_bounds_check(index, slice.length)];\n");
        fprintf(out, "}\n\n");
    }
}

// Writes four-space indentation for a nested C statement.
static void emit_indent(FILE *out, int indent) {
    for (int i = 0; i < indent; i++) {
        fprintf(out, "    ");
    }
}

// Emits a C expression for a Pine expression node.
static void emit_expr(ASTNode *node, FILE *out) {
    switch (node->type) {
        case AST_NUMBER:
            fprintf(out, "%lld", node->number.value);
            break;
        case AST_CHAR_LITERAL:
        case AST_STRING_LITERAL:
            fprintf(out, "%s", node->literal.text);
            break;
        case AST_NULL_LITERAL:
            fprintf(out, "NULL");
            break;
        case AST_BOOL_LITERAL:
            fprintf(out, node->boolean.value ? "true" : "false");
            break;
        case AST_ARRAY_LITERAL:
            fprintf(out, "{");
            for (size_t i = 0; i < node->list.count; i++) {
                if (i > 0) fprintf(out, ", ");
                emit_expr(node->list.items[i], out);
            }
            fprintf(out, "}");
            break;
        case AST_STRUCT_LITERAL:
            fprintf(out, "(%s){", node->struct_literal.type_name);
            for (size_t i = 0; i < node->struct_literal.fields->list.count; i++) {
                ASTNode *field = node->struct_literal.fields->list.items[i];
                if (i > 0) fprintf(out, ", ");
                fprintf(out, ".%s = ", field->field_init.name);
                emit_expr(field->field_init.value, out);
            }
            fprintf(out, "}");
            break;
        case AST_IDENTIFIER:
            fprintf(out, "%s", node->identifier.name);
            break;
        case AST_CALL_EXPR:
            if (strcmp(node->call.name, "move") == 0 && node->call.args->list.count == 1) {
                emit_expr(node->call.args->list.items[0], out);
                break;
            }
            fprintf(out, "%s(", node->call.name);
            for (size_t i = 0; i < node->call.args->list.count; i++) {
                if (i > 0) {
                    fprintf(out, ", ");
                }
                emit_expr(node->call.args->list.items[i], out);
            }
            fprintf(out, ")");
            break;
        case AST_UNARY_EXPR:
            fputc('(', out);
            switch (node->unary.op) {
                case TOKEN_MINUS: fprintf(out, "-"); break;
                case TOKEN_BANG: fprintf(out, "!"); break;
                case TOKEN_TILDE: fprintf(out, "~"); break;
                case TOKEN_STAR: fprintf(out, "*"); break;
                case TOKEN_AND: fprintf(out, "&"); break;
                default: fprintf(out, "/* unsupported unary */"); break;
            }
            emit_expr(node->unary.operand, out);
            fputc(')', out);
            break;
        case AST_BINARY_EXPR:
            fputc('(', out);
            emit_expr(node->binary.left, out);
            switch (node->binary.op) {
                case TOKEN_PLUS: fprintf(out, " + "); break;
                case TOKEN_MINUS: fprintf(out, " - "); break;
                case TOKEN_STAR: fprintf(out, " * "); break;
                case TOKEN_SLASH: fprintf(out, " / "); break;
                case TOKEN_PERCENT: fprintf(out, " %% "); break;
                case TOKEN_AND: fprintf(out, " & "); break;
                case TOKEN_OR: fprintf(out, " | "); break;
                case TOKEN_XOR: fprintf(out, " ^ "); break;
                case TOKEN_LSHIFT: fprintf(out, " << "); break;
                case TOKEN_RSHIFT: fprintf(out, " >> "); break;
                case TOKEN_LT: fprintf(out, " < "); break;
                case TOKEN_LTE: fprintf(out, " <= "); break;
                case TOKEN_GT: fprintf(out, " > "); break;
                case TOKEN_GTE: fprintf(out, " >= "); break;
                case TOKEN_EQ: fprintf(out, " == "); break;
                case TOKEN_NEQ: fprintf(out, " != "); break;
                case TOKEN_AND_AND: fprintf(out, " && "); break;
                case TOKEN_OR_OR: fprintf(out, " || "); break;
                default: fprintf(out, " ? "); break;
            }
            emit_expr(node->binary.right, out);
            fputc(')', out);
            break;
        case AST_FIELD_EXPR:
            emit_expr(node->field.object, out);
            fprintf(out, ".%s", node->field.field);
            break;
        case AST_INDEX_EXPR:
            if (node->index.checked_is_slice) {
                fprintf(out, "(*pine_slice_%s_at(", node->index.checked_element_type);
                emit_expr(node->index.object, out);
                fprintf(out, ", (int64_t)(");
                emit_expr(node->index.index, out);
                fprintf(out, ")))");
            } else {
                emit_expr(node->index.object, out);
                fprintf(out, "[");
                if (node->index.checked_length > 0) {
                    fprintf(out, "pine_bounds_check((int64_t)(");
                    emit_expr(node->index.index, out);
                    fprintf(out, "), %zu)", node->index.checked_length);
                } else {
                    emit_expr(node->index.index, out);
                }
                fprintf(out, "]");
            }
            break;
        default:
            fprintf(out, "/* unsupported expression */");
            break;
    }
}

// Forward declarations let statements and blocks emit each other recursively.
static void emit_block(ASTNode *block, FILE *out, int indent);
static void emit_stmt(ASTNode *node, FILE *out, int indent);

// Emits one of the init/step clauses used inside a C for loop header.
static void emit_for_clause(ASTNode *node, FILE *out) {
    if (!node) {
        return;
    }

    switch (node->type) {
        case AST_VAR_DECL:
            if (node->var_decl.is_const) {
                fprintf(out, "const ");
            }
            emit_c_type(node->var_decl.var_type, out);
            fprintf(out, " %s", node->var_decl.name);
            if (node->var_decl.array_size > 0) {
                fprintf(out, "[%zu]", node->var_decl.array_size);
            }
            if (node->var_decl.value) {
                fprintf(out, " = ");
                emit_expr(node->var_decl.value, out);
            } else {
                fprintf(out, " = {0}");
            }
            break;
        case AST_ASSIGN_STMT:
            emit_expr(node->assign.target, out);
            fprintf(out, " = ");
            emit_expr(node->assign.value, out);
            break;
        case AST_EXPR_STMT:
            emit_expr(node->expr_stmt.expr, out);
            break;
        default:
            fprintf(out, "/* unsupported for clause */");
            break;
    }
}

// Emits one Pine statement as C at the requested indentation level.
static void emit_stmt(ASTNode *node, FILE *out, int indent) {
    switch (node->type) {
        case AST_VAR_DECL:
            emit_indent(out, indent);
            if (node->var_decl.is_const) {
                fprintf(out, "const ");
            }
            emit_c_type(node->var_decl.var_type, out);
            fprintf(out, " %s", node->var_decl.name);
            if (node->var_decl.array_size > 0) {
                fprintf(out, "[%zu]", node->var_decl.array_size);
            }
            if (node->var_decl.value) {
                fprintf(out, " = ");
                emit_expr(node->var_decl.value, out);
            } else {
                fprintf(out, " = {0}");
            }
            fprintf(out, ";\n");
            break;
        case AST_ASSIGN_STMT:
            emit_indent(out, indent);
            emit_expr(node->assign.target, out);
            fprintf(out, " = ");
            emit_expr(node->assign.value, out);
            fprintf(out, ";\n");
            break;
        case AST_EXPR_STMT:
            emit_indent(out, indent);
            emit_expr(node->expr_stmt.expr, out);
            fprintf(out, ";\n");
            break;
        case AST_RETURN_STMT:
            emit_indent(out, indent);
            fprintf(out, "return");
            if (node->ret.value) {
                fprintf(out, " ");
                emit_expr(node->ret.value, out);
            }
            fprintf(out, ";\n");
            break;
        case AST_IF_STMT:
            emit_indent(out, indent);
            fprintf(out, "if (");
            emit_expr(node->if_stmt.condition, out);
            fprintf(out, ") {\n");
            emit_block(node->if_stmt.then_block, out, indent + 1);
            emit_indent(out, indent);
            fprintf(out, "}");
            if (node->if_stmt.else_block) {
                fprintf(out, " else {\n");
                emit_block(node->if_stmt.else_block, out, indent + 1);
                emit_indent(out, indent);
                fprintf(out, "}");
            }
            fprintf(out, "\n");
            break;
        case AST_WHILE_STMT:
            emit_indent(out, indent);
            fprintf(out, "while (");
            emit_expr(node->while_stmt.condition, out);
            fprintf(out, ") {\n");
            emit_block(node->while_stmt.body, out, indent + 1);
            emit_indent(out, indent);
            fprintf(out, "}\n");
            break;
        case AST_FOR_STMT:
            emit_indent(out, indent);
            fprintf(out, "for (");
            emit_for_clause(node->for_stmt.init, out);
            fprintf(out, "; ");
            if (node->for_stmt.condition) {
                emit_expr(node->for_stmt.condition, out);
            }
            fprintf(out, "; ");
            emit_for_clause(node->for_stmt.step, out);
            fprintf(out, ") {\n");
            emit_block(node->for_stmt.body, out, indent + 1);
            emit_indent(out, indent);
            fprintf(out, "}\n");
            break;
        case AST_SWITCH_STMT:
            emit_indent(out, indent);
            fprintf(out, "switch (");
            emit_expr(node->switch_stmt.expr, out);
            fprintf(out, ") {\n");
            for (size_t i = 0; i < node->switch_stmt.cases->list.count; i++) {
                ASTNode *case_node = node->switch_stmt.cases->list.items[i];
                emit_indent(out, indent + 1);
                if (case_node->case_stmt.is_default) {
                    fprintf(out, "default:\n");
                } else {
                    fprintf(out, "case %lld:\n", case_node->case_stmt.value);
                }
                emit_block(case_node->case_stmt.body, out, indent + 2);
            }
            emit_indent(out, indent);
            fprintf(out, "}\n");
            break;
        case AST_BREAK_STMT:
            emit_indent(out, indent);
            fprintf(out, "break;\n");
            break;
        case AST_CONTINUE_STMT:
            emit_indent(out, indent);
            fprintf(out, "continue;\n");
            break;
        case AST_UNSAFE_BLOCK:
            emit_indent(out, indent);
            fprintf(out, "{\n");
            emit_block(node->unsafe_block.body, out, indent + 1);
            emit_indent(out, indent);
            fprintf(out, "}\n");
            break;
        default:
            emit_indent(out, indent);
            fprintf(out, "/* unsupported statement */\n");
            break;
    }
}

// Emits every statement in a block/list node.
static void emit_block(ASTNode *block, FILE *out, int indent) {
    for (size_t i = 0; i < block->list.count; i++) {
        emit_stmt(block->list.items[i], out, indent);
    }
}

// Emits a C function parameter list, using `void` for no parameters.
static void emit_params(ASTNode *params, FILE *out) {
    if (params->list.count == 0) {
        fprintf(out, "void");
        return;
    }

    for (size_t i = 0; i < params->list.count; i++) {
        ASTNode *param = params->list.items[i];
        if (i > 0) {
            fprintf(out, ", ");
        }
        emit_c_type(param->param.param_type, out);
        fprintf(out, " %s", param->param.name);
        if (param->param.array_size > 0) {
            fprintf(out, "[%zu]", param->param.array_size);
        }
    }
}

static void emit_function_signature(ASTNode *node, FILE *out) {
    emit_c_type(node->function.return_type, out);
    fprintf(out, " %s(", node->function.name);
    emit_params(node->function.params, out);
    fprintf(out, ")");
}

static void emit_function_decl(ASTNode *node, FILE *out) {
    emit_function_signature(node, out);
    fprintf(out, ";\n");
}

// Emits a complete C function definition.
static void emit_function(ASTNode *node, FILE *out) {
    emit_function_signature(node, out);
    fprintf(out, " {\n");
    emit_block(node->function.body, out, 1);
    fprintf(out, "}\n\n");
}

static void emit_enum(ASTNode *node, FILE *out) {
    fprintf(out, "typedef enum %s {\n", node->enum_decl.name);
    for (size_t i = 0; i < node->enum_decl.values->list.count; i++) {
        ASTNode *value = node->enum_decl.values->list.items[i];
        fprintf(out, "    %s = %lld%s\n", value->enum_value.name, value->enum_value.value,
                i + 1 < node->enum_decl.values->list.count ? "," : "");
    }
    fprintf(out, "} %s;\n\n", node->enum_decl.name);
}

// Emits a typedef-backed C struct for a Pine struct declaration.
static void emit_struct(ASTNode *node, FILE *out) {
    fprintf(out, "typedef struct %s {\n", node->struct_decl.name);
    for (size_t i = 0; i < node->struct_decl.fields->list.count; i++) {
        ASTNode *field = node->struct_decl.fields->list.items[i];
        fprintf(out, "    ");
        emit_c_type(field->field_decl.field_type, out);
        fprintf(out, " %s;\n", field->field_decl.name);
    }
    fprintf(out, "} %s;\n\n", node->struct_decl.name);
}

// Emits a top-level global or const global declaration.
static void emit_global(ASTNode *node, FILE *out) {
    if (node->var_decl.is_const) {
        fprintf(out, "const ");
    }
    emit_c_type(node->var_decl.var_type, out);
    fprintf(out, " %s", node->var_decl.name);
    if (node->var_decl.array_size > 0) {
        fprintf(out, "[%zu]", node->var_decl.array_size);
    }
    if (node->var_decl.value) {
        fprintf(out, " = ");
        emit_expr(node->var_decl.value, out);
    } else {
        fprintf(out, " = {0}");
    }
    fprintf(out, ";\n");
}

void codegen_generate_c(ASTNode *root, FILE *out) {
    fprintf(out, "#include <stdbool.h>\n");
    fprintf(out, "#include <stddef.h>\n");
    fprintf(out, "#include <stdint.h>\n\n");
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n\n");
    emit_slice_typedefs(out);
    fprintf(out, "static size_t pine_bounds_check(int64_t index, size_t length) {\n");
    fprintf(out, "    if (index < 0 || (uint64_t)index >= (uint64_t)length) {\n");
    fprintf(out, "        fprintf(stderr, \"Pine bounds check failed: index %%lld out of length %%zu\\n\", (long long)index, length);\n");
    fprintf(out, "        abort();\n");
    fprintf(out, "    }\n");
    fprintf(out, "    return (size_t)index;\n");
    fprintf(out, "}\n\n");
    emit_slice_access_helpers(out);

    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *item = root->list.items[i];
        if (item->type == AST_ENUM_DECL) emit_enum(item, out);
    }
    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *item = root->list.items[i];
        if (item->type == AST_STRUCT_DECL) emit_struct(item, out);
    }

    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *item = root->list.items[i];
        if (item->type == AST_FUNCTION) {
            emit_function_decl(item, out);
        }
    }
    fprintf(out, "\n");

    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *item = root->list.items[i];
        if (item->type == AST_VAR_DECL) {
            emit_global(item, out);
        }
    }
    fprintf(out, "\n");

    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *item = root->list.items[i];
        if (item->type == AST_FUNCTION) {
            emit_function(item, out);
        }
    }
}
