# Pine Language Roadmap

This roadmap describes the work needed to grow Pine from a small C transpiler into a full independent programming language.

Pine's direction is:

- C-like syntax
- memory safe by default
- explicit unsafe code for low-level control
- C transpiler first
- native backend later
- simple, predictable performance

## Phase 1: Core C-Like Language

Goal: make Pine able to express small real programs.

Already started:

- C-like function declarations
- C-like variable declarations
- integer literals
- arithmetic expressions
- function parameters
- function calls
- `if` and `else`
- assignment statements
- expression statements
- `while`
- `break` and `continue`
- unary operators: `-x`, `!ok`
- additional operators: `%`, `&&`, `||`
- `return`
- C code generation
- baseline semantic analysis
- baseline primitive type checker
- struct declarations
- field declarations
- struct names in the type system
- field access parsing
- field access type checking
- C backend emission for structs
- fixed-size array declarations
- array indexing expressions
- array index type checking
- array element type checking
- C backend emission for fixed arrays
- fixed array length tracking in semantic types
- generated bounds checks for fixed array indexing
- compile-time rejection for constant indexes outside fixed array length
- pointer type parsing
- address expressions
- dereference expressions
- unsafe block parsing
- unsafe requirement for raw pointer dereference
- C backend emission for pointers and unsafe blocks
- bitwise operators: `&`, `|`, `^`, `~`, `<<`, `>>`
- integer-only semantic checks for bitwise operators
- C backend emission for bitwise operators
- `for` loop parsing and C backend emission
- `switch`, `case`, and `default` parsing
- switch semantic checks for integer expressions and duplicate labels
- global variable declarations
- global constant declarations
- semantic checks for duplicate globals
- constant initializer checks for globals
- C backend emission for globals and constants
- name lookup across global and local scopes
- char literals
- string literals
- block comments
- semantic types for char and string literal expressions
- C backend emission for char and string literals
- AST source spans for diagnostics
- parse errors that report the unexpected token
- semantic errors with line and column for common checks
- type mismatch diagnostics with source locations
- simple parse error recovery
- semantic warning diagnostics separate from errors
- warning counts in compiler output
- unreachable code warnings after return
- `import` token and parser support
- dotted module path syntax such as `import std.io;`
- per-file module loading in the CLI
- imported top-level declarations before semantic analysis
- duplicate imported files skipped during loading
- simple circular import detection
- source-only `std` folder
- `std.math` helper module
- `std.array` helper module
- import-based standard library examples
- `pine build <file>`
- `pine run <file>`
- `pine test <file>`
- plain `pine <file>` compatibility mode for C output
- C compiler discovery using `CC`, `cc`, `gcc`, or `clang`
- slice type parsing for forms such as `[]i32`
- function parameters that accept slices
- slice indexing with bounds checks
- pointer-plus-length C representation for primitive slices
- nullable type syntax using `?`
- `null` literal
- semantic checks that reject unchecked nullable use
- checked nullable narrowing in `if (value != null)`
- explicit `move(value)` operation
- use-after-move diagnostics for non-copy values
- primitive numeric and pointer-like values remain copyable
- compact textual Pine IR format
- AST-to-IR dump for functions, statements, and expressions
- `pine ir <file>` debugging mode
- first debug-only native backend artifact
- stack-VM-shaped textual native target
- `pine native <file>` command
- labels and jumps in the native debug artifact
- native debug lowering for `if`, `while`, `for`, and `switch`
- native backend control-flow example
- native debug stack frame modeling
- native debug frame slots for parameters and locals
- native debug call and return-value modeling
- native debug primitive size and alignment modeling
- native debug stack slot offsets
- native debug struct field layout
- flat, opcode-based Pine IR shared by `pine ir` and `pine native`
- native debug backend lowered directly from Pine IR instead of a second AST walk

Next core features:

- source snippets in diagnostics
- public/private visibility for imported declarations

## Phase 2: Semantic Analysis

Goal: make Pine understand meaning, not only syntax.

Implemented baseline:

- symbol tables
- lexical scopes
- function signature registry
- variable declaration tracking
- duplicate name errors
- unknown identifier errors
- function call argument checking
- return statement checking
- basic return type checking
- primitive internal type representation
- operator type checking

Next semantic features:

- assignment checking for more complex lvalues
- richer type mismatch explanations

This is the point where Pine starts becoming a real language instead of just syntax that emits C.

## Phase 3: Type System

Goal: make all Pine programs statically checked before code generation.

