# Pine Documentation

This folder documents the current Pine language and compiler direction.

## Recommended Reading Order

1. [Design.md](./Design.md) explains Pine's identity and safety model.
2. [Syntax.md](./Syntax.md) describes the current supported syntax.
3. [Roadmap.md](./Roadmap.md) tracks completed work and next steps.
4. [../README_BUILD.md](../README_BUILD.md) explains how to build and run the current compiler.

## What Pine Is Today

Pine is currently a C-like systems language implemented as a C transpiler. The compiler can parse a multi-file Pine program, run semantic checks, and emit readable C. It also has a textual IR dump mode for compiler development.

Current compiler pipeline:

```text
Pine source
  -> lexer
  -> parser
  -> AST
  -> semantic and safety checks
  -> optional textual IR dump
  -> C backend
```

The C backend remains the production path while the IR and future native backend are being designed.

## Current Safety Features

Safe-by-default behavior already started:

- fixed-array indexing emits bounds checks
- constant out-of-bounds indexes are rejected
- slice indexing emits bounds checks
- raw pointer dereference requires `unsafe`
- nullable values must be checked before normal use
- unreachable statements after `return` are warnings
- explicit `move(value)` starts ownership tracking

## Current Standard Library Seed

The `std/` folder contains source-only Pine modules:

- `std.math`
- `std.array`

These modules are intentionally tiny. They are used to exercise imports and give the language a foundation for later standard library growth.

## Current CLI

```sh
pine <file>            # transpile to C on stdout
pine transpile <file>  # explicit transpile mode
pine build <file>      # emit C and compile it with a C compiler
pine run <file>        # build and run
pine test <file>       # build and run as a test program
pine ir <file>         # dump textual Pine IR
pine targets           # list supported native debug targets
pine native <file> [--target x86_64|aarch64|riscv64]
                       # emit architecture-aware debug artifact
```

## Planned Work

The next large area is instruction selection and ABI design for the native backend. The current debug artifact already carries x86-64, ARM64, or RV64 target/layout metadata. The C backend should remain useful and readable while executable native code develops behind debug-focused commands.
