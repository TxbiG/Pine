#include "ir.h"
#include "lexer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------
// Small growable-array helpers
// ---------------------------------------------------------------------

static char *ir_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    memcpy(copy, s, len);
    return copy;
}

typedef struct {
    int next_label;
    int break_depth;
    int continue_depth;
    int break_labels[64];
    int continue_labels[64];
} IRLowerContext;

static void ir_func_reserve(IRFunction *fn) {
    if (fn->instr_count < fn->instr_cap) {
        return;
    }
    fn->instr_cap = fn->instr_cap == 0 ? 32 : fn->instr_cap * 2;
    fn->instrs = realloc(fn->instrs, fn->instr_cap * sizeof(IRInstr));
}

static IRInstr *ir_emit(IRFunction *fn, IROpcode op) {
    ir_func_reserve(fn);
    IRInstr *instr = &fn->instrs[fn->instr_count++];
    instr->op = op;
    instr->name = NULL;
    instr->type = NULL;
    instr->value = 0;
    instr->extra = 0;
    instr->flag = 0;
    instr->fallback_ast = NULL;
    return instr;
}

static const char *ir_unary_op_name(int op) {
    switch (op) {
        case TOKEN_MINUS: return "neg";
        case TOKEN_BANG: return "not";
        case TOKEN_TILDE: return "bit_not";
        case TOKEN_AND: return "address_of";
        case TOKEN_STAR: return "deref";
        default: return "op";
    }
}

// Converts binary operator tokens into stable IR operator names.
static const char *ir_op_name(int op) {
    switch (op) {
        case TOKEN_PLUS: return "add";
        case TOKEN_MINUS: return "sub";
        case TOKEN_STAR: return "mul";
        case TOKEN_SLASH: return "div";
        case TOKEN_PERCENT: return "mod";
        case TOKEN_AND: return "bit_and";
        case TOKEN_OR: return "bit_or";
        case TOKEN_XOR: return "bit_xor";
        case TOKEN_LSHIFT: return "shl";
        case TOKEN_RSHIFT: return "shr";
        case TOKEN_LT: return "lt";
        case TOKEN_LTE: return "lte";
        case TOKEN_GT: return "gt";
        case TOKEN_GTE: return "gte";
        case TOKEN_EQ: return "eq";
        case TOKEN_NEQ: return "neq";
        case TOKEN_AND_AND: return "and";
        case TOKEN_OR_OR: return "or";
        case TOKEN_BANG: return "not";
        case TOKEN_TILDE: return "bit_not";
        default: return "op";
    }
}

static int ir_new_label(IRLowerContext *ctx) {
    return ctx->next_label++;
}

static char *ir_label_name(const char *prefix, int id) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s_%d", prefix, id);
    return ir_strdup(buf);
}

static char *ir_case_label_name(int switch_id, long case_value, int is_default) {
    char buf[64];
    if (is_default) {
        snprintf(buf, sizeof(buf), "L_switch_default_%d", switch_id);
    } else {
        snprintf(buf, sizeof(buf), "L_switch_case_%d_%lld", switch_id, case_value);
    }
    return ir_strdup(buf);
}

// ---------------------------------------------------------------------
// Expression lowering
// ---------------------------------------------------------------------

