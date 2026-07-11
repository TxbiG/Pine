#ifndef SEMA_H
#define SEMA_H

#include "ast.h"

// Runs semantic analysis and type checking. Returns nonzero when no errors occur.
int sema_analyze(ASTNode *root, int *warning_count);

#endif
