#include "native.h"

#include <string.h>

// This is not machine code yet. It is the first native-backend-shaped artifact.
// The target is a tiny stack-machine text format used to design lowering.
// It is generated directly from the flat Pine IR (see ir.h) rather than
// walking the AST a second time, so IR and native output share one lowering.
typedef struct {
    size_t size;
    size_t align;
} NativeLayout;

typedef struct {
    const char *names[256];
    const char *types[256];
    size_t sizes[256];
    size_t aligns[256];
    size_t offsets[256];
    int count;
    size_t frame_size;
    size_t frame_align;
} NativeFrame;

// Writes indentation for nested native debug blocks.
static void native_indent(FILE *out, int indent) {
    for (int i = 0; i < indent; i++) {
        fprintf(out, "  ");
    }
}

// Rounds a byte offset up to the next multiple of the requested alignment.
static size_t native_align_to(size_t value, size_t align) {
    if (align <= 1) {
        return value;
    }

    size_t remainder = value % align;
    if (remainder == 0) {
        return value;
    }

    return value + (align - remainder);
}

// Assigns debug size/alignment for primitive and pointer-like Pine types.
static NativeLayout native_type_layout(const char *type) {
    if (type == NULL) {
        return (NativeLayout){8, 8};
    }

    if (strcmp(type, "void") == 0) return (NativeLayout){0, 1};
    if (strcmp(type, "bool") == 0) return (NativeLayout){1, 1};
    if (strcmp(type, "char") == 0) return (NativeLayout){1, 1};
    if (strcmp(type, "u8") == 0 || strcmp(type, "i8") == 0) return (NativeLayout){1, 1};
    if (strcmp(type, "u16") == 0 || strcmp(type, "i16") == 0) return (NativeLayout){2, 2};
    if (strcmp(type, "u32") == 0 || strcmp(type, "i32") == 0) return (NativeLayout){4, 4};
    if (strcmp(type, "float") == 0 || strcmp(type, "f32") == 0) return (NativeLayout){4, 4};
    if (strcmp(type, "u64") == 0 || strcmp(type, "i64") == 0) return (NativeLayout){8, 8};
    if (strcmp(type, "double") == 0 || strcmp(type, "f64") == 0) return (NativeLayout){8, 8};
    if (strcmp(type, "string") == 0) return (NativeLayout){16, 8};

    size_t length = strlen(type);
    if (length > 0 && type[length - 1] == '*') return (NativeLayout){8, 8};
    if (length > 0 && type[length - 1] == '?') return (NativeLayout){8, 8};
    if (length > 1 && type[0] == '[' && type[1] == ']') return (NativeLayout){16, 8};

    return (NativeLayout){8, 8};
}

// Applies fixed-array size expansion to a base type layout.
static NativeLayout native_slot_layout(const char *type, size_t array_size) {
    NativeLayout layout = native_type_layout(type);
    if (array_size > 0) {
        layout.size *= array_size;
    }
    return layout;
}

// Finds a parameter or local slot in the current function frame.
static int native_find_slot(NativeFrame *frame, const char *name) {
    for (int i = frame->count - 1; i >= 0; i--) {
        if (strcmp(frame->names[i], name) == 0) {
            return i;
        }
    }

    return -1;
}

// Adds a parameter or local slot to the current function frame.
static int native_add_slot(NativeFrame *frame, const char *name, const char *type, size_t array_size) {
    int existing = native_find_slot(frame, name);
    if (existing >= 0) {
        return existing;
    }

    if (frame->count >= 256) {
        return -1;
    }

    int slot = frame->count;
    NativeLayout layout = native_slot_layout(type, array_size);
    size_t offset = native_align_to(frame->frame_size, layout.align);
    frame->names[slot] = name;
    frame->types[slot] = type;
    frame->sizes[slot] = layout.size;
    frame->aligns[slot] = layout.align;
    frame->offsets[slot] = offset;
    frame->count++;
    frame->frame_size = offset + layout.size;
    if (layout.align > frame->frame_align) {
        frame->frame_align = layout.align;
    }
    return slot;
}

// Emits one frame slot declaration in the native debug artifact.
static void native_emit_frame_slot(NativeFrame *frame, FILE *out, int indent, const char *kind, int slot, const char *type, const char *name) {
    native_indent(out, indent);
    if (slot >= 0) {
        fprintf(out, "%s %d %s %s size=%zu align=%zu offset=%zu\n",
                kind, slot, type, name, frame->sizes[slot], frame->aligns[slot], frame->offsets[slot]);
    }
}