Primitive types:

- `u8`, `u16`, `u32`, `u64`
- `i8`, `i16`, `i32`, `i64`
- `bool`
- `char`
- `float`
- `double`
- `void`

Type rules:

- signed and unsigned checking
- integer promotion rules
- explicit casts
- implicit conversion policy
- type inference later if desired
- function parameter type checking
- function return type checking
- comparison result type is `bool`
- assignment type checking
- initializer type checking

Future types:

- enums
- unions
- slices
- pointers
- nullable values
- function pointers
- opaque types
- generics

## Phase 4: Data Types

Goal: give Pine the data modelling power needed for systems programming.

Core data types:

```pine
struct Vec2 {
    f32 x;
    f32 y;
}

enum Color {
    Red,
    Green,
    Blue,
}

i32 values[8];
```

Implemented baseline:

- struct declarations
- struct field declarations
- struct field access
- fixed-size array declarations
- array indexing
- array length tracking for fixed arrays

Needed features:

- struct initialization
- enum declarations
- enum values
- slices: `[]i32`
- strings as safe slices or standard library types
- unions
- safe tagged unions later

## Phase 5: Memory Safety

Goal: make safe Pine code avoid common C memory bugs without destroying performance.

Safe-by-default rules:

- variables must be initialized before read
- array access is bounds checked
- slices carry length metadata
- nullable values must be checked before use
- raw pointer dereference requires `unsafe`
- pointer arithmetic requires `unsafe`
- unchecked casts require `unsafe`
- inline assembly requires `unsafe`

Memory models to add:

- stack values by default
- owned heap pointers
- borrowed references or slices
- arena allocation
- explicit allocator APIs
- move rules
- copy rules
- destructor or cleanup rules
- `defer` for predictable cleanup

Implemented baseline:

- fixed array access emits runtime bounds checks
- constant fixed array indexes outside the array length are rejected at compile time
- raw pointer dereference requires `unsafe`

Example direction:

```pine
i32 main() {
    i32 values[4] = [1, 2, 3, 4];
    return values[0];
}

unsafe {
    u8* raw = device_memory();
    raw[0] = 255;
}
```

## Phase 6: Unsafe Boundary

Goal: keep low-level power available without making all code dangerous.

Unsafe should be required for:

- raw pointer dereference
- pointer arithmetic
- calling unsafe foreign functions
- unchecked casts
- direct hardware memory access
- inline assembly
- manual allocator internals

The compiler should track whether code is inside an `unsafe` block and reject unsafe operations outside it.

Implemented baseline:

- pointer declarations
- address expressions
- raw pointer dereference expressions
- unsafe blocks
- rejection for raw pointer dereference outside `unsafe`
- C backend emission for pointers and unsafe blocks

## Phase 7: Modules And Packages

Goal: make Pine projects larger than one file.

Needed features:

- public/private visibility
- package layout
- module-level symbol tables
- standard library imports
- project config file

Implemented baseline:

- `import`
- dotted module paths
- per-file module loading
- duplicate imported files skipped
- simple circular import detection

Possible syntax:

```pine
import std.io;
import math.vec;
```

## Phase 8: Standard Library

Goal: make Pine useful without forcing every program to call C directly.

Initial modules:

- `std.io`
- `std.mem`
- `std.string`
- `std.array`
- `std.slice`
- `std.math`
- `std.time`
- `std.fs`
- `std.os`
- `std.thread`
- `std.test`

Important library features:

- printing
- file IO
- allocation
- arenas
- strings
- slices
- math
- time
- OS interaction
- safe wrappers around C APIs

## Phase 9: Compiler Architecture

Goal: make the compiler maintainable as Pine grows.

Long-term pipeline:

```text
source
  -> lexer
  -> parser
  -> AST
  -> semantic analysis
  -> type checking
  -> safety checking
  -> lowered IR
  -> backend
  -> executable or generated C
```

Compiler components:

- lexer
- parser
- AST
- diagnostics
- symbol table
- type checker
- safety checker
- C backend
- test runner
- eventually Pine IR
- eventually native backend

## Phase 10: C Backend

Goal: keep Pine easy to build and debug while the language is still changing.

The C backend should:

- emit readable C
- map Pine integer types to `<stdint.h>`
- map Pine `bool` to `<stdbool.h>`
- preserve simple control flow
- generate bounds checks for safe arrays and slices
- emit helper functions for runtime checks
- support debug-friendly output

The C backend should stay useful even after a native backend exists.

