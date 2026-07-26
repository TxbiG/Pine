#ifndef NATIVE_H
#define NATIVE_H

#include <stdio.h>
#include "ir.h"

typedef enum {
    NATIVE_TARGET_X86_64,
    NATIVE_TARGET_AARCH64,
    NATIVE_TARGET_RISCV64
} NativeTarget;

// Parses a canonical target name or a supported alias.
int native_target_parse(const char *name, NativeTarget *target);
const char *native_target_name(NativeTarget target);
void native_list_targets(FILE *out);

// Emits a debug-only, architecture-aware native backend artifact from Pine IR.
// This is not machine code and has no ABI or object-file contract.
void native_emit_debug(IRModule *module, NativeTarget target, FILE *out);

#endif
