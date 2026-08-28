#include "sema.h"
#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TYPE_UNKNOWN,
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_SLICE,
    TYPE_NULL,
    TYPE_NULLABLE
} TypeKind;

#define TYPE_NESTING_MAX 8

typedef struct {
    TypeKind kind;
    const char *name;
    TypeKind element_kinds[TYPE_NESTING_MAX];
    const char *element_names[TYPE_NESTING_MAX];
    size_t element_depth;
    size_t array_size;
} Type;

typedef struct {
    const char *name;
    Type type;
    int is_const;
    int is_checked_nullable;
    int is_moved;
    const char *source_file;
    int is_public;
} Symbol;

typedef struct Scope Scope;

struct Scope {
    Symbol *symbols;
    size_t count;
    Scope *parent;
};

typedef struct {
    const char *name;
    ASTNode *values;
    const char *source_file;
    int is_public;
} EnumSymbol;

typedef struct {
    const char *name;
    ASTNode *fields;
    const char *source_file;
    int is_public;
} StructSymbol;

typedef struct {
    const char *name;
    Type return_type;
    ASTNode *params;
    const char *source_file;
    int is_public;
} FunctionSymbol;

typedef struct {
    EnumSymbol *enums;
    size_t enum_count;
    StructSymbol *structs;
    size_t struct_count;
    FunctionSymbol *functions;
    size_t function_count;
    Scope *scope;
    Type current_return_type;
    const char *current_source_file;
    int loop_depth;
    int switch_depth;
    int unsafe_depth;
    int errors;
    int warnings;
} Sema;

// Constructors for common semantic Type values.
static Type type_unknown(void) { return (Type){TYPE_UNKNOWN}; }
static Type type_i32(void) { return (Type){TYPE_I32}; }
static Type type_bool(void) { return (Type){TYPE_BOOL}; }
static Type type_struct(const char *name) { return (Type){TYPE_STRUCT, name}; }
static Type type_enum(const char *name) { return (Type){TYPE_ENUM, name}; }
static Type type_null(void) { return (Type){TYPE_NULL}; }
static Type type_wrap(TypeKind kind, Type element) {
    Type type = {0};
    type.kind = kind;
    type.element_kinds[0] = element.kind;
    type.element_names[0] = element.name;
    type.element_depth = element.element_depth + 1;
    if (type.element_depth > TYPE_NESTING_MAX) {
        return type_unknown();
    }
    for (size_t i = 0; i < element.element_depth; i++) {
        type.element_kinds[i + 1] = element.element_kinds[i];
        type.element_names[i + 1] = element.element_names[i];
    }
    return type;
}

static Type type_pointer(Type element) { return type_wrap(TYPE_POINTER, element); }
static Type type_nullable(Type element) { return type_wrap(TYPE_NULLABLE, element); }
static Type type_slice(Type element) { return type_wrap(TYPE_SLICE, element); }

static Type type_array(Type element, size_t size) {
    Type type = type_wrap(TYPE_ARRAY, element);
    type.array_size = size;
    return type;
}

static Type type_element(Type container) {
    if (container.element_depth == 0) return type_unknown();
    Type type = {0};
    type.kind = container.element_kinds[0];
    type.name = container.element_names[0];
    type.element_depth = container.element_depth - 1;
    for (size_t i = 0; i < type.element_depth; i++) {
        type.element_kinds[i] = container.element_kinds[i + 1];
        type.element_names[i] = container.element_names[i + 1];
    }
    return type;
}

static Type array_element_type(Type array) { return type_element(array); }
static Type slice_element_type(Type slice) { return type_element(slice); }
static Type pointer_element_type(Type pointer) { return type_element(pointer); }
static Type nullable_element_type(Type nullable) { return type_element(nullable); }

// Forward declarations used while converting named types.
static EnumSymbol *find_enum(Sema *sema, const char *name);
static StructSymbol *find_struct(Sema *sema, const char *name);
static int sema_can_access(Sema *sema, const char *source_file, int is_public);

// Converts a non-pointer type spelling into a semantic type.
static Type type_from_base_name(Sema *sema, const char *name) {
    if (strcmp(name, "void") == 0) return (Type){TYPE_VOID};
    if (strcmp(name, "bool") == 0) return (Type){TYPE_BOOL};
    if (strcmp(name, "char") == 0) return (Type){TYPE_CHAR};
    if (strcmp(name, "string") == 0) return (Type){TYPE_STRING};
    if (strcmp(name, "float") == 0) return (Type){TYPE_FLOAT};
    if (strcmp(name, "double") == 0) return (Type){TYPE_DOUBLE};
    if (strcmp(name, "i8") == 0) return (Type){TYPE_I8};
    if (strcmp(name, "i16") == 0) return (Type){TYPE_I16};
    if (strcmp(name, "i32") == 0) return (Type){TYPE_I32};
    if (strcmp(name, "int") == 0) return (Type){TYPE_I32};
    if (strcmp(name, "i64") == 0) return (Type){TYPE_I64};
    if (strcmp(name, "u8") == 0) return (Type){TYPE_U8};
    if (strcmp(name, "u16") == 0) return (Type){TYPE_U16};
    if (strcmp(name, "u32") == 0) return (Type){TYPE_U32};
    if (strcmp(name, "u64") == 0) return (Type){TYPE_U64};
    StructSymbol *struct_symbol = find_struct(sema, name);
    if (struct_symbol && sema_can_access(sema, struct_symbol->source_file, struct_symbol->is_public)) {
        return type_struct(name);
    }
    EnumSymbol *enum_symbol = find_enum(sema, name);
    if (enum_symbol && sema_can_access(sema, enum_symbol->source_file, enum_symbol->is_public)) {
        return type_enum(name);
    }
    return type_unknown();
}

// Converts a full Pine type spelling, including pointer stars, into a Type.
static Type type_from_name(Sema *sema, const char *name) {
    size_t full_length = strlen(name);
    if (full_length > 0 && name[full_length - 1] == '?') {
        char inner_name[128];
        if (full_length >= sizeof(inner_name)) {
            return type_unknown();
        }
        memcpy(inner_name, name, full_length - 1);
        inner_name[full_length - 1] = '\0';
        Type inner = type_from_name(sema, inner_name);
        if (inner.kind != TYPE_POINTER) {
            return type_unknown();
        }
        return type_nullable(inner);
    }

    if (strncmp(name, "[]", 2) == 0) {
        Type element = type_from_name(sema, name + 2);
        if (element.kind == TYPE_UNKNOWN) {
            return element;
        }
        if (element.kind == TYPE_VOID || element.kind == TYPE_STRUCT ||
            element.kind == TYPE_POINTER || element.kind == TYPE_SLICE ||
            element.kind == TYPE_ARRAY || element.kind == TYPE_NULLABLE ||
            element.kind == TYPE_STRING) {
            return type_unknown();
        }
        return type_slice(element);
    }

    size_t length = strlen(name);
    size_t pointer_count = 0;

    while (length > 0 && name[length - 1] == '*') {
        pointer_count++;
        length--;
    }

    char base[128];
    if (length >= sizeof(base)) {
        return type_unknown();
    }

    memcpy(base, name, length);
    base[length] = '\0';

    Type type = type_from_base_name(sema, base);
    if (type.kind == TYPE_UNKNOWN) {
        return type;
    }

    for (size_t i = 0; i < pointer_count; i++) {
        type = type_pointer(type);
    }

    return type;
}