static void ir_lower_expr(IRLowerContext *ctx, IRFunction *fn, ASTNode *expr) {
    switch (expr->type) {
        case AST_NUMBER: {
            IRInstr *instr = ir_emit(fn, IR_CONST);
            instr->value = expr->number.value;
            instr->type = ir_strdup("i32");
            break;
        }
        case AST_CHAR_LITERAL:
        case AST_STRING_LITERAL: {
            IRInstr *instr = ir_emit(fn, IR_LITERAL);
            instr->name = ir_strdup(expr->literal.text);
            instr->type = ir_strdup(expr->type == AST_CHAR_LITERAL ? "char" : "string");
            break;
        }
        case AST_NULL_LITERAL:
            ir_emit(fn, IR_NULL)->type = ir_strdup("null");
            break;
        case AST_BOOL_LITERAL: {
            IRInstr *instr = ir_emit(fn, IR_CONST);
            instr->value = expr->boolean.value;
            instr->type = ir_strdup("bool");
            break;
        }
        case AST_ARRAY_LITERAL: {
            for (size_t i = 0; i < expr->list.count; i++) {
                ir_lower_expr(ctx, fn, expr->list.items[i]);
            }
            IRInstr *instr = ir_emit(fn, IR_ARRAY);
            instr->extra = expr->list.count;
            break;
        }
        case AST_STRUCT_LITERAL: {
            for (size_t i = 0; i < expr->struct_literal.fields->list.count; i++) {
                ASTNode *field = expr->struct_literal.fields->list.items[i];
                ir_lower_expr(ctx, fn, field->field_init.value);
                IRInstr *marker = ir_emit(fn, IR_STRUCT_FIELD);
                marker->name = ir_strdup(field->field_init.name);
            }
            IRInstr *build = ir_emit(fn, IR_STRUCT_BUILD);
            build->name = ir_strdup(expr->struct_literal.type_name);
            build->extra = expr->struct_literal.fields->list.count;
            break;
        }
        case AST_IDENTIFIER: {
            IRInstr *instr = ir_emit(fn, IR_LOAD);
            instr->name = ir_strdup(expr->identifier.name);
            break;
        }
        case AST_UNARY_EXPR: {
            ir_lower_expr(ctx, fn, expr->unary.operand);
            IRInstr *instr = ir_emit(fn, IR_UNARY);
            instr->name = ir_strdup(ir_unary_op_name(expr->unary.op));
            break;
        }
        case AST_BINARY_EXPR: {
            if (expr->binary.op == TOKEN_AND_AND || expr->binary.op == TOKEN_OR_OR) {
                int branch_id = ir_new_label(ctx);
                int end_id = ir_new_label(ctx);
                char *branch_label = ir_label_name(
                    expr->binary.op == TOKEN_AND_AND ? "L_logic_false" : "L_logic_true",
                    branch_id);
                char *end_label = ir_label_name("L_logic_end", end_id);
                ir_lower_expr(ctx, fn, expr->binary.left);
                IRInstr *branch = ir_emit(fn,
                    expr->binary.op == TOKEN_AND_AND ? IR_JUMP_IF_FALSE : IR_JUMP_IF_TRUE);
                branch->name = ir_strdup(branch_label);
                ir_lower_expr(ctx, fn, expr->binary.right);
                IRInstr *jump = ir_emit(fn, IR_JUMP);
                jump->name = ir_strdup(end_label);
                ir_emit(fn, IR_LABEL)->name = branch_label;
                IRInstr *constant = ir_emit(fn, IR_CONST);
                constant->value = expr->binary.op == TOKEN_AND_AND ? 0 : 1;
                constant->type = ir_strdup("bool");
                ir_emit(fn, IR_LABEL)->name = end_label;
            } else {
                ir_lower_expr(ctx, fn, expr->binary.left);
                ir_lower_expr(ctx, fn, expr->binary.right);
                IRInstr *instr = ir_emit(fn, IR_BINARY);
                instr->name = ir_strdup(ir_op_name(expr->binary.op));
            }
            break;
        }
        case AST_FIELD_EXPR: {
            ir_lower_expr(ctx, fn, expr->field.object);
            IRInstr *instr = ir_emit(fn, IR_FIELD);
            instr->name = ir_strdup(expr->field.field);
            break;
        }
        case AST_INDEX_EXPR: {
            ir_lower_expr(ctx, fn, expr->index.object);
            ir_lower_expr(ctx, fn, expr->index.index);
            IRInstr *instr = ir_emit(fn, IR_INDEX);
            instr->value = (long)expr->index.checked_length;
            instr->flag = expr->index.checked_is_slice;
            break;
        }
        case AST_CALL_EXPR: {
            for (size_t i = 0; i < expr->call.args->list.count; i++) {
                ir_lower_expr(ctx, fn, expr->call.args->list.items[i]);
            }
            IRInstr *instr = ir_emit(fn, IR_CALL);
            instr->name = ir_strdup(expr->call.name);
            instr->extra = expr->call.args->list.count;
            break;
        }
        default: {
            IRInstr *instr = ir_emit(fn, IR_UNSUPPORTED);
            instr->name = ir_strdup("expression");
            instr->fallback_ast = expr;
            break;
        }
    }
}

// ---------------------------------------------------------------------
// Statement lowering
// ---------------------------------------------------------------------

static void ir_lower_block(IRLowerContext *ctx, IRFunction *fn, ASTNode *block);

