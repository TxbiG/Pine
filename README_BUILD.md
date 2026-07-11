# Building Pine 0.1

This starter compiler reads a small Pine program and emits C to stdout.

Supported current slice:

- C-like functions
- global variable declarations
- global constant declarations
- char literals
- string literals
- block comments
- module imports with dotted paths
- struct declarations
- struct field declarations
- struct field access
- fixed-size array declarations
- array indexing expressions
- generated bounds checks for fixed-size array indexing
- pointer declarations
- address expressions
- pointer dereference expressions inside `unsafe`
- `unsafe` blocks
- bitwise operators: `&`, `|`, `^`, `~`, `<<`, `>>`
- function parameters
- function calls
- C-like variable declarations
- assignment statements
- expression statements
- integer literals
- identifiers
- `+`, `-`, `*`, `/`
- `%`
- `==`, `!=`, `<`, `<=`, `>`, `>=`
- `!`, `&&`, `||`
- `if` and `else`
- `while`
- `for`
- `switch`, `case`, and `default`
- `break` and `continue`
- `return`
- semantic analysis for names, scopes, calls, and returns
- AST source locations for diagnostics
- parse errors that report the unexpected token
- semantic and type errors with line and column where available
- parser recovery for simple syntax errors
- semantic warnings reported separately from errors
- warning counts in compiler output
- internal type checking for primitive types
- struct name and field access checking
- array index and element type checking
- compile-time rejection for constant indexes outside fixed array length
- rejection for raw pointer dereference outside `unsafe`
- integer-only checks for bitwise operators
- integer-only checks for switch expressions
- duplicate switch case and default checking
- global name lookup from function bodies
- duplicate global name checking
- constant initializer checking for globals
- semantic types for char and string literal expressions
- C code generation
- per-file import loading before semantic analysis
- duplicate imported files are skipped
- circular imports are rejected
- source-only standard library seed under `std`
- `std.math` helpers for common `i32` operations
- `std.array` helpers for fixed `i32[4]` arrays
- CLI commands: `pine build`, `pine run`, and `pine test`
- plain `pine <file>` compatibility mode for C output
- C compiler discovery using `CC`, `cc`, `gcc`, or `clang`
- slice type parsing for forms such as `[]i32`
- slice parameters with pointer-plus-length C representation
- slice indexing with generated bounds checks
- nullable type syntax using `?`
- `null` literal
- nullable values must be checked before normal use
- explicit `move(value)` operation
- use-after-move diagnostics for non-copy values
- textual Pine IR dump with `pine ir <file>`
- debug-only native backend artifact with `pine native <file>`
- native debug frame slots for function parameters and locals
- native debug call and return-value records
- native debug storage layout for primitive types, frame slots, globals, and structs

Current backend status:

- The C backend is the production backend.
- `pine ir <file>` emits a textual compiler-development IR.
- `pine native <file>` emits a debug-only stack-VM-shaped artifact.
- Native backend work has started, but it does not emit real machine code yet.
- native debug output now uses generated labels and jumps for core control flow.
- native debug output models parameters and locals as explicit frame slots.
- native debug output includes debug-only sizes, alignments, and offsets.

Example Pine:

```pine
i32 add(i32 a, i32 b) {
    return a + b;
}

i32 main() {
    return add(40, 2);
}
```

Example build command:

```sh
cc src/main.c src/lexer.c src/parser.c src/sema.c src/ast.c src/codegen.c src/ir.c src/native.c -o pine
./pine examples/simple.pine > simple.c
./pine build examples/simple.pine
./pine run examples/simple.pine
./pine ir examples/simple.pine
./pine native examples/simple.pine
./pine native examples/native_flow.pine
./pine native examples/native_calls.pine
./pine native examples/native_layout.pine
```

The generated C for `examples/simple.pine` should match `examples/simple.expected.c`.

CLI commands:

```sh
./pine <file>             # transpile to C on stdout
./pine transpile <file>   # explicit transpile mode
./pine build <file>       # emit C and compile it
./pine run <file>         # build and run
./pine test <file>        # build and run as a test
./pine ir <file>          # dump textual Pine IR
./pine native <file>      # emit debug-only native backend artifact
```

Import examples:

```sh
./pine examples/import_main.pine > import_main.c
./pine examples/import_array.pine > import_array.c
./pine build examples/import_main.pine
./pine examples/slice_param.pine > slice_param.c
./pine examples/nullable_pointer.pine > nullable_pointer.c
./pine examples/move_struct.pine > move_struct.c
```

Semantic analysis currently catches:

- duplicate function names
- duplicate global names
- non-constant global initializers
- missing const global initializers
- simple char and string literal type mismatches
- duplicate variable and parameter names in the same scope
- unknown identifiers
- unknown function calls
- wrong function call argument counts
- simple argument type mismatches
- simple initializer type mismatches
- simple assignment type mismatches
- arithmetic, comparison, and logical operator type checks
- simple return type mismatches
- non-bool `if` conditions
- non-bool `while` conditions
- non-bool `for` conditions
- non-integer `switch` expressions
- duplicate `case` labels
- duplicate `default` labels
- `break` and `continue` outside loops
- duplicate struct names
- duplicate struct fields
- unknown struct fields
- array indexing on non-array values
- non-integer array indexes
- constant array indexes outside the fixed array length
- raw pointer dereference outside `unsafe`
- non-integer bitwise operands
- assignment to const names
- missing returns in non-void functions
- unreachable statements after return, reported as warnings