// Returns a short display name for diagnostics.
static const char *type_name(Type type) {
    switch (type.kind) {
        case TYPE_VOID: return "void";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return "char";
        case TYPE_STRING: return "string";
        case TYPE_FLOAT: return "float";
        case TYPE_DOUBLE: return "double";
        case TYPE_I8: return "i8";
        case TYPE_I16: return "i16";
        case TYPE_I32: return "i32";
        case TYPE_I64: return "i64";
        case TYPE_U8: return "u8";
        case TYPE_U16: return "u16";
        case TYPE_U32: return "u32";
        case TYPE_U64: return "u64";
        case TYPE_STRUCT: return type.name;
        case TYPE_ENUM: return type.name;
        case TYPE_POINTER: return "pointer";
        case TYPE_ARRAY: return "array";
        case TYPE_SLICE: return "slice";
        case TYPE_NULL: return "null";
        case TYPE_NULLABLE: return "nullable";
        case TYPE_UNKNOWN: return "<unknown>";
    }
    return "<unknown>";
}

// Checks exact type identity, including nested wrappers and array sizes.
static int same_type(Type a, Type b) {
    if (a.kind != b.kind || a.element_depth != b.element_depth) return 0;
    if (a.kind == TYPE_ARRAY && a.array_size != b.array_size) return 0;
    if ((a.kind == TYPE_STRUCT || a.kind == TYPE_ENUM) &&
        (!a.name || !b.name || strcmp(a.name, b.name) != 0)) return 0;
    for (size_t i = 0; i < a.element_depth; i++) {
        if (a.element_kinds[i] != b.element_kinds[i]) return 0;
        const char *an = a.element_names[i];
        const char *bn = b.element_names[i];
        if ((an == NULL) != (bn == NULL)) return 0;
        if (an && strcmp(an, bn) != 0) return 0;
    }
    return 1;
}

// Type classification helpers used by operator checks.
static int is_signed_integer(Type type) {
    return type.kind == TYPE_I8 || type.kind == TYPE_I16 ||
           type.kind == TYPE_I32 || type.kind == TYPE_I64;
}

static int is_unsigned_integer(Type type) {
    return type.kind == TYPE_U8 || type.kind == TYPE_U16 ||
           type.kind == TYPE_U32 || type.kind == TYPE_U64;
}

static int is_integer(Type type) {
    return is_signed_integer(type) || is_unsigned_integer(type);
}

static int is_index_type(Type type) {
    return is_integer(type);
}

static int is_numeric(Type type) {
    return is_integer(type) || type.kind == TYPE_FLOAT || type.kind == TYPE_DOUBLE;
}

// Current copy defaults: scalar, pointer-like, and borrowed view types copy freely.
static int is_copy_type(Type type) {
    switch (type.kind) {
        case TYPE_VOID:
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_STRING:
        case TYPE_FLOAT:
        case TYPE_DOUBLE:
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_I64:
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        case TYPE_U64:
        case TYPE_POINTER:
        case TYPE_SLICE:
        case TYPE_NULL:
        case TYPE_NULLABLE:
            return 1;
        case TYPE_ENUM:
            return 1;
        case TYPE_STRUCT:
        case TYPE_ARRAY:
        case TYPE_UNKNOWN:
            return 0;
    }
    return 0;
}

// Implements Pine's current assignment compatibility rules.
static int is_assignable(Type expected, Type actual) {
    if (same_type(expected, actual)) {
        return 1;
    }

    if (expected.kind == TYPE_NULLABLE) {
        if (actual.kind == TYPE_NULL) {
            return 1;
        }
        return is_assignable(nullable_element_type(expected), actual);
    }

    // Integer literals are represented as i32 until literal width tracking exists.
    if (actual.kind == TYPE_I32 && is_integer(expected)) {
        return 1;
    }

    return 0;
}

// Computes the result type for numeric and integer-like binary operators.
static Type arithmetic_result(Type left, Type right) {
    if (left.kind == TYPE_DOUBLE || right.kind == TYPE_DOUBLE) return (Type){TYPE_DOUBLE};
    if (left.kind == TYPE_FLOAT || right.kind == TYPE_FLOAT) return (Type){TYPE_FLOAT};
    if (same_type(left, right)) return left;
    if (left.kind == TYPE_I32 && is_integer(right)) return right;
    if (right.kind == TYPE_I32 && is_integer(left)) return left;
    return type_unknown();
}

static void sema_print_snippet(ASTNode *node) {
    if (!node || !node->source_file || node->line <= 0) return;
    FILE *file = fopen(node->source_file, "rb");
    if (!file) return;
    char line[1024];
    int current = 1;
    while (fgets(line, sizeof(line), file)) {
        if (current++ != node->line) continue;
        size_t length = strlen(line);
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) line[--length] = '\0';
        fprintf(stderr, "  %s\n  ", line);
        for (int i = 1; i < node->column; i++) fputc(line[i - 1] == '\t' ? '\t' : ' ', stderr);
        fprintf(stderr, "^\n");
        break;
    }
    fclose(file);
}

static int sema_can_access(Sema *sema, const char *source_file, int is_public) {
    return is_public || !source_file || !sema->current_source_file ||
           strcmp(source_file, sema->current_source_file) == 0;
}

// Records a semantic error without source location information.
static void sema_error(Sema *sema, const char *message, const char *name) {
    if (name) {
        fprintf(stderr, "Pine semantic error: %s '%s'\n", message, name);
    } else {
        fprintf(stderr, "Pine semantic error: %s\n", message);
    }
    sema->errors++;
}

// Records a semantic error using an AST node location when available.
static void sema_error_at(Sema *sema, ASTNode *node, const char *message, const char *name) {
    if (node && node->line > 0) {
        if (name) {
            fprintf(stderr, "%s:%d:%d: Pine semantic error: %s '%s'\n",
                    node->source_file ? node->source_file : "<input>",
                    node->line, node->column, message, name);
        } else {
            fprintf(stderr, "%s:%d:%d: Pine semantic error: %s\n",
                    node->source_file ? node->source_file : "<input>",
                    node->line, node->column, message);
        }
        sema_print_snippet(node);
        sema->errors++;
        return;
    }
    sema_error(sema, message, name);
}

