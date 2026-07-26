#ifndef IR_H
#define IR_H

#include <stdio.h>
#include "ast.h"

// Pine IR is a flat, stack-machine-shaped instruction list per function.
// Control flow (if/while/for/switch/break/continue) is linearized into
// labels and jumps at this stage, so every later consumer -- the `pine ir`
// dumper and the native debug backend -- shares one lowering instead of
// each walking the AST on its own.
//
// Operand usage per opcode is documented next to each IROpcode value.
typedef enum {
    IR_CONST,             // name unused, value = integer literal
    IR_LITERAL,            // name = literal text (char or string)
    IR_NULL,                 // (no operands)
    IR_LOAD,                  // name = variable name
    IR_STORE,                  // name = variable name (pops preceding value)
    IR_STORE_FIELD,            // name = field; pops object and value
    IR_STORE_INDEX,            // value/flag = bounds metadata; pops object, index, value
    IR_STORE_DEREF,            // pops pointer and value
    IR_ARRAY,                  // extra = element count; pops elements and pushes one aggregate
    IR_STRUCT_FIELD,           // name = field associated with preceding value
    IR_STRUCT_BUILD,           // name = type, extra = field count
    IR_UNARY,                    // name = operator name (pops 1 operand)
    IR_BINARY,                    // name = operator name (pops 2 operands)
    IR_FIELD,                      // name = field name (pops 1 object)
    IR_INDEX,                       // value = checked length (0 if none), flag = checked_is_slice (pops object, index)
    IR_CALL,                         // name = function name, extra = arg count (pops `extra` args)
    IR_POP,                            // discard a computed value used as a statement
    IR_RETURN_VALUE,                     // pops the return value
    IR_RETURN,                            // (no operands)
    IR_DECL_LOCAL,                         // name = var name, type = var type, extra = array_size
    IR_SCOPE_BEGIN,
    IR_SCOPE_END,
    IR_LABEL,                                // name = label
    IR_JUMP,                                  // name = target label
    IR_JUMP_IF_FALSE,                          // name = target label (pops condition)
    IR_JUMP_IF_TRUE,                           // name = target label (pops condition)
    IR_SWITCH_DISPATCH,                         // name = end label (pops switch value; case matching is by the following IR_CASE labels)
    IR_CASE,                                     // name = case label, value = case value, flag = is_default
    IR_UNSAFE_BEGIN,
    IR_UNSAFE_END,
    IR_BREAK,               // used only if break appears outside any tracked loop
    IR_CONTINUE,           // used only if continue appears outside any tracked loop
    IR_UNSUPPORTED         // name = short description of the unhandled AST node
} IROpcode;

typedef struct {
    IROpcode op;
    char *name;    // identifier / label / literal text / operator name / call name, or NULL
    char *type;     // Pine type text, used by IR_DECL_LOCAL only
    long long value;       // literal value / checked length / case value, meaning is opcode-specific
    size_t extra;      // call arg count / decl_local array size
    int flag;            // checked_is_slice / case is_default, meaning is opcode-specific
    // Borrowed source node for IR_UNSUPPORTED; the AST must outlive the IR.
    ASTNode *fallback_ast;
} IRInstr;

typedef struct {
    char *name;
    char *type;
    size_t array_size;
} IRParam;

typedef struct {
    char *name;
    char *return_type;
    IRParam *params;
    size_t param_count;
    IRInstr *instrs;
    size_t instr_count;
    size_t instr_cap;
} IRFunction;

typedef struct {
    char *field_type;
    char *name;
} IRField;

typedef struct {
    char *name;
    IRField *fields;
    size_t field_count;
} IRStruct;

typedef struct {
    char *type;
    char *name;
    size_t array_size;
    IRInstr *init_instrs;   // may be empty if there is no initializer
    size_t init_count;
} IRGlobal;

typedef struct {
    IRStruct *structs;
    size_t struct_count;
    IRGlobal *globals;
    size_t global_count;
    IRFunction *functions;
    size_t function_count;
} IRModule;

// Lowers a semantically checked AST into the flat Pine IR described above.
IRModule *ir_lower_program(ASTNode *root);
// Releases an IR module and everything it owns.
void ir_free_module(IRModule *module);
// Prints the flat instruction-list form of a module (`pine ir`).
void ir_dump_program(IRModule *module, FILE *out);

// Describes the shared operand-stack convention used for expressions.
void ir_instr_stack_effect(const IRInstr *instr, size_t *pops, size_t *pushes);

#endif
