[![Build](https://github.com/TxbiG/Pine/actions/workflows/build.yml/badge.svg)](https://github.com/TxbiG/Pine/actions/workflows/build.yml)
# Pine Language

Pine is a C-like systems programming language project focused on predictable performance, low-level control, and memory safety by default.

Pine currently uses a C transpiler as its production backend. The compiler parses Pine source, performs semantic and safety checks, and emits readable C. Native backend work is planned after the language and IR shape are more stable.

## Current Status

Pine is in early development, but it already supports a useful compiler slice:

- C-like functions, variables, globals, and constants
- primitive integer, float, bool, char, string, and void types
- structs, designated struct literals, and field access
- enums with implicit or explicit integer values
- `true` and `false` boolean literals
- fixed-size arrays with bounds checks and initializer literals
- fixed-array parameters and assignment through index/field/dereference lvalues
- slices with pointer-plus-length representation
- nullable values with explicit `null` checks
- pointers and explicit `unsafe` blocks
- `if`, `else`, `while`, `for`, `switch`, `case`, and `default`
- `break`, `continue`, and `return`
- arithmetic, comparison, logical, and bitwise operators
- imports, private-by-default declarations, `pub` exports, and source-only `std` modules
- warnings, parse recovery, and line/column diagnostics
- explicit `move(value)` ownership foundation
- textual IR dump using `pine ir <file>`
- architecture-aware debug native artifacts for `x86_64`, `aarch64`/`arm64`, and `riscv64`/`rv64`
- native debug frame slots, call records, and label-based control flow
- native debug storage layout for primitive types, frame slots, globals, and structs

## Design Goals

- Stay close to C syntax and performance expectations.
- Keep safe Pine code safe by default.
- Make low-level operations explicit through `unsafe`.
- Prefer predictable checks that C compilers can optimize.
- Keep the runtime small and avoid a garbage collector.
- Use a C backend first, then grow toward native code generation.

## Repository Layout

```text
.
|-- Documentation/   language design, syntax, and roadmap
|-- examples/        sample Pine programs
|-- src/             compiler source
|-- std/             source-only Pine standard library seed
|-- README_BUILD.md  current build and compiler usage notes
`-- LICENSE
```

## Building The Compiler

A C compiler and CMake are required to build Pine itself.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The direct single-command C build remains supported:

```sh
cc src/main.c src/lexer.c src/parser.c src/sema.c src/ast.c src/codegen.c src/ir.c src/native.c -o pine
```

On Windows, use `gcc`, `clang`, or another C compiler available on PATH.

## Basic Usage

Transpile Pine to C:

```sh
./pine examples/simple.pine > simple.c
```

Use the CLI commands:

```sh
./pine build examples/simple.pine
./pine run examples/simple.pine
./pine test examples/simple.pine
./pine check examples/simple.pine
./pine ir examples/simple.pine
./pine targets
./pine native examples/simple.pine --target x86_64
./pine native examples/simple.pine --target aarch64
./pine native examples/simple.pine --target riscv64
./pine native examples/native_flow.pine
./pine native examples/native_calls.pine
./pine native examples/native_layout.pine
```

`pine build` and `pine run` look for a C compiler using `CC`, then `cc`, `gcc`, and `clang`.

Native target selection currently controls the debug artifact's architecture,
triple, pointer/layout metadata, and stack alignment. It does not yet perform
instruction selection, implement an ABI, or emit object files.

## Documentation

- [Documentation index](./Documentation/README.md)
- [Design direction](./Documentation/Design.md)
- [Syntax reference](./Documentation/Syntax.md)
- [Roadmap](./Documentation/Roadmap.md)
- [Build notes](./README_BUILD.md)

## License

Pine is distributed under the [Apache-2.0 License](./LICENSE).