// Records a type mismatch without source location information.
static void type_error(Sema *sema, const char *message, Type expected, Type actual) {
    fprintf(stderr, "Pine semantic error: %s: expected %s, got %s\n",
            message, type_name(expected), type_name(actual));
    sema->errors++;
}

// Records a type mismatch using an AST node location when available.
static void type_error_at(Sema *sema, ASTNode *node, const char *message, Type expected, Type actual) {
    if (node && node->line > 0) {
        fprintf(stderr, "%s:%d:%d: Pine semantic error: %s: expected %s, got %s\n",
                node->source_file ? node->source_file : "<input>",
                node->line, node->column, message, type_name(expected), type_name(actual));
        sema_print_snippet(node);
        sema->errors++;
        return;
    }
    type_error(sema, message, expected, actual);
}

// Records a semantic warning using an AST node location when available.
static void sema_warning_at(Sema *sema, ASTNode *node, const char *message, const char *name) {
    if (node && node->line > 0) {
        if (name) {
            fprintf(stderr, "Pine warning at line %d, column %d: %s '%s'\n",
                    node->line, node->column, message, name);
        } else {
            fprintf(stderr, "Pine warning at line %d, column %d: %s\n",
                    node->line, node->column, message);
        }
    } else if (name) {
        fprintf(stderr, "Pine warning: %s '%s'\n", message, name);
    } else {
        fprintf(stderr, "Pine warning: %s\n", message);
    }
    sema->warnings++;
}

// Evaluates the small subset of integer constants needed for bounds checks.
static int constant_integer_value(ASTNode *expr, long long *value) {
    if (expr->type == AST_NUMBER) {
        *value = expr->number.value;
        return 1;
    }

    if (expr->type == AST_UNARY_EXPR && expr->unary.op == TOKEN_MINUS &&
        constant_integer_value(expr->unary.operand, value)) {
        *value = -*value;
        return 1;
    }

    return 0;
}

// Creates a new lexical scope nested under the current scope.
static void push_scope(Sema *sema) {
    Scope *scope = calloc(1, sizeof(Scope));
    scope->parent = sema->scope;
    sema->scope = scope;
}

// Removes the current lexical scope.
static void pop_scope(Sema *sema) {
    Scope *scope = sema->scope;
    sema->scope = scope->parent;
    free(scope->symbols);
    free(scope);
}

// Looks up a name in one scope only.
static Symbol *find_local_symbol(Scope *scope, const char *name) {
    if (!scope) return NULL;
    for (size_t i = 0; i < scope->count; i++) {
        if (strcmp(scope->symbols[i].name, name) == 0) return &scope->symbols[i];
    }
    return NULL;
}

// Looks up a name through the current scope chain.
static Symbol *find_symbol(Sema *sema, const char *name) {
    for (Scope *scope = sema->scope; scope; scope = scope->parent) {
        Symbol *symbol = find_local_symbol(scope, name);
        if (symbol) return symbol;
    }
    return NULL;
}

// Adds a name to the current scope and reports duplicates.
static void add_symbol(Sema *sema, const char *name, Type type, int is_const) {
    if (find_local_symbol(sema->scope, name)) {
        sema_error(sema, "duplicate name", name);
        return;
    }

    size_t next = sema->scope->count + 1;
    sema->scope->symbols = realloc(sema->scope->symbols, next * sizeof(Symbol));
    sema->scope->symbols[sema->scope->count] =
        (Symbol){name, type, is_const, 0, 0, sema->current_source_file, 0};
    sema->scope->count = next;
}

static void add_top_symbol(Sema *sema, const char *name, Type type, int is_const,
                           const char *source_file, int is_public) {
    add_symbol(sema, name, type, is_const);
    Symbol *symbol = find_local_symbol(sema->scope, name);
    if (symbol) {
        symbol->source_file = source_file;
        symbol->is_public = is_public;
    }
}

// Adds a non-null checked shadow for a nullable symbol inside a narrowed scope.
static void add_checked_nullable_symbol(Sema *sema, const char *name, Type nullable_type) {
    Type inner = nullable_element_type(nullable_type);
    size_t next = sema->scope->count + 1;
    sema->scope->symbols = realloc(sema->scope->symbols, next * sizeof(Symbol));
    sema->scope->symbols[sema->scope->count] =
        (Symbol){name, inner, 0, 1, 0, sema->current_source_file, 0};
    sema->scope->count = next;
}

// Marks an explicitly moved non-copy symbol so later reads can be rejected.
static void mark_moved_if_needed(Sema *sema, ASTNode *expr) {
    if (!expr || expr->type != AST_IDENTIFIER) {
        sema_error_at(sema, expr, "move requires a named value", NULL);
        return;
    }

    Symbol *symbol = find_symbol(sema, expr->identifier.name);
    if (!symbol) {
        sema_error_at(sema, expr, "unknown identifier", expr->identifier.name);
        return;
    }

    if (!is_copy_type(symbol->type)) {
        symbol->is_moved = 1;
    }
}

typedef struct {
    Symbol **symbols;
    int *moved;
    size_t count;
} MoveState;

static MoveState capture_move_state(Sema *sema) {
    MoveState state = {0};
    for (Scope *scope = sema->scope; scope; scope = scope->parent) state.count += scope->count;
    state.symbols = state.count ? malloc(state.count * sizeof(Symbol *)) : NULL;
    state.moved = state.count ? malloc(state.count * sizeof(int)) : NULL;
    size_t index = 0;
    for (Scope *scope = sema->scope; scope; scope = scope->parent) {
        for (size_t i = 0; i < scope->count; i++) {
            state.symbols[index] = &scope->symbols[i];
            state.moved[index] = scope->symbols[i].is_moved;
            index++;
        }
    }
    return state;
}

static void restore_move_state(const MoveState *state) {
    for (size_t i = 0; i < state->count; i++) state->symbols[i]->is_moved = state->moved[i];
}

static void merge_move_states(const MoveState *left, const MoveState *right) {
    for (size_t i = 0; i < left->count; i++) {
        left->symbols[i]->is_moved = left->moved[i] || right->moved[i];
    }
}

static void free_move_state(MoveState *state) {
    free(state->symbols);
    free(state->moved);
    *state = (MoveState){0};
}

static EnumSymbol *find_enum(Sema *sema, const char *name) {
    for (size_t i = 0; i < sema->enum_count; i++) {
        if (strcmp(sema->enums[i].name, name) == 0) return &sema->enums[i];
    }
    return NULL;
}

