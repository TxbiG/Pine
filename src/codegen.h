#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include <stdio.h>

// Output target type
typedef enum {
    TARGET_X86,
    TARGET_BYTECODE
} CodegenTarget;

// ===== API =====
void codegen_generate(ASTNode *root, FILE *out, CodegenTarget target);

#endif