static void ir_lower_stmt(IRLowerContext *ctx, IRFunction *fn, ASTNode *stmt) {
    switch (stmt->type) {
        case AST_VAR_DECL: {
            IRInstr *decl = ir_emit(fn, IR_DECL_LOCAL);
            decl->name = ir_strdup(stmt->var_decl.name);
            decl->type = ir_strdup(stmt->var_decl.var_type);
            decl->extra = stmt->var_decl.array_size;
            if (stmt->var_decl.value) {
                ir_lower_expr(ctx, fn, stmt->var_decl.value);
                IRInstr *store = ir_emit(fn, IR_STORE);
                store->name = ir_strdup(stmt->var_decl.name);
            }
            break;
        }
        case AST_ASSIGN_STMT: {
            ASTNode *target = stmt->assign.target;
            if (target->type == AST_IDENTIFIER) {
                ir_lower_expr(ctx, fn, stmt->assign.value);
                IRInstr *store = ir_emit(fn, IR_STORE);
                store->name = ir_strdup(target->identifier.name);
            } else if (target->type == AST_FIELD_EXPR) {
                ir_lower_expr(ctx, fn, target->field.object);
                ir_lower_expr(ctx, fn, stmt->assign.value);
                IRInstr *store = ir_emit(fn, IR_STORE_FIELD);
                store->name = ir_strdup(target->field.field);
            } else if (target->type == AST_INDEX_EXPR) {
                ir_lower_expr(ctx, fn, target->index.object);
                ir_lower_expr(ctx, fn, target->index.index);
                ir_lower_expr(ctx, fn, stmt->assign.value);
                IRInstr *store = ir_emit(fn, IR_STORE_INDEX);
                store->value = (long)target->index.checked_length;
                store->flag = target->index.checked_is_slice;
            } else if (target->type == AST_UNARY_EXPR && target->unary.op == TOKEN_STAR) {
                ir_lower_expr(ctx, fn, target->unary.operand);
                ir_lower_expr(ctx, fn, stmt->assign.value);
                ir_emit(fn, IR_STORE_DEREF);
            } else {
                IRInstr *unsupported = ir_emit(fn, IR_UNSUPPORTED);
                unsupported->name = ir_strdup("assignment_target");
                unsupported->fallback_ast = stmt;
            }
            break;
        }
        case AST_EXPR_STMT:
            ir_lower_expr(ctx, fn, stmt->expr_stmt.expr);
            ir_emit(fn, IR_POP);
            break;
        case AST_RETURN_STMT:
            if (stmt->ret.value) {
                ir_lower_expr(ctx, fn, stmt->ret.value);
                ir_emit(fn, IR_RETURN_VALUE);
            }
            ir_emit(fn, IR_RETURN);
            break;
        case AST_IF_STMT: {
            int else_id = ir_new_label(ctx);
            int end_id = ir_new_label(ctx);
            char *else_label = ir_label_name("L_else", else_id);
            char *end_label = ir_label_name("L_end_if", end_id);

            ir_lower_expr(ctx, fn, stmt->if_stmt.condition);
            IRInstr *jf = ir_emit(fn, IR_JUMP_IF_FALSE);
            jf->name = ir_strdup(else_label);

            ir_lower_block(ctx, fn, stmt->if_stmt.then_block);
            IRInstr *j = ir_emit(fn, IR_JUMP);
            j->name = ir_strdup(end_label);

            ir_emit(fn, IR_LABEL)->name = else_label;
            if (stmt->if_stmt.else_block) {
                ir_lower_block(ctx, fn, stmt->if_stmt.else_block);
            }
            ir_emit(fn, IR_LABEL)->name = end_label;
            break;
        }
        case AST_WHILE_STMT: {
            int start_id = ir_new_label(ctx);
            int end_id = ir_new_label(ctx);
            char *start_label = ir_label_name("L_while_start", start_id);
            char *end_label = ir_label_name("L_while_end", end_id);

            ir_emit(fn, IR_LABEL)->name = ir_strdup(start_label);
            ir_emit(fn, IR_LABEL)->name = ir_label_name("L_continue", start_id);

            ir_lower_expr(ctx, fn, stmt->while_stmt.condition);
            IRInstr *jf = ir_emit(fn, IR_JUMP_IF_FALSE);
            jf->name = ir_strdup(end_label);

            ctx->break_labels[ctx->break_depth++] = end_id;
            ctx->continue_labels[ctx->continue_depth++] = start_id;
            ir_lower_block(ctx, fn, stmt->while_stmt.body);
            ctx->break_depth--;
            ctx->continue_depth--;

            IRInstr *j = ir_emit(fn, IR_JUMP);
            j->name = ir_strdup(start_label);
            ir_emit(fn, IR_LABEL)->name = ir_label_name("L_break", end_id);
            ir_emit(fn, IR_LABEL)->name = end_label;
            break;
        }
        case AST_FOR_STMT: {
            int start_id = ir_new_label(ctx);
            int step_id = ir_new_label(ctx);
            int end_id = ir_new_label(ctx);
            char *start_label = ir_label_name("L_for_start", start_id);
            char *step_label = ir_label_name("L_for_step", step_id);
            char *end_label = ir_label_name("L_for_end", end_id);

            if (stmt->for_stmt.init) ir_lower_stmt(ctx, fn, stmt->for_stmt.init);
            ir_emit(fn, IR_LABEL)->name = ir_strdup(start_label);
            if (stmt->for_stmt.condition) {
                ir_lower_expr(ctx, fn, stmt->for_stmt.condition);
                IRInstr *jf = ir_emit(fn, IR_JUMP_IF_FALSE);
                jf->name = ir_strdup(end_label);
            }

            ctx->break_labels[ctx->break_depth++] = end_id;
            ctx->continue_labels[ctx->continue_depth++] = step_id;
            ir_lower_block(ctx, fn, stmt->for_stmt.body);
            ctx->break_depth--;
            ctx->continue_depth--;

            ir_emit(fn, IR_LABEL)->name = ir_label_name("L_continue", step_id);
            ir_emit(fn, IR_LABEL)->name = ir_strdup(step_label);
            if (stmt->for_stmt.step) ir_lower_stmt(ctx, fn, stmt->for_stmt.step);
            IRInstr *j = ir_emit(fn, IR_JUMP);
            j->name = ir_strdup(start_label);
            ir_emit(fn, IR_LABEL)->name = ir_label_name("L_break", end_id);
            ir_emit(fn, IR_LABEL)->name = end_label;
            break;
        }
        case AST_SWITCH_STMT: {
            int switch_id = ir_new_label(ctx);
            char *end_label = ir_label_name("L_switch_end", switch_id);

            ir_lower_expr(ctx, fn, stmt->switch_stmt.expr);
            IRInstr *dispatch = ir_emit(fn, IR_SWITCH_DISPATCH);
            dispatch->name = ir_strdup(end_label);

            for (size_t i = 0; i < stmt->switch_stmt.cases->list.count; i++) {
                ASTNode *case_node = stmt->switch_stmt.cases->list.items[i];
                IRInstr *case_instr = ir_emit(fn, IR_CASE);
                case_instr->name = ir_case_label_name(switch_id, case_node->case_stmt.value, case_node->case_stmt.is_default);
                case_instr->value = case_node->case_stmt.value;
                case_instr->flag = case_node->case_stmt.is_default;

                ctx->break_labels[ctx->break_depth++] = switch_id;
                ir_lower_block(ctx, fn, case_node->case_stmt.body);
                ctx->break_depth--;
            }

            ir_emit(fn, IR_LABEL)->name = ir_label_name("L_break", switch_id);
            ir_emit(fn, IR_LABEL)->name = end_label;
            break;
        }
        case AST_BREAK_STMT:
            if (ctx->break_depth > 0) {
                IRInstr *j = ir_emit(fn, IR_JUMP);
                j->name = ir_label_name("L_break", ctx->break_labels[ctx->break_depth - 1]);
            } else {
                ir_emit(fn, IR_BREAK);
            }
            break;
        case AST_CONTINUE_STMT:
            if (ctx->continue_depth > 0) {
                IRInstr *j = ir_emit(fn, IR_JUMP);
                j->name = ir_label_name("L_continue", ctx->continue_labels[ctx->continue_depth - 1]);
            } else {
                ir_emit(fn, IR_CONTINUE);
            }
            break;
        case AST_UNSAFE_BLOCK:
            ir_emit(fn, IR_UNSAFE_BEGIN);
            ir_lower_block(ctx, fn, stmt->unsafe_block.body);
            ir_emit(fn, IR_UNSAFE_END);
            break;
        default: {
            IRInstr *instr = ir_emit(fn, IR_UNSUPPORTED);
            instr->name = ir_strdup("statement");
            instr->fallback_ast = stmt;
            break;
        }
    }
}