static void collect_enums(Sema *sema, ASTNode *root) {
    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *node = root->list.items[i];
        if (node->type != AST_ENUM_DECL) continue;
        if (find_enum(sema, node->enum_decl.name)) {
            sema_error_at(sema, node, "duplicate enum", node->enum_decl.name);
            continue;
        }
        size_t next = sema->enum_count + 1;
        sema->enums = realloc(sema->enums, next * sizeof(EnumSymbol));
        sema->enums[sema->enum_count++] =
            (EnumSymbol){node->enum_decl.name, node->enum_decl.values, node->source_file, node->is_public};
        for (size_t j = 0; j < node->enum_decl.values->list.count; j++) {
            ASTNode *value = node->enum_decl.values->list.items[j];
            for (size_t k = j + 1; k < node->enum_decl.values->list.count; k++) {
                ASTNode *other = node->enum_decl.values->list.items[k];
                if (strcmp(value->enum_value.name, other->enum_value.name) == 0) {
                    sema_error_at(sema, other, "duplicate enum value", other->enum_value.name);
                }
            }
        }
    }
}

static void add_enum_values(Sema *sema) {
    for (size_t i = 0; i < sema->enum_count; i++) {
        EnumSymbol *symbol = &sema->enums[i];
        for (size_t j = 0; j < symbol->values->list.count; j++) {
            ASTNode *value = symbol->values->list.items[j];
            add_top_symbol(sema, value->enum_value.name, type_enum(symbol->name), 1,
                           symbol->source_file, symbol->is_public);
        }
    }
}

// Finds a collected function signature by name.
static FunctionSymbol *find_function(Sema *sema, const char *name) {
    for (size_t i = 0; i < sema->function_count; i++) {
        if (strcmp(sema->functions[i].name, name) == 0) return &sema->functions[i];
    }
    return NULL;
}

// Finds a collected struct declaration by name.
static StructSymbol *find_struct(Sema *sema, const char *name) {
    for (size_t i = 0; i < sema->struct_count; i++) {
        if (strcmp(sema->structs[i].name, name) == 0) return &sema->structs[i];
    }
    return NULL;
}

// Finds a field declaration inside a struct.
static ASTNode *find_struct_field(StructSymbol *struct_symbol, const char *field_name) {
    for (size_t i = 0; i < struct_symbol->fields->list.count; i++) {
        ASTNode *field = struct_symbol->fields->list.items[i];
        if (strcmp(field->field_decl.name, field_name) == 0) return field;
    }
    return NULL;
}

// Registers all struct names before checking fields and functions.
static void collect_structs(Sema *sema, ASTNode *root) {
    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *node = root->list.items[i];
        if (node->type != AST_STRUCT_DECL) {
            continue;
        }

        if (find_struct(sema, node->struct_decl.name)) {
            sema_error(sema, "duplicate struct", node->struct_decl.name);
            continue;
        }

        size_t next = sema->struct_count + 1;
        sema->structs = realloc(sema->structs, next * sizeof(StructSymbol));
        sema->structs[sema->struct_count] = (StructSymbol){
            node->struct_decl.name,
            node->struct_decl.fields,
            node->source_file,
            node->is_public
        };
        sema->struct_count = next;
    }
}

// Checks struct field types and duplicate field names.
static void analyze_structs(Sema *sema) {
    for (size_t i = 0; i < sema->struct_count; i++) {
        StructSymbol *struct_symbol = &sema->structs[i];
        sema->current_source_file = struct_symbol->source_file;

        for (size_t j = 0; j < struct_symbol->fields->list.count; j++) {
            ASTNode *field = struct_symbol->fields->list.items[j];
            Type field_type = type_from_name(sema, field->field_decl.field_type);
            if (field_type.kind == TYPE_UNKNOWN) {
                sema_error(sema, "unknown struct field type", field->field_decl.field_type);
            }

            for (size_t k = j + 1; k < struct_symbol->fields->list.count; k++) {
                ASTNode *other = struct_symbol->fields->list.items[k];
                if (strcmp(field->field_decl.name, other->field_decl.name) == 0) {
                    sema_error(sema, "duplicate struct field", field->field_decl.name);
                }
            }
        }
    }
}

// Registers function signatures before checking function bodies.
static void collect_functions(Sema *sema, ASTNode *root) {
    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *fn = root->list.items[i];
        if (fn->type != AST_FUNCTION) {
            continue;
        }
        if (find_function(sema, fn->function.name)) {
            sema_error(sema, "duplicate function", fn->function.name);
            continue;
        }
        if (find_local_symbol(sema->scope, fn->function.name)) {
            sema_error(sema, "duplicate top-level name", fn->function.name);
            continue;
        }

        sema->current_source_file = fn->source_file;
        Type return_type = type_from_name(sema, fn->function.return_type);
        if (return_type.kind == TYPE_UNKNOWN) {
            sema_error(sema, "unknown function return type", fn->function.return_type);
        }

        size_t next = sema->function_count + 1;
        sema->functions = realloc(sema->functions, next * sizeof(FunctionSymbol));
        sema->functions[sema->function_count] = (FunctionSymbol){
            fn->function.name,
            return_type,
            fn->function.params,
            fn->source_file,
            fn->is_public
        };
        sema->function_count = next;
    }
}

// Forward declaration for recursive expression analysis.
static Type analyze_expr(Sema *sema, ASTNode *expr);

// Accepts the expression forms currently allowed in global initializers.
static int is_constant_initializer(ASTNode *expr) {
    if (!expr) {
        return 1;
    }

    switch (expr->type) {
        case AST_NUMBER:
        case AST_CHAR_LITERAL:
        case AST_STRING_LITERAL:
        case AST_NULL_LITERAL:
        case AST_BOOL_LITERAL:
            return 1;
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < expr->list.count; i++) {
                if (!is_constant_initializer(expr->list.items[i])) return 0;
            }
            return 1;
        case AST_STRUCT_LITERAL:
            for (size_t i = 0; i < expr->struct_literal.fields->list.count; i++) {
                if (!is_constant_initializer(expr->struct_literal.fields->list.items[i]->field_init.value)) return 0;
            }
            return 1;
        case AST_UNARY_EXPR:
            return (expr->unary.op == TOKEN_MINUS || expr->unary.op == TOKEN_TILDE) &&
                   is_constant_initializer(expr->unary.operand);
        case AST_BINARY_EXPR:
            switch (expr->binary.op) {
                case TOKEN_PLUS:
                case TOKEN_MINUS:
                case TOKEN_STAR:
                case TOKEN_SLASH:
                case TOKEN_PERCENT:
                case TOKEN_AND:
                case TOKEN_OR:
                case TOKEN_XOR:
                case TOKEN_LSHIFT:
                case TOKEN_RSHIFT:
                    return is_constant_initializer(expr->binary.left) &&
                           is_constant_initializer(expr->binary.right);
                default:
                    return 0;
            }
        default:
            return 0;
    }
}