// Emits debug layout for one struct declaration.
static void native_emit_struct_layout(IRStruct *s, FILE *out) {
    size_t offset = 0;
    size_t max_align = 1;

    for (size_t i = 0; i < s->field_count; i++) {
        NativeLayout layout = native_type_layout(s->fields[i].field_type);
        offset = native_align_to(offset, layout.align);
        offset += layout.size;
        if (layout.align > max_align) {
            max_align = layout.align;
        }
    }

    size_t struct_size = native_align_to(offset, max_align);
    fprintf(out, "TYPE_STRUCT %s size=%zu align=%zu\n", s->name, struct_size, max_align);

    offset = 0;
    for (size_t i = 0; i < s->field_count; i++) {
        NativeLayout layout = native_type_layout(s->fields[i].field_type);
        offset = native_align_to(offset, layout.align);
        fprintf(out, "  FIELD %s %s offset=%zu size=%zu align=%zu\n",
                s->fields[i].field_type, s->fields[i].name, offset, layout.size, layout.align);
        offset += layout.size;
    }
}

// Converts an IR operator name into a debug native opcode.
static const char *native_op_name(const char *ir_op) {
    if (strcmp(ir_op, "add") == 0) return "ADD";
    if (strcmp(ir_op, "sub") == 0) return "SUB";
    if (strcmp(ir_op, "mul") == 0) return "MUL";
    if (strcmp(ir_op, "div") == 0) return "DIV";
    if (strcmp(ir_op, "mod") == 0) return "MOD";
    if (strcmp(ir_op, "bit_and") == 0) return "BIT_AND";
    if (strcmp(ir_op, "bit_or") == 0) return "BIT_OR";
    if (strcmp(ir_op, "bit_xor") == 0) return "BIT_XOR";
    if (strcmp(ir_op, "shl") == 0) return "SHL";
    if (strcmp(ir_op, "shr") == 0) return "SHR";
    if (strcmp(ir_op, "lt") == 0) return "CMP_LT";
    if (strcmp(ir_op, "lte") == 0) return "CMP_LTE";
    if (strcmp(ir_op, "gt") == 0) return "CMP_GT";
    if (strcmp(ir_op, "gte") == 0) return "CMP_GTE";
    if (strcmp(ir_op, "eq") == 0) return "CMP_EQ";
    if (strcmp(ir_op, "neq") == 0) return "CMP_NEQ";
    if (strcmp(ir_op, "and") == 0) return "LOGIC_AND";
    if (strcmp(ir_op, "or") == 0) return "LOGIC_OR";
    if (strcmp(ir_op, "not") == 0) return "LOGIC_NOT";
    if (strcmp(ir_op, "bit_not") == 0) return "BIT_NOT";
    return "OP";
}

// Emits one IR instruction as native debug stack-machine text. Frame slot
// bookkeeping for IR_DECL_LOCAL is handled by the caller so declaration
// order matches the IR exactly.
static void native_emit_instr(NativeFrame *frame, const IRInstr *instr, FILE *out, int indent) {
    switch (instr->op) {
        case IR_CONST:
            native_indent(out, indent);
            fprintf(out, "PUSH_I64 %ld\n", instr->value);
            break;
        case IR_LITERAL:
            native_indent(out, indent);
            fprintf(out, "PUSH_LITERAL %s\n", instr->name);
            break;
        case IR_NULL:
            native_indent(out, indent);
            fprintf(out, "PUSH_NULL\n");
            break;
        case IR_LOAD: {
            int slot = native_find_slot(frame, instr->name);
            native_indent(out, indent);
            if (slot >= 0) {
                fprintf(out, "LOAD_SLOT %d ; %s\n", slot, instr->name);
            } else {
                fprintf(out, "LOAD_GLOBAL %s\n", instr->name);
            }
            break;
        }
        case IR_STORE: {
            int slot = native_find_slot(frame, instr->name);
            native_indent(out, indent);
            if (slot >= 0) {
                fprintf(out, "STORE_SLOT %d ; %s\n", slot, instr->name);
            } else {
                fprintf(out, "STORE_GLOBAL %s\n", instr->name);
            }
            break;
        }
        case IR_UNARY:
        case IR_BINARY:
            native_indent(out, indent);
            fprintf(out, "%s\n", native_op_name(instr->name));
            break;
        case IR_FIELD:
            native_indent(out, indent);
            fprintf(out, "FIELD %s\n", instr->name);
            break;
        case IR_INDEX:
            native_indent(out, indent);
            if (instr->flag) {
                fprintf(out, "INDEX_BOUNDS_SLICE\n");
            } else if (instr->value > 0) {
                fprintf(out, "INDEX_BOUNDS %ld\n", instr->value);
            } else {
                fprintf(out, "INDEX\n");
            }
            break;
        case IR_CALL:
            native_indent(out, indent);
            fprintf(out, "CALL %s argc=%zu returns=1\n", instr->name, instr->extra);
            break;
        case IR_POP:
            native_indent(out, indent);
            fprintf(out, "POP\n");
            break;
        case IR_RETURN_VALUE:
            native_indent(out, indent);
            fprintf(out, "RETURN_VALUE\n");
            break;
        case IR_RETURN:
            native_indent(out, indent);
            fprintf(out, "RET\n");
            break;
        case IR_DECL_LOCAL:
            // Frame slot registration and the FRAME_LOCAL line are handled
            // by the caller so the slot table stays in sync.
            break;
        case IR_LABEL:
            native_indent(out, indent);
            fprintf(out, "LABEL %s\n", instr->name);
            break;
        case IR_JUMP:
            native_indent(out, indent);
            fprintf(out, "JUMP %s\n", instr->name);
            break;
        case IR_JUMP_IF_FALSE:
            native_indent(out, indent);
            fprintf(out, "JUMP_IF_FALSE %s\n", instr->name);
            break;
        case IR_SWITCH_DISPATCH:
            native_indent(out, indent);
            fprintf(out, "SWITCH_DISPATCH %s\n", instr->name);
            break;
        case IR_CASE:
            native_indent(out, indent);
            fprintf(out, "LABEL %s\n", instr->name);
            break;
        case IR_UNSAFE_BEGIN:
            native_indent(out, indent);
            fprintf(out, "UNSAFE_BEGIN\n");
            break;
        case IR_UNSAFE_END:
            native_indent(out, indent);
            fprintf(out, "UNSAFE_END\n");
            break;
        case IR_BREAK:
            native_indent(out, indent);
            fprintf(out, "BREAK\n");
            break;
        case IR_CONTINUE:
            native_indent(out, indent);
            fprintf(out, "CONTINUE\n");
            break;
        case IR_UNSUPPORTED:
            native_indent(out, indent);
            fprintf(out, "UNSUPPORTED_%s\n", instr->name ? instr->name : "NODE");
            break;
    }
}