static void ir_lower_block(IRLowerContext *ctx, IRFunction *fn, ASTNode *block) {
    ir_emit(fn, IR_SCOPE_BEGIN);
    for (size_t i = 0; i < block->list.count; i++) {
        ir_lower_stmt(ctx, fn, block->list.items[i]);
    }
    ir_emit(fn, IR_SCOPE_END);
}

// ---------------------------------------------------------------------
// Top-level lowering
// ---------------------------------------------------------------------

static void ir_lower_function(ASTNode *fn_node, IRFunction *fn) {
    fn->name = ir_strdup(fn_node->function.name);
    fn->return_type = ir_strdup(fn_node->function.return_type);

    fn->param_count = fn_node->function.params->list.count;
    fn->params = fn->param_count ? calloc(fn->param_count, sizeof(IRParam)) : NULL;
    for (size_t i = 0; i < fn->param_count; i++) {
        ASTNode *param = fn_node->function.params->list.items[i];
        fn->params[i].name = ir_strdup(param->param.name);
        fn->params[i].type = ir_strdup(param->param.param_type);
        fn->params[i].array_size = param->param.array_size;
    }

    IRLowerContext ctx = {0};
    ir_lower_block(&ctx, fn, fn_node->function.body);
}

IRModule *ir_lower_program(ASTNode *root) {
    IRModule *module = calloc(1, sizeof(IRModule));

    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *item = root->list.items[i];
        if (item->type == AST_STRUCT_DECL) module->struct_count++;
        else if (item->type == AST_ENUM_DECL) module->global_count += item->enum_decl.values->list.count;
        else if (item->type == AST_VAR_DECL) module->global_count++;
        else if (item->type == AST_FUNCTION) module->function_count++;
    }

    module->structs = module->struct_count ? calloc(module->struct_count, sizeof(IRStruct)) : NULL;
    module->globals = module->global_count ? calloc(module->global_count, sizeof(IRGlobal)) : NULL;
    module->functions = module->function_count ? calloc(module->function_count, sizeof(IRFunction)) : NULL;

    size_t struct_i = 0, global_i = 0, function_i = 0;
    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *item = root->list.items[i];
        if (item->type == AST_STRUCT_DECL) {
            IRStruct *s = &module->structs[struct_i++];
            s->name = ir_strdup(item->struct_decl.name);
            s->field_count = item->struct_decl.fields->list.count;
            s->fields = s->field_count ? calloc(s->field_count, sizeof(IRField)) : NULL;
            for (size_t j = 0; j < s->field_count; j++) {
                ASTNode *field = item->struct_decl.fields->list.items[j];
                s->fields[j].field_type = ir_strdup(field->field_decl.field_type);
                s->fields[j].name = ir_strdup(field->field_decl.name);
            }
        } else if (item->type == AST_ENUM_DECL) {
            for (size_t j = 0; j < item->enum_decl.values->list.count; j++) {
                ASTNode *value = item->enum_decl.values->list.items[j];
                IRGlobal *g = &module->globals[global_i++];
                g->type = ir_strdup(item->enum_decl.name);
                g->name = ir_strdup(value->enum_value.name);
                g->init_instrs = calloc(1, sizeof(IRInstr));
                g->init_count = 1;
                g->init_instrs[0].op = IR_CONST;
                g->init_instrs[0].value = value->enum_value.value;
            }
        } else if (item->type == AST_VAR_DECL) {
            IRGlobal *g = &module->globals[global_i++];
            g->type = ir_strdup(item->var_decl.var_type);
            g->name = ir_strdup(item->var_decl.name);
            g->array_size = item->var_decl.array_size;
            if (item->var_decl.value) {
                IRFunction scratch = {0};
                IRLowerContext ctx = {0};
                ir_lower_expr(&ctx, &scratch, item->var_decl.value);
                g->init_instrs = scratch.instrs;
                g->init_count = scratch.instr_count;
            }
        } else if (item->type == AST_FUNCTION) {
            ir_lower_function(item, &module->functions[function_i++]);
        }
    }

    for (size_t i = 0; i < module->function_count; i++) {
        IRFunction *fn = &module->functions[i];
        for (size_t j = 0; j < fn->instr_count; j++) {
            IRInstr *instr = &fn->instrs[j];
            if (instr->op != IR_CALL) continue;
            for (size_t k = 0; k < module->function_count; k++) {
                if (strcmp(module->functions[k].name, instr->name) == 0) {
                    instr->type = ir_strdup(module->functions[k].return_type);
                    if (strcmp(instr->type, "void") == 0 &&
                        j + 1 < fn->instr_count && fn->instrs[j + 1].op == IR_POP) {
                        fn->instrs[j + 1].flag = 1;
                    }
                    break;
                }
            }
        }
    }

    return module;
}