// Type-checks a unary expression and returns its result type.
static Type analyze_unary(Sema *sema, ASTNode *expr) {
    Type operand = analyze_expr(sema, expr->unary.operand);
    switch (expr->unary.op) {
        case TOKEN_MINUS:
            if (!is_numeric(operand)) {
                sema_error_at(sema, expr, "unary '-' requires numeric operand", NULL);
                return type_unknown();
            }
            if (is_unsigned_integer(operand)) {
                sema_error_at(sema, expr, "unary '-' cannot be used on unsigned integer", NULL);
            }
            return operand;
        case TOKEN_BANG:
            if (operand.kind != TYPE_BOOL) {
                type_error_at(sema, expr, "unary '!' operand type mismatch", type_bool(), operand);
            }
            return type_bool();
        case TOKEN_TILDE:
            if (!is_integer(operand)) {
                sema_error_at(sema, expr, "bitwise '~' requires integer operand", NULL);
                return type_unknown();
            }
            return operand;
        case TOKEN_AND:
            if (expr->unary.operand->type != AST_IDENTIFIER &&
                expr->unary.operand->type != AST_FIELD_EXPR &&
                expr->unary.operand->type != AST_INDEX_EXPR &&
                !(expr->unary.operand->type == AST_UNARY_EXPR &&
                  expr->unary.operand->unary.op == TOKEN_STAR)) {
                sema_error_at(sema, expr, "address-of requires an assignable value", NULL);
                return type_unknown();
            }
            if (operand.kind == TYPE_VOID) {
                sema_error_at(sema, expr, "cannot take address of void value", NULL);
                return type_unknown();
            }
            return type_pointer(operand);
        case TOKEN_STAR:
            if (sema->unsafe_depth == 0) {
                sema_error_at(sema, expr, "raw pointer dereference requires unsafe", NULL);
            }
            if (operand.kind != TYPE_POINTER) {
                sema_error_at(sema, expr, "dereference requires pointer operand", NULL);
                return type_unknown();
            }
            return pointer_element_type(operand);
        default:
            sema_error(sema, "unknown unary operator", NULL);
            return type_unknown();
    }
}

// Type-checks a binary expression and returns its result type.
static Type analyze_binary(Sema *sema, ASTNode *expr) {
    if ((expr->binary.op == TOKEN_EQ || expr->binary.op == TOKEN_NEQ) &&
        ((expr->binary.left->type == AST_NULL_LITERAL && expr->binary.right->type == AST_IDENTIFIER) ||
         (expr->binary.right->type == AST_NULL_LITERAL && expr->binary.left->type == AST_IDENTIFIER))) {
        ASTNode *ident = expr->binary.left->type == AST_IDENTIFIER ? expr->binary.left : expr->binary.right;
        Symbol *symbol = find_symbol(sema, ident->identifier.name);
        if (!symbol) {
            sema_error_at(sema, ident, "unknown identifier", ident->identifier.name);
            return type_bool();
        }
        if (symbol->type.kind != TYPE_NULLABLE) {
            sema_error_at(sema, expr, "null check requires nullable value", ident->identifier.name);
        }
        return type_bool();
    }

    Type left = analyze_expr(sema, expr->binary.left);
    Type right = analyze_expr(sema, expr->binary.right);

    switch (expr->binary.op) {
        case TOKEN_AND_AND:
        case TOKEN_OR_OR:
            if (left.kind != TYPE_BOOL) type_error_at(sema, expr->binary.left, "left logical operand type mismatch", type_bool(), left);
            if (right.kind != TYPE_BOOL) type_error_at(sema, expr->binary.right, "right logical operand type mismatch", type_bool(), right);
            return type_bool();
        case TOKEN_LT:
        case TOKEN_LTE:
        case TOKEN_GT:
        case TOKEN_GTE:
            if (!is_numeric(left) || !is_numeric(right)) {
                sema_error_at(sema, expr, "ordered comparison requires numeric operands", NULL);
            }
            if (!is_assignable(left, right) && !is_assignable(right, left)) {
                sema_error_at(sema, expr, "comparison between incompatible types", NULL);
            }
            return type_bool();
        case TOKEN_EQ:
        case TOKEN_NEQ:
            if (!is_assignable(left, right) && !is_assignable(right, left)) {
                sema_error_at(sema, expr, "equality comparison between incompatible types", NULL);
            }
            return type_bool();
        case TOKEN_PERCENT:
            if (!is_integer(left) || !is_integer(right)) {
                sema_error_at(sema, expr, "'%' requires integer operands", NULL);
                return type_unknown();
            }
            return arithmetic_result(left, right);
        case TOKEN_AND:
        case TOKEN_OR:
        case TOKEN_XOR:
            if (!is_integer(left) || !is_integer(right)) {
                sema_error_at(sema, expr, "bitwise operator requires integer operands", NULL);
                return type_unknown();
            }
            return arithmetic_result(left, right);
        case TOKEN_LSHIFT:
        case TOKEN_RSHIFT:
            if (!is_integer(left) || !is_integer(right)) {
                sema_error_at(sema, expr, "shift operator requires integer operands", NULL);
                return type_unknown();
            }
            return left;
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH: {
            if (!is_numeric(left) || !is_numeric(right)) {
                sema_error_at(sema, expr, "arithmetic requires numeric operands", NULL);
                return type_unknown();
            }
            Type result = arithmetic_result(left, right);
            if (result.kind == TYPE_UNKNOWN) {
                sema_error_at(sema, expr, "arithmetic between incompatible types", NULL);
            }
            return result;
        }
        default:
            sema_error(sema, "unknown binary operator", NULL);
            return type_unknown();
    }
}

// Type-checks a function call against the collected signature.
static Type analyze_call(Sema *sema, ASTNode *expr) {
    if (strcmp(expr->call.name, "move") == 0) {
        if (expr->call.args->list.count != 1) {
            sema_error_at(sema, expr, "move expects one argument", NULL);
            return type_unknown();
        }

        ASTNode *arg = expr->call.args->list.items[0];
        Type type = analyze_expr(sema, arg);
        mark_moved_if_needed(sema, arg);
        return type;
    }

    FunctionSymbol *fn = find_function(sema, expr->call.name);
    if (!fn) {
        sema_error_at(sema, expr, "unknown function", expr->call.name);
        return type_unknown();
    }
    if (!sema_can_access(sema, fn->source_file, fn->is_public)) {
        sema_error_at(sema, expr, "function is private to another module", expr->call.name);
    }

    if (fn->params->list.count != expr->call.args->list.count) {
        sema_error_at(sema, expr, "wrong number of arguments for function", expr->call.name);
        return fn->return_type;
    }

    for (size_t i = 0; i < expr->call.args->list.count; i++) {
        ASTNode *param = fn->params->list.items[i];
        Type expected = type_from_name(sema, param->param.param_type);
        if (param->param.array_size > 0) expected = type_array(expected, param->param.array_size);
        Type actual = analyze_expr(sema, expr->call.args->list.items[i]);

        if (!is_assignable(expected, actual)) {
            type_error_at(sema, expr->call.args->list.items[i], "argument type mismatch", expected, actual);
        }
    }

    return fn->return_type;
}

