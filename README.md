# Pine Language

Pine is a C-like systems programming language project focused on predictable performance, low-level control, and memory safety by default.

Pine currently uses a C transpiler as its production backend. The compiler parses Pine source, performs semantic and safety checks, and emits readable C. Native backend work is planned after the language and IR shape are more stable.

## Current Status

Pine is in early development, but it already supports a useful compiler slice:

- C-like functions, variables, globals, and constants
- primitive integer, float, bool, char, string, and void types
- structs and field access
- fixed-size arrays with bounds checks
- slices with pointer-plus-length representation
- nullable values with explicit `null` checks
- pointers and explicit `unsafe` blocks
- `if`, `else`, `while`, `for`, `switch`, `case`, and `default`
- `break`, `continue`, and `return`
- arithmetic, comparison, logical, and bitwise operators
- imports and source-only `std` modules
- warnings, parse recovery, and line/column diagnostics
- explicit `move(value)` ownership foundation
- textual IR dump using `pine ir <file>`
- debug-only native backend artifact using `pine native <file>`
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

A C compiler is currently required to build Pine itself.

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
./pine ir examples/simple.pine
./pine native examples/simple.pine
./pine native examples/native_flow.pine
./pine native examples/native_calls.pine
./pine native examples/native_layout.pine
```

`pine build` and `pine run` look for a C compiler using `CC`, then `cc`, `gcc`, and `clang`.

## Documentation

- [Documentation index](./Documentation/README.md)
- [Design direction](./Documentation/Design.md)
- [Syntax reference](./Documentation/Syntax.md)
- [Roadmap](./Documentation/Roadmap.md)
- [Build notes](./README_BUILD.md)

## License

Pine is distributed under the [Apache-2.0 License](./LICENSE).
