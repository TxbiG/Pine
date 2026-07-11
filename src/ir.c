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
    int loop_depth;
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
    return instr;
}

// Converts operator tokens into stable IR operator names.
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
        snprintf(buf, sizeof(buf), "L_switch_case_%d_%ld", switch_id, case_value);
    }
    return ir_strdup(buf);
}

// ---------------------------------------------------------------------
// Expression lowering
// ---------------------------------------------------------------------

static void ir_lower_expr(IRFunction *fn, ASTNode *expr) {
    switch (expr->type) {
        case AST_NUMBER: {
            IRInstr *instr = ir_emit(fn, IR_CONST);
            instr->value = expr->number.value;
            break;
        }
        case AST_CHAR_LITERAL:
        case AST_STRING_LITERAL: {
            IRInstr *instr = ir_emit(fn, IR_LITERAL);
            instr->name = ir_strdup(expr->literal.text);
            break;
        }
        case AST_NULL_LITERAL:
            ir_emit(fn, IR_NULL);
            break;
        case AST_IDENTIFIER: {
            IRInstr *instr = ir_emit(fn, IR_LOAD);
            instr->name = ir_strdup(expr->identifier.name);
            break;
        }
        case AST_UNARY_EXPR: {
            ir_lower_expr(fn, expr->unary.operand);
            IRInstr *instr = ir_emit(fn, IR_UNARY);
            instr->name = ir_strdup(ir_op_name(expr->unary.op));
            break;
        }
        case AST_BINARY_EXPR: {
            ir_lower_expr(fn, expr->binary.left);
            ir_lower_expr(fn, expr->binary.right);
            IRInstr *instr = ir_emit(fn, IR_BINARY);
            instr->name = ir_strdup(ir_op_name(expr->binary.op));
            break;
        }
        case AST_FIELD_EXPR: {
            ir_lower_expr(fn, expr->field.object);
            IRInstr *instr = ir_emit(fn, IR_FIELD);
            instr->name = ir_strdup(expr->field.field);
            break;
        }
        case AST_INDEX_EXPR: {
            ir_lower_expr(fn, expr->index.object);
            ir_lower_expr(fn, expr->index.index);
            IRInstr *instr = ir_emit(fn, IR_INDEX);
            instr->value = (long)expr->index.checked_length;
            instr->flag = expr->index.checked_is_slice;
            break;
        }
        case AST_CALL_EXPR: {
            for (size_t i = 0; i < expr->call.args->list.count; i++) {
                ir_lower_expr(fn, expr->call.args->list.items[i]);
            }
            IRInstr *instr = ir_emit(fn, IR_CALL);
            instr->name = ir_strdup(expr->call.name);
            instr->extra = expr->call.args->list.count;
            break;
        }
        default: {
            IRInstr *instr = ir_emit(fn, IR_UNSUPPORTED);
            instr->name = ir_strdup("expression");
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
                ir_lower_expr(fn, stmt->var_decl.value);
                IRInstr *store = ir_emit(fn, IR_STORE);
                store->name = ir_strdup(stmt->var_decl.name);
            }
            break;
        }
        case AST_ASSIGN_STMT: {
            ir_lower_expr(fn, stmt->assign.value);
            IRInstr *store = ir_emit(fn, IR_STORE);
            store->name = ir_strdup(stmt->assign.name);
            break;
        }
        case AST_EXPR_STMT:
            ir_lower_expr(fn, stmt->expr_stmt.expr);
            ir_emit(fn, IR_POP);
            break;
        case AST_RETURN_STMT:
            if (stmt->ret.value) {
                ir_lower_expr(fn, stmt->ret.value);
                ir_emit(fn, IR_RETURN_VALUE);
            }
            ir_emit(fn, IR_RETURN);
            break;
        case AST_IF_STMT: {
            int else_id = ir_new_label(ctx);
            int end_id = ir_new_label(ctx);
            char *else_label = ir_label_name("L_else", else_id);
            char *end_label = ir_label_name("L_end_if", end_id);

            ir_lower_expr(fn, stmt->if_stmt.condition);
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

            ir_lower_expr(fn, stmt->while_stmt.condition);
            IRInstr *jf = ir_emit(fn, IR_JUMP_IF_FALSE);
            jf->name = ir_strdup(end_label);

            ctx->break_labels[ctx->loop_depth] = end_id;
            ctx->continue_labels[ctx->loop_depth] = start_id;
            ctx->loop_depth++;
            ir_lower_block(ctx, fn, stmt->while_stmt.body);
            ctx->loop_depth--;

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
                ir_lower_expr(fn, stmt->for_stmt.condition);
                IRInstr *jf = ir_emit(fn, IR_JUMP_IF_FALSE);
                jf->name = ir_strdup(end_label);
            }

            ctx->break_labels[ctx->loop_depth] = end_id;
            ctx->continue_labels[ctx->loop_depth] = step_id;
            ctx->loop_depth++;
            ir_lower_block(ctx, fn, stmt->for_stmt.body);
            ctx->loop_depth--;

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

            ir_lower_expr(fn, stmt->switch_stmt.expr);
            IRInstr *dispatch = ir_emit(fn, IR_SWITCH_DISPATCH);
            dispatch->name = ir_strdup(end_label);

            for (size_t i = 0; i < stmt->switch_stmt.cases->list.count; i++) {
                ASTNode *case_node = stmt->switch_stmt.cases->list.items[i];
                IRInstr *case_instr = ir_emit(fn, IR_CASE);
                case_instr->name = ir_case_label_name(switch_id, case_node->case_stmt.value, case_node->case_stmt.is_default);
                case_instr->value = case_node->case_stmt.value;
                case_instr->flag = case_node->case_stmt.is_default;

                ctx->break_labels[ctx->loop_depth] = switch_id;
                ctx->continue_labels[ctx->loop_depth] = -1;
                ctx->loop_depth++;
                ir_lower_block(ctx, fn, case_node->case_stmt.body);
                ctx->loop_depth--;
            }

            ir_emit(fn, IR_LABEL)->name = ir_label_name("L_break", switch_id);
            ir_emit(fn, IR_LABEL)->name = end_label;
            break;
        }
        case AST_BREAK_STMT:
            if (ctx->loop_depth > 0) {
                IRInstr *j = ir_emit(fn, IR_JUMP);
                j->name = ir_label_name("L_break", ctx->break_labels[ctx->loop_depth - 1]);
            } else {
                ir_emit(fn, IR_BREAK);
            }
            break;
        case AST_CONTINUE_STMT:
            if (ctx->loop_depth > 0 && ctx->continue_labels[ctx->loop_depth - 1] >= 0) {
                IRInstr *j = ir_emit(fn, IR_JUMP);
                j->name = ir_label_name("L_continue", ctx->continue_labels[ctx->loop_depth - 1]);
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
            break;
        }
    }
}