// Type-checks any expression node and returns its result type.
static Type analyze_expr(Sema *sema, ASTNode *expr) {
    switch (expr->type) {
        case AST_NUMBER:
            return type_i32();
        case AST_CHAR_LITERAL:
            return (Type){TYPE_CHAR};
        case AST_STRING_LITERAL:
            return (Type){TYPE_STRING};
        case AST_NULL_LITERAL:
            return type_null();
        case AST_BOOL_LITERAL:
            return type_bool();
        case AST_ARRAY_LITERAL: {
            if (expr->list.count == 0) {
                sema_error_at(sema, expr, "array initializer cannot be empty", NULL);
                return type_unknown();
            }
            Type element = analyze_expr(sema, expr->list.items[0]);
            for (size_t i = 1; i < expr->list.count; i++) {
                Type actual = analyze_expr(sema, expr->list.items[i]);
                if (!is_assignable(element, actual)) {
                    type_error_at(sema, expr->list.items[i], "array initializer element mismatch", element, actual);
                }
            }
            return type_array(element, expr->list.count);
        }
        case AST_STRUCT_LITERAL: {
            StructSymbol *symbol = find_struct(sema, expr->struct_literal.type_name);
            if (!symbol) {
                sema_error_at(sema, expr, "unknown struct literal type", expr->struct_literal.type_name);
                return type_unknown();
            }
            for (size_t i = 0; i < expr->struct_literal.fields->list.count; i++) {
                ASTNode *init = expr->struct_literal.fields->list.items[i];
                ASTNode *field = find_struct_field(symbol, init->field_init.name);
                if (!field) {
                    sema_error_at(sema, init, "unknown struct literal field", init->field_init.name);
                    analyze_expr(sema, init->field_init.value);
                    continue;
                }
                for (size_t j = i + 1; j < expr->struct_literal.fields->list.count; j++) {
                    ASTNode *other = expr->struct_literal.fields->list.items[j];
                    if (strcmp(init->field_init.name, other->field_init.name) == 0) {
                        sema_error_at(sema, other, "duplicate struct literal field", other->field_init.name);
                    }
                }
                Type expected = type_from_name(sema, field->field_decl.field_type);
                Type actual = analyze_expr(sema, init->field_init.value);
                if (!is_assignable(expected, actual)) {
                    type_error_at(sema, init, "struct literal field mismatch", expected, actual);
                }
            }
            for (size_t i = 0; i < symbol->fields->list.count; i++) {
                ASTNode *field = symbol->fields->list.items[i];
                int found = 0;
                for (size_t j = 0; j < expr->struct_literal.fields->list.count; j++) {
                    ASTNode *init = expr->struct_literal.fields->list.items[j];
                    if (strcmp(field->field_decl.name, init->field_init.name) == 0) found = 1;
                }
                if (!found) sema_error_at(sema, expr, "missing struct literal field", field->field_decl.name);
            }
            return type_struct(symbol->name);
        }
        case AST_IDENTIFIER: {
            Symbol *symbol = find_symbol(sema, expr->identifier.name);
            if (!symbol) {
                sema_error_at(sema, expr, "unknown identifier", expr->identifier.name);
                return type_unknown();
            }
            if (!sema_can_access(sema, symbol->source_file, symbol->is_public)) {
                sema_error_at(sema, expr, "name is private to another module", expr->identifier.name);
            }
            if (symbol->is_moved) {
                sema_error_at(sema, expr, "use after move", expr->identifier.name);
                return symbol->type;
            }
            if (symbol->type.kind == TYPE_NULLABLE && !symbol->is_checked_nullable) {
                sema_error_at(sema, expr, "nullable value must be checked before use", expr->identifier.name);
                return nullable_element_type(symbol->type);
            }
            return symbol->type;
        }
        case AST_UNARY_EXPR:
            return analyze_unary(sema, expr);
        case AST_BINARY_EXPR:
            return analyze_binary(sema, expr);
        case AST_FIELD_EXPR: {
            Type object = analyze_expr(sema, expr->field.object);
            if (object.kind != TYPE_STRUCT) {
                sema_error_at(sema, expr, "field access requires struct value", expr->field.field);
                return type_unknown();
            }

            StructSymbol *struct_symbol = find_struct(sema, object.name);
            if (!struct_symbol) {
                sema_error_at(sema, expr, "unknown struct type", object.name);
                return type_unknown();
            }

            ASTNode *field = find_struct_field(struct_symbol, expr->field.field);
            if (!field) {
                sema_error_at(sema, expr, "unknown struct field", expr->field.field);
                return type_unknown();
            }

            return type_from_name(sema, field->field_decl.field_type);
        }
        case AST_INDEX_EXPR: {
            Type object = analyze_expr(sema, expr->index.object);
            Type index = analyze_expr(sema, expr->index.index);

            if (object.kind != TYPE_ARRAY && object.kind != TYPE_SLICE) {
                sema_error_at(sema, expr, "indexing requires array or slice value", NULL);
                return type_unknown();
            }

            if (!is_index_type(index)) {
                type_error_at(sema, expr->index.index, "array index type mismatch", type_i32(), index);
            }

            if (object.kind == TYPE_ARRAY) {
                expr->index.checked_length = object.array_size;
                long long constant_index = 0;
                if (constant_integer_value(expr->index.index, &constant_index) &&
                    (constant_index < 0 || (size_t)constant_index >= object.array_size)) {
                    sema_error_at(sema, expr, "array index out of bounds", NULL);
                }
                return array_element_type(object);
            }

            expr->index.checked_is_slice = 1;
            Type element = slice_element_type(object);
            const char *element_name = type_name(element);
            size_t name_length = strlen(element_name);
            expr->index.checked_element_type = malloc(name_length + 1);
            memcpy(expr->index.checked_element_type, element_name, name_length + 1);
            return element;
        }
        case AST_CALL_EXPR:
            return analyze_call(sema, expr);
        default:
            sema_error(sema, "unsupported expression in semantic analyzer", NULL);
            return type_unknown();
    }
}

// Forward declaration for recursive statement/block analysis.
static int analyze_block(Sema *sema, ASTNode *block);

// Detects `name != null` or `name == null` and returns the name that was checked.
static const char *nullable_check_name(ASTNode *condition, int want_not_null) {
    if (!condition || condition->type != AST_BINARY_EXPR) {
        return NULL;
    }
    if (want_not_null && condition->binary.op != TOKEN_NEQ) {
        return NULL;
    }
    if (!want_not_null && condition->binary.op != TOKEN_EQ) {
        return NULL;
    }
    if (condition->binary.left->type == AST_IDENTIFIER && condition->binary.right->type == AST_NULL_LITERAL) {
        return condition->binary.left->identifier.name;
    }
    if (condition->binary.right->type == AST_IDENTIFIER && condition->binary.left->type == AST_NULL_LITERAL) {
        return condition->binary.right->identifier.name;
    }
    return NULL;
}

