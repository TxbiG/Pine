# Pine Design Direction

## Core Identity

Pine is a low-level, C-like systems programming language with memory safety enabled by default.

The goal is to keep the directness and predictable performance of C while removing common memory-corruption risks from normal Pine code.

Pine should feel familiar to C programmers:

- explicit types
- type-first variable declarations
- type-first function declarations
- predictable layout
- value types by default
- functions and structs as core building blocks
- manual control available through explicit `unsafe`
- minimal hidden runtime behavior

## Current Compilation Strategy

The production backend is currently a C transpiler.

```text
Pine source
  -> lexer
  -> parser
  -> AST
  -> semantic and safety checks
  -> C output
  -> C compiler
```

The compiler also has a textual IR dump:

```text
Pine source
  -> checked AST
  -> pine ir dump
```

The IR is currently for debugging and compiler development. The C backend remains the reliable build path until the native backend matures.

The first native backend path is also debug-only. It emits a stack-machine-shaped text artifact with no ABI, linker, or object-file contract yet.

## Safety Model

Pine is safe by default.

Safe Pine code should prevent:

- out-of-bounds array and slice access
- invalid null use
- accidental raw pointer dereference
- use-after-move for future owned resources
- duplicate top-level definitions
- simple type mismatches before code generation

Pine still allows low-level programming through explicit unsafe code.

```pine
i32 value = 10;
i32* ptr = &value;

unsafe {
    value = *ptr;
}
```

Unsafe is required for raw pointer dereference. Later unsafe operations will include pointer arithmetic, unchecked casts, inline assembly, unsafe FFI calls, and allocator internals.

## Bounds Safety

Fixed arrays carry length in the semantic type system:

```pine
i32 values[4];
return values[i];
```

Generated C uses a small bounds-check helper. Constant indexes outside fixed-array length are rejected at compile time.

Slices use pointer-plus-length representation:

```pine
i32 first([]i32 values) {
    return values[0];
}
```

Slice indexing emits a runtime bounds check using the slice length.

## Null Safety

Nullable types are explicit:

```pine
i32*? maybe_value = null;
```

Nullable values must be checked before ordinary use:

```pine
if (maybe_value != null) {
    unsafe {
        return *maybe_value;
    }
}
```

This is an early implementation. Later work should add richer flow-sensitive tracking, unwrap helpers, and more precise diagnostics.

## Ownership Direction

Pine has started ownership foundations with explicit moves:

```pine
Token next = move(token);
```

The compiler treats primitive numeric types, raw pointers, nullable values, and slices as copyable for now. Structs and arrays are non-copy for move diagnostics.

The long-term goal is to support owned heap pointers, deterministic cleanup, arenas, and move rules without needing a garbage collector.

## Modules And Standard Library

Imports use dotted paths:

```pine
import std.math;
```

The compiler loads imported top-level declarations before semantic analysis. The current `std/` folder is source-only and intentionally small.

Current seed modules:

- `std.math`
- `std.array`

## Diagnostics

The compiler currently reports:

- parse errors with unexpected token text
- line and column for common semantic errors
- multiple semantic errors before exiting
- warnings separate from errors
- unreachable statement warnings after `return`

Future diagnostics should include source snippets, underlines, suggestions, and better type mismatch explanations.

## Native Backend Direction

The native backend should come after the C backend, semantic analysis, and IR are stable enough to build on.

Recommended next native-backend approach:

- keep C backend as the production path
- use IR dump mode to understand lowering
- create a debug-only native artifact first
- start with a tiny target and documented limitations
- avoid trying to replace the C backend too early

Current debug native target:

```text
pine native <file> --target x86_64|aarch64|riscv64
  -> pine_native_debug 0
  -> target and target_triple metadata
  -> architecture-specific pointer and stack layout
  -> instruction_set stack-vm-text
  -> instruction_selection unsupported
  -> abi unsupported
  -> object_emission unsupported
  -> expression_lowering operand-stack
  -> control_flow labels-and-jumps
  -> calls frame-slots
  -> layout debug-primitive-sizes
```

The debug native artifact now models each function with a simple frame. Parameters and locals are assigned frame slots, identifier reads use `LOAD_SLOT`, local writes use `STORE_SLOT`, global access remains explicit, and calls record argument count plus a return value.

The native debug artifact also assigns primitive debug sizes and alignments, then uses those values to model stack slot offsets, global storage size, struct field layout, and target stack alignment. Target selection accepts `x86_64` (`x64`/`amd64`), `aarch64` (`arm64`), and `riscv64` (`rv64`). These targets currently share a 64-bit little-endian data model. The metadata is an input to future instruction selection and ABI work, not a claim that the current textual artifact is executable machine code.

## Pine 0.1 Shape

Pine 0.1 should remain small and practical:

- C-like functions and variables
- primitive types
- structs
- fixed arrays and slices
- nullable values
- explicit unsafe blocks
- imports
- C backend
- textual IR dump

Features still planned later:

- native backend
- richer ownership and cleanup
- enum declarations
- unions
- standard library growth
- formatter and language tooling
- generics
- method syntax or optional OOP features
