#ifndef NATIVE_H
#define NATIVE_H

#include <stdio.h>
#include "ir.h"

// Emits a debug-only native backend artifact from a lowered Pine IR module.
// This is not machine code: it is a stack-machine-shaped text target used
// to design frame layout and control-flow lowering ahead of a real backend.
void native_emit_debug(IRModule *module, FILE *out);

#endif