static Symbol *lvalue_root_symbol(Sema *sema, ASTNode *target) {
    if (!target) return NULL;
    if (target->type == AST_IDENTIFIER) return find_symbol(sema, target->identifier.name);
    if (target->type == AST_FIELD_EXPR) return lvalue_root_symbol(sema, target->field.object);
    if (target->type == AST_INDEX_EXPR) return lvalue_root_symbol(sema, target->index.object);
    return NULL;
}

static Type analyze_lvalue(Sema *sema, ASTNode *target) {
    if (target->type == AST_IDENTIFIER) {
        Symbol *symbol = find_symbol(sema, target->identifier.name);
        if (!symbol) {
            sema_error_at(sema, target, "unknown identifier", target->identifier.name);
            return type_unknown();
        }
        return symbol->type;
    }
    if (target->type == AST_FIELD_EXPR || target->type == AST_INDEX_EXPR ||
        (target->type == AST_UNARY_EXPR && target->unary.op == TOKEN_STAR)) {
        return analyze_expr(sema, target);
    }
    sema_error_at(sema, target, "expression is not assignable", NULL);
    return type_unknown();
}

// Checks one statement and returns whether it definitely returns.
static int analyze_statement(Sema *sema, ASTNode *stmt) {
    switch (stmt->type) {
        case AST_VAR_DECL: {
            Type declared = type_from_name(sema, stmt->var_decl.var_type);
            if (declared.kind == TYPE_UNKNOWN) {
                sema_error_at(sema, stmt, "unknown variable type", stmt->var_decl.var_type);
            }
            if (stmt->var_decl.array_size > 0) {
                if (declared.kind == TYPE_VOID) {
                    sema_error_at(sema, stmt, "array element type cannot be void", stmt->var_decl.name);
                }
                declared = type_array(declared, stmt->var_decl.array_size);
            }
            if (stmt->var_decl.value) {
                Type actual = analyze_expr(sema, stmt->var_decl.value);
                if (declared.kind == TYPE_ARRAY && stmt->var_decl.value->type != AST_ARRAY_LITERAL) {
                    sema_error_at(sema, stmt, "fixed arrays require an array initializer literal", NULL);
                }
                if (!is_assignable(declared, actual)) {
                    type_error_at(sema, stmt, "initializer type mismatch", declared, actual);
                }
            }
            add_symbol(sema, stmt->var_decl.name, declared, stmt->var_decl.is_const);
            return 0;
        }
        case AST_ASSIGN_STMT: {
            Symbol *root = lvalue_root_symbol(sema, stmt->assign.target);
            if (root && root->is_const) {
                sema_error_at(sema, stmt, "cannot assign through const value", root->name);
            }
            Type expected = analyze_lvalue(sema, stmt->assign.target);
            Type actual = analyze_expr(sema, stmt->assign.value);
            if (expected.kind == TYPE_ARRAY && stmt->assign.target->type == AST_IDENTIFIER) {
                sema_error_at(sema, stmt, "whole fixed-array assignment is not supported", NULL);
            }
            if (!is_assignable(expected, actual)) {
                type_error_at(sema, stmt, "assignment type mismatch", expected, actual);
            }
            if (root && stmt->assign.target->type == AST_IDENTIFIER) root->is_moved = 0;
            return 0;
        }
        case AST_EXPR_STMT:
            analyze_expr(sema, stmt->expr_stmt.expr);
            return 0;
        case AST_RETURN_STMT: {
            if (!stmt->ret.value) {
                if (sema->current_return_type.kind != TYPE_VOID) {
                    sema_error_at(sema, stmt, "non-void function must return a value", NULL);
                }
                return 1;
            }
            Type actual = analyze_expr(sema, stmt->ret.value);
            if (sema->current_return_type.kind == TYPE_VOID) {
                sema_error_at(sema, stmt, "void function cannot return a value", NULL);
            } else if (!is_assignable(sema->current_return_type, actual)) {
                type_error_at(sema, stmt, "return type mismatch", sema->current_return_type, actual);
            }
            return 1;
        }
        case AST_IF_STMT: {
            Type condition = analyze_expr(sema, stmt->if_stmt.condition);
            if (condition.kind != TYPE_BOOL) {
                type_error_at(sema, stmt->if_stmt.condition, "if condition type mismatch", type_bool(), condition);
            }
            const char *checked_then = nullable_check_name(stmt->if_stmt.condition, 1);
            const char *checked_else = nullable_check_name(stmt->if_stmt.condition, 0);
            MoveState before = capture_move_state(sema);
            int then_returns = 0;
            int else_returns = 0;

            if (checked_then) {
                Symbol *symbol = find_symbol(sema, checked_then);
                push_scope(sema);
                if (symbol && symbol->type.kind == TYPE_NULLABLE) {
                    add_checked_nullable_symbol(sema, checked_then, symbol->type);
                }
                then_returns = analyze_block(sema, stmt->if_stmt.then_block);
                pop_scope(sema);
            } else {
                then_returns = analyze_block(sema, stmt->if_stmt.then_block);
            }
            MoveState then_state = capture_move_state(sema);
            restore_move_state(&before);

            if (stmt->if_stmt.else_block) {
                if (checked_else) {
                    Symbol *symbol = find_symbol(sema, checked_else);
                    push_scope(sema);
                    if (symbol && symbol->type.kind == TYPE_NULLABLE) {
                        add_checked_nullable_symbol(sema, checked_else, symbol->type);
                    }
                    else_returns = analyze_block(sema, stmt->if_stmt.else_block);
                    pop_scope(sema);
                } else {
                    else_returns = analyze_block(sema, stmt->if_stmt.else_block);
                }
            }
            MoveState else_state = capture_move_state(sema);
            merge_move_states(&then_state, &else_state);
            free_move_state(&before);
            free_move_state(&then_state);
            free_move_state(&else_state);
            return then_returns && else_returns;
        }
        case AST_WHILE_STMT: {
            Type condition = analyze_expr(sema, stmt->while_stmt.condition);
            if (condition.kind != TYPE_BOOL) {
                type_error_at(sema, stmt->while_stmt.condition, "while condition type mismatch", type_bool(), condition);
            }
            sema->loop_depth++;
            analyze_block(sema, stmt->while_stmt.body);
            sema->loop_depth--;
            return 0;
        }
        case AST_FOR_STMT: {
            push_scope(sema);
            if (stmt->for_stmt.init) {
                analyze_statement(sema, stmt->for_stmt.init);
            }
            if (stmt->for_stmt.condition) {
                Type condition = analyze_expr(sema, stmt->for_stmt.condition);
                if (condition.kind != TYPE_BOOL) {
                    type_error_at(sema, stmt->for_stmt.condition, "for condition type mismatch", type_bool(), condition);
                }
            }
            sema->loop_depth++;
            analyze_block(sema, stmt->for_stmt.body);
            if (stmt->for_stmt.step) {
                analyze_statement(sema, stmt->for_stmt.step);
            }
            sema->loop_depth--;
            pop_scope(sema);
            return 0;
        }
        case AST_SWITCH_STMT: {
            Type switch_type = analyze_expr(sema, stmt->switch_stmt.expr);
            if (!is_integer(switch_type) && switch_type.kind != TYPE_ENUM) {
                sema_error_at(sema, stmt->switch_stmt.expr, "switch expression must be integer", NULL);
            }

            int seen_default = 0;
            for (size_t i = 0; i < stmt->switch_stmt.cases->list.count; i++) {
                ASTNode *case_node = stmt->switch_stmt.cases->list.items[i];
                if (case_node->case_stmt.is_default) {
                    if (seen_default) {
                        sema_error_at(sema, case_node, "duplicate default label", NULL);
                    }
                    seen_default = 1;
                } else {
                    for (size_t j = i + 1; j < stmt->switch_stmt.cases->list.count; j++) {
                        ASTNode *other = stmt->switch_stmt.cases->list.items[j];
                        if (!other->case_stmt.is_default &&
                            other->case_stmt.value == case_node->case_stmt.value) {
                            sema_error_at(sema, other, "duplicate case label", NULL);
                        }
                    }
                }
            }

            MoveState before = capture_move_state(sema);
            MoveState merged = capture_move_state(sema);
            sema->switch_depth++;
            for (size_t i = 0; i < stmt->switch_stmt.cases->list.count; i++) {
                restore_move_state(&before);
                ASTNode *case_node = stmt->switch_stmt.cases->list.items[i];
                analyze_block(sema, case_node->case_stmt.body);
                MoveState branch = capture_move_state(sema);
                for (size_t j = 0; j < merged.count; j++) {
                    merged.moved[j] = merged.moved[j] || branch.moved[j];
                }
                free_move_state(&branch);
            }
            sema->switch_depth--;
            restore_move_state(&merged);
            free_move_state(&before);
            free_move_state(&merged);
            return 0;
        }
        case AST_BREAK_STMT:
            if (sema->loop_depth == 0 && sema->switch_depth == 0) {
                sema_error_at(sema, stmt, "break used outside loop or switch", NULL);
            }
            return 0;
        case AST_CONTINUE_STMT:
            if (sema->loop_depth == 0) sema_error_at(sema, stmt, "continue used outside loop", NULL);
            return 0;
        case AST_UNSAFE_BLOCK: {
            sema->unsafe_depth++;
            int returns = analyze_block(sema, stmt->unsafe_block.body);
            sema->unsafe_depth--;
            return returns;
        }
        default:
            sema_error_at(sema, stmt, "unsupported statement in semantic analyzer", NULL);
            return 0;
    }
}