static void ir_lower_block(IRLowerContext *ctx, IRFunction *fn, ASTNode *block) {
    for (size_t i = 0; i < block->list.count; i++) {
        ir_lower_stmt(ctx, fn, block->list.items[i]);
    }
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
    }

    IRLowerContext ctx = {0};
    ir_lower_block(&ctx, fn, fn_node->function.body);
}

IRModule *ir_lower_program(ASTNode *root) {
    IRModule *module = calloc(1, sizeof(IRModule));

    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *item = root->list.items[i];
        if (item->type == AST_STRUCT_DECL) module->struct_count++;
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
        } else if (item->type == AST_VAR_DECL) {
            IRGlobal *g = &module->globals[global_i++];
            g->type = ir_strdup(item->var_decl.var_type);
            g->name = ir_strdup(item->var_decl.name);
            g->array_size = item->var_decl.array_size;
            if (item->var_decl.value) {
                IRFunction scratch = {0};
                ir_lower_expr(&scratch, item->var_decl.value);
                g->init_instrs = scratch.instrs;
                g->init_count = scratch.instr_count;
            }
        } else if (item->type == AST_FUNCTION) {
            ir_lower_function(item, &module->functions[function_i++]);
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

// ---------------------------------------------------------------------
// Textual dump
// ---------------------------------------------------------------------

static void ir_dump_instr(const IRInstr *instr, FILE *out) {
    switch (instr->op) {
        case IR_CONST: fprintf(out, "  const %ld\n", instr->value); break;
        case IR_LITERAL: fprintf(out, "  literal %s\n", instr->name); break;
        case IR_NULL: fprintf(out, "  null\n"); break;
        case IR_LOAD: fprintf(out, "  load %s\n", instr->name); break;
        case IR_STORE: fprintf(out, "  store %s\n", instr->name); break;
        case IR_UNARY: fprintf(out, "  unary %s\n", instr->name); break;
        case IR_BINARY: fprintf(out, "  binary %s\n", instr->name); break;
        case IR_FIELD: fprintf(out, "  field %s\n", instr->name); break;
        case IR_INDEX:
            if (instr->flag) fprintf(out, "  index bounds=slice\n");
            else if (instr->value > 0) fprintf(out, "  index bounds=%ld\n", instr->value);
            else fprintf(out, "  index\n");
            break;
        case IR_CALL: fprintf(out, "  call %s argc=%zu\n", instr->name, instr->extra); break;
        case IR_POP: fprintf(out, "  pop\n"); break;
        case IR_RETURN_VALUE: fprintf(out, "  return_value\n"); break;
        case IR_RETURN: fprintf(out, "  return\n"); break;
        case IR_DECL_LOCAL:
            fprintf(out, "  decl_local %s %s", instr->type, instr->name);
            if (instr->extra > 0) fprintf(out, "[%zu]", instr->extra);
            fprintf(out, "\n");
            break;
        case IR_LABEL: fprintf(out, "label %s:\n", instr->name); break;
        case IR_JUMP: fprintf(out, "  jump %s\n", instr->name); break;
        case IR_JUMP_IF_FALSE: fprintf(out, "  jump_if_false %s\n", instr->name); break;
        case IR_SWITCH_DISPATCH: fprintf(out, "  switch_dispatch end=%s\n", instr->name); break;
        case IR_CASE:
            if (instr->flag) fprintf(out, "case default %s:\n", instr->name);
            else fprintf(out, "case %ld %s:\n", instr->value, instr->name);
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