// ---------------------------------------------------------------------
// Freeing
// ---------------------------------------------------------------------

static void ir_free_instrs(IRInstr *instrs, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(instrs[i].name);
        free(instrs[i].type);
    }
    free(instrs);
}

void ir_free_module(IRModule *module) {
    if (!module) return;

    for (size_t i = 0; i < module->struct_count; i++) {
        IRStruct *s = &module->structs[i];
        for (size_t j = 0; j < s->field_count; j++) {
            free(s->fields[j].field_type);
            free(s->fields[j].name);
        }
        free(s->fields);
        free(s->name);
    }
    free(module->structs);

    for (size_t i = 0; i < module->global_count; i++) {
        IRGlobal *g = &module->globals[i];
        free(g->type);
        free(g->name);
        ir_free_instrs(g->init_instrs, g->init_count);
    }
    free(module->globals);

    for (size_t i = 0; i < module->function_count; i++) {
        IRFunction *fn = &module->functions[i];
        free(fn->name);
        free(fn->return_type);
        for (size_t j = 0; j < fn->param_count; j++) {
            free(fn->params[j].name);
            free(fn->params[j].type);
        }
        free(fn->params);
        ir_free_instrs(fn->instrs, fn->instr_count);
    }
    free(module->functions);

    free(module);
}