// Emits one function in the debug native target from its lowered IR.
static void native_emit_function(IRFunction *fn, FILE *out) {
    NativeFrame frame = {0};
    frame.frame_align = 1;

    fprintf(out, "FUNC %s RET %s\n", fn->name, fn->return_type);
    fprintf(out, "  FRAME_BEGIN %s\n", fn->name);
    for (size_t i = 0; i < fn->param_count; i++) {
        int slot = native_add_slot(&frame, fn->params[i].name, fn->params[i].type, 0);
        native_emit_frame_slot(&frame, out, 1, "FRAME_PARAM", slot, fn->params[i].type, fn->params[i].name);
    }

    for (size_t i = 0; i < fn->instr_count; i++) {
        IRInstr *instr = &fn->instrs[i];
        if (instr->op == IR_DECL_LOCAL) {
            int slot = native_add_slot(&frame, instr->name, instr->type, instr->extra);
            native_emit_frame_slot(&frame, out, 1, "FRAME_LOCAL", slot, instr->type, instr->name);
            if (instr->extra > 0) {
                native_indent(out, 1);
                fprintf(out, "FRAME_SLOT_ARRAY_LENGTH %d %zu\n", slot, instr->extra);
            }
            continue;
        }
        native_emit_instr(&frame, instr, out, 1);
    }

    fprintf(out, "  FRAME_SIZE %zu align=%zu\n", native_align_to(frame.frame_size, frame.frame_align), frame.frame_align);
    fprintf(out, "  FRAME_END %s slots=%d\n", fn->name, frame.count);
    fprintf(out, "END_FUNC\n\n");
}

void native_emit_debug(IRModule *module, FILE *out) {
    fprintf(out, "pine_native_debug 0\n");
    fprintf(out, "target stack-vm-text\n");
    fprintf(out, "abi none\n");
    fprintf(out, "object_format debug-text\n");
    fprintf(out, "control_flow labels-and-jumps\n\n");
    fprintf(out, "calls frame-slots\n\n");
    fprintf(out, "layout debug-primitive-sizes\n");
    fprintf(out, "layout_note sizes_follow_c_backend_assumptions\n\n");
    fprintf(out, "source ir\n\n");

    for (size_t i = 0; i < module->struct_count; i++) {
        native_emit_struct_layout(&module->structs[i], out);
    }

    for (size_t i = 0; i < module->global_count; i++) {
        IRGlobal *g = &module->globals[i];
        NativeLayout layout = native_slot_layout(g->type, g->array_size);
        fprintf(out, "GLOBAL %s %s size=%zu align=%zu\n", g->type, g->name, layout.size, layout.align);
    }
    fprintf(out, "\n");

    for (size_t i = 0; i < module->function_count; i++) {
        native_emit_function(&module->functions[i], out);
    }
}