## Phase 11: Independent Native Backend

Goal: eventually make Pine independent from C compilers.

This should come after the language is stable.

Needed systems:

- Pine IR
- lowering from AST to IR
- stack layout
- register allocation
- instruction selection
- calling convention support
- object file generation
- linker integration
- platform ABI support
- startup code
- debug information later

Initial native targets:

- x86_64
- ARM64

Later targets:

- RISC-V 64
- WASM32

## Phase 12: Build Tool

Goal: make Pine feel like a complete toolchain.

Commands:

```text
pine build
pine run
pine test
pine fmt
pine doc
```

Needed features:

- project config
- source discovery
- debug and release modes
- incremental builds
- dependency cache
- cross-compilation flags
- C compiler discovery while using the C backend
- test discovery
- build scripts later

## Phase 13: Tooling

Goal: make Pine pleasant to use.

Needed tools:

- formatter
- linter
- language server
- syntax highlighting
- documentation generator
- package manager
- test runner
- benchmark runner
- debugger support
- playground or REPL later

## Phase 14: Diagnostics

Goal: make compiler errors clear and helpful.

Diagnostics should include:

- file path
- line and column
- source snippets
- underlined spans
- clear expected/found messages
- type mismatch explanations
- unknown name suggestions
- recovery after parse errors
- warnings separate from errors

Example direction:

```text
main.pine:4:12: error: unknown variable 'anser'
    return anser;
           ^^^^^
help: did you mean 'answer'?
```

## Phase 15: Runtime

Goal: define what support code Pine needs at runtime.

Runtime pieces:

- program entry
- panic handler
- assert handler
- bounds check failure handler
- allocation hooks
- optional stack traces
- startup and shutdown hooks
- platform abstraction layer

The runtime should stay small. Pine should not require a garbage collector.

## Recommended Implementation Order

1. Array initializer literals — close the gap between documented examples and what the parser actually accepts.
2. Assignment to complex lvalues (`arr[i] = x;`, `p.field = x;`) — currently a parse error, not just a semantic gap.
3. Enum declarations — `enum` is already a reserved keyword with no parser support behind it.
4. Struct literal initialization.
5. Decide the fate of the `class` keyword.
6. Source snippets in diagnostics.

## Current Best Next Step

The native backend now consumes the same flat Pine IR that `pine ir` dumps (`ir_lower_program` in `ir.c`), instead of independently walking the AST. Control-flow linearization (labels, jumps, break/continue targets, switch dispatch) now happens once, in the IR lowering pass; `native.c` only adds frame/layout concerns on top. `pine ir` and `pine native` on the same source now share label IDs, confirming they come from one lowering.

The next best compiler feature is **array initializer literals** (`i32 values[4] = [1, 2, 3, 4];`).

While exercising the new IR pipeline against real programs, a second, related gap surfaced: **assignment to anything other than a bare variable name is a parse error**, not just an unchecked semantic case. `arr[i] = 1;` and `point.x = 1;` both fail to parse today. `AST_ASSIGN_STMT` only carries a name, not an arbitrary lvalue expression. This should be scoped alongside array initializers, since both touch the same assignment-statement grammar.

Start with:

- parser support for bracketed initializer lists in variable declarations
- element-count validation against the declared fixed array length (reuse the existing fixed-length tracking used for bounds checks)
- per-element type checking against the array's element type in `sema.c`
- extend `AST_ASSIGN_STMT` (or add a new lvalue-assignment node) to accept index and field targets, not just identifiers
- C backend and IR/native lowering for both, plus at least one example comparing `pine ir` and `pine native` output for an array-initialized, index-assigned program

## Following That

**Enum declarations.** `enum` and `union` are already reserved in the lexer (`lexer.h`) but neither token is consumed anywhere in `parser.c`. Wiring up `enum` is low-risk (no new keyword needed) and closes one of the two data types Phase 4 lists as "needed" but missing. `union` can follow the same shape once enum's semantic-analysis pattern (a name plus a closed set of tagged constant values) is in place.

**A decision on `class`.** `class` is reserved in the lexer. Pine's stated direction is C-like structs with no inheritance or vtables, so a `class` keyword implies a design question that hasn't been answered yet: is this a future syntactic sugar over structs-plus-functions, or should the keyword be dropped to keep the language's identity clear? This should be settled with a short design note before enum/struct work makes `class` an even more visible gap.

**Then** source snippets in diagnostics (already flagged as a Phase 1 "next core feature," and cheap relative to its payoff).