void ir_instr_stack_effect(const IRInstr *instr, size_t *pops, size_t *pushes) {
    size_t local_pops = 0;
    size_t local_pushes = 0;
    switch (instr->op) {
        case IR_CONST: case IR_LITERAL: case IR_NULL: case IR_LOAD:
            local_pushes = 1; break;
        case IR_UNARY: case IR_FIELD:
            local_pops = 1; local_pushes = 1; break;
        case IR_BINARY: case IR_INDEX:
            local_pops = 2; local_pushes = 1; break;
        case IR_CALL:
            local_pops = instr->extra;
            local_pushes = (!instr->type || strcmp(instr->type, "void") != 0) ? 1 : 0;
            break;
        case IR_POP:
            local_pops = instr->flag ? 0 : 1;
            break;
        case IR_STORE: case IR_RETURN_VALUE:
        case IR_JUMP_IF_FALSE: case IR_JUMP_IF_TRUE: case IR_SWITCH_DISPATCH:
            local_pops = 1; break;
        case IR_STORE_FIELD: case IR_STORE_DEREF:
            local_pops = 2; break;
        case IR_STORE_INDEX:
            local_pops = 3; break;
        case IR_ARRAY:
        case IR_STRUCT_BUILD:
            local_pops = instr->extra; local_pushes = 1; break;
        case IR_STRUCT_FIELD:
            break;
        default:
            break;
    }
    if (pops) *pops = local_pops;
    if (pushes) *pushes = local_pushes;
}
// ---------------------------------------------------------------------
// Textual dump
// ---------------------------------------------------------------------