// Checks a block in a nested lexical scope.
static int analyze_block(Sema *sema, ASTNode *block) {
    int returns = 0;
    push_scope(sema);
    for (size_t i = 0; i < block->list.count; i++) {
        if (returns) {
            sema_warning_at(sema, block->list.items[i], "unreachable statement after return", NULL);
            analyze_statement(sema, block->list.items[i]);
            continue;
        }
        returns = analyze_statement(sema, block->list.items[i]);
    }
    pop_scope(sema);
    return returns;
}

// Checks a global variable or const declaration and registers its symbol.
static void analyze_global(Sema *sema, ASTNode *global) {
    sema->current_source_file = global->source_file;
    Type declared = type_from_name(sema, global->var_decl.var_type);
    if (declared.kind == TYPE_UNKNOWN) {
        sema_error_at(sema, global, "unknown global type", global->var_decl.var_type);
    }
    if (global->var_decl.array_size > 0) {
        if (declared.kind == TYPE_VOID) {
            sema_error_at(sema, global, "array element type cannot be void", global->var_decl.name);
        }
        declared = type_array(declared, global->var_decl.array_size);
    }
    if (global->var_decl.is_const && !global->var_decl.value) {
        sema_error_at(sema, global, "const global requires initializer", global->var_decl.name);
    }
    if (global->var_decl.value) {
        if (declared.kind == TYPE_ARRAY && global->var_decl.value->type != AST_ARRAY_LITERAL) {
            sema_error_at(sema, global, "fixed arrays require an array initializer literal", NULL);
        }
        if (!is_constant_initializer(global->var_decl.value)) {
            sema_error_at(sema, global, "global initializer must be constant", global->var_decl.name);
        }
        Type actual = analyze_expr(sema, global->var_decl.value);
        if (!is_assignable(declared, actual)) {
            type_error_at(sema, global, "global initializer type mismatch", declared, actual);
        }
    }
    add_top_symbol(sema, global->var_decl.name, declared, global->var_decl.is_const,
                   global->source_file, global->is_public);
}

// Checks parameters, body, and return behavior for one function.
static void analyze_function(Sema *sema, ASTNode *fn) {
    sema->current_source_file = fn->source_file;
    sema->current_return_type = type_from_name(sema, fn->function.return_type);
    push_scope(sema);

    for (size_t i = 0; i < fn->function.params->list.count; i++) {
        ASTNode *param = fn->function.params->list.items[i];
        Type type = type_from_name(sema, param->param.param_type);
        if (type.kind == TYPE_UNKNOWN) sema_error_at(sema, param, "unknown parameter type", param->param.param_type);
        if (param->param.array_size > 0) {
            if (type.kind == TYPE_VOID) sema_error_at(sema, param, "array parameter element cannot be void", NULL);
            type = type_array(type, param->param.array_size);
        }
        add_symbol(sema, param->param.name, type, 0);
    }

    int returns = analyze_block(sema, fn->function.body);
    if (sema->current_return_type.kind != TYPE_VOID && !returns) {
        sema_error_at(sema, fn, "function may exit without returning", fn->function.name);
    }
    pop_scope(sema);
    sema->current_return_type = type_unknown();
}

int sema_analyze(ASTNode *root, int *warning_count) {
    Sema sema = {0};
    collect_enums(&sema, root);
    collect_structs(&sema, root);
    analyze_structs(&sema);
    push_scope(&sema);
    add_enum_values(&sema);
    for (size_t i = 0; i < root->list.count; i++) {
        if (root->list.items[i]->type == AST_VAR_DECL) {
            analyze_global(&sema, root->list.items[i]);
        }
    }
    collect_functions(&sema, root);
    for (size_t i = 0; i < root->list.count; i++) {
        if (root->list.items[i]->type == AST_FUNCTION) {
            analyze_function(&sema, root->list.items[i]);
        }
    }
    pop_scope(&sema);
    free(sema.enums);
    free(sema.structs);
    free(sema.functions);
    if (warning_count) {
        *warning_count = sema.warnings;
    }
    return sema.errors == 0;
}
