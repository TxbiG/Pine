#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include <stdio.h>

// Emits C source for a semantically checked Pine AST.
void codegen_generate_c(ASTNode *root, FILE *out);

#endif