static void ir_dump_instr(const IRInstr *instr, FILE *out) {
    switch (instr->op) {
        case IR_CONST: fprintf(out, "  const %lld\n", instr->value); break;
        case IR_LITERAL: fprintf(out, "  literal %s\n", instr->name); break;
        case IR_NULL: fprintf(out, "  null\n"); break;
        case IR_LOAD: fprintf(out, "  load %s\n", instr->name); break;
        case IR_STORE: fprintf(out, "  store %s\n", instr->name); break;
        case IR_STORE_FIELD: fprintf(out, "  store_field %s\n", instr->name); break;
        case IR_STORE_INDEX:
            if (instr->flag) fprintf(out, "  store_index bounds=slice\n");
            else if (instr->value > 0) fprintf(out, "  store_index bounds=%lld\n", instr->value);
            else fprintf(out, "  store_index\n");
            break;
        case IR_STORE_DEREF: fprintf(out, "  store_deref\n"); break;
        case IR_ARRAY: fprintf(out, "  array count=%zu\n", instr->extra); break;
        case IR_STRUCT_FIELD: fprintf(out, "  struct_field %s\n", instr->name); break;
        case IR_STRUCT_BUILD: fprintf(out, "  struct_build %s count=%zu\n", instr->name, instr->extra); break;
        case IR_UNARY: fprintf(out, "  unary %s\n", instr->name); break;
        case IR_BINARY: fprintf(out, "  binary %s\n", instr->name); break;
        case IR_FIELD: fprintf(out, "  field %s\n", instr->name); break;
        case IR_INDEX:
            if (instr->flag) fprintf(out, "  index bounds=slice\n");
            else if (instr->value > 0) fprintf(out, "  index bounds=%lld\n", instr->value);
            else fprintf(out, "  index\n");
            break;
        case IR_CALL: fprintf(out, "  call %s argc=%zu\n", instr->name, instr->extra); break;
        case IR_POP: fprintf(out, instr->flag ? "  discard_void\n" : "  pop\n"); break;
        case IR_RETURN_VALUE: fprintf(out, "  return_value\n"); break;
        case IR_RETURN: fprintf(out, "  return\n"); break;
        case IR_DECL_LOCAL:
            fprintf(out, "  decl_local %s %s", instr->type, instr->name);
            if (instr->extra > 0) fprintf(out, "[%zu]", instr->extra);
            fprintf(out, "\n");
            break;
        case IR_SCOPE_BEGIN: fprintf(out, "  scope_begin\n"); break;
        case IR_SCOPE_END: fprintf(out, "  scope_end\n"); break;
        case IR_LABEL: fprintf(out, "label %s:\n", instr->name); break;
        case IR_JUMP: fprintf(out, "  jump %s\n", instr->name); break;
        case IR_JUMP_IF_FALSE: fprintf(out, "  jump_if_false %s\n", instr->name); break;
        case IR_JUMP_IF_TRUE: fprintf(out, "  jump_if_true %s\n", instr->name); break;
        case IR_SWITCH_DISPATCH: fprintf(out, "  switch_dispatch end=%s\n", instr->name); break;
        case IR_CASE:
            if (instr->flag) fprintf(out, "case default %s:\n", instr->name);
            else fprintf(out, "case %lld %s:\n", instr->value, instr->name);
            break;
        case IR_UNSAFE_BEGIN: fprintf(out, "  unsafe_begin\n"); break;
        case IR_UNSAFE_END: fprintf(out, "  unsafe_end\n"); break;
        case IR_BREAK: fprintf(out, "  break\n"); break;
        case IR_CONTINUE: fprintf(out, "  continue\n"); break;
        case IR_UNSUPPORTED: fprintf(out, "  unsupported %s\n", instr->name); break;
    }
}

void ir_dump_program(IRModule *module, FILE *out) {
    fprintf(out, "pine_ir 1\n\n");

    for (size_t i = 0; i < module->struct_count; i++) {
        IRStruct *s = &module->structs[i];
        fprintf(out, "struct %s\n", s->name);
        for (size_t j = 0; j < s->field_count; j++) {
            fprintf(out, "  field %s %s\n", s->fields[j].field_type, s->fields[j].name);
        }
        fprintf(out, "end\n\n");
    }

    for (size_t i = 0; i < module->global_count; i++) {
        IRGlobal *g = &module->globals[i];
        fprintf(out, "global %s %s", g->type, g->name);
        if (g->array_size > 0) fprintf(out, "[%zu]", g->array_size);
        fprintf(out, "\n");
        for (size_t j = 0; j < g->init_count; j++) {
            ir_dump_instr(&g->init_instrs[j], out);
        }
        fprintf(out, "\n");
    }

    for (size_t i = 0; i < module->function_count; i++) {
        IRFunction *fn = &module->functions[i];
        fprintf(out, "function %s -> %s\n", fn->name, fn->return_type);
        for (size_t j = 0; j < fn->param_count; j++) {
            fprintf(out, "  param %s %s\n", fn->params[j].type, fn->params[j].name);
        }
        for (size_t j = 0; j < fn->instr_count; j++) {
            ir_dump_instr(&fn->instrs[j], out);
        }
        fprintf(out, "end\n\n");
    }
}
