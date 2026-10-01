# Contributing to Pine

Thank you for contributing to **Pine Language**, a C-like systems programming language focused on predictable performance, low-level control, and memory safety by default.

Pine is in early development. Contributions to the compiler, language design, standard library, diagnostics, tests, documentation, and build system are especially welcome.

## Before You Start

For language-design changes or substantial compiler architecture changes, open an issue first.

Please search existing issues and pull requests before starting work, particularly for syntax, type-system, safety, ownership, IR, and standard-library changes.

## Development Requirements

- Git
- CMake
- A C compiler
- A supported native build environment

Pine currently uses a **C transpiler as its production backend**. The compiler parses Pine source, performs semantic and safety checks, and emits readable C; native backend work is planned as the language and IR mature.

## Building

```bash
git clone https://github.com/TxbiG/Pine.git
cd Pine

cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

See `README_BUILD.md` for repository-specific build details.

## Repository Structure

- `src/` — compiler implementation
- `library/` — compiler/runtime library material
- `Documentation/` — language documentation and design notes
- `examples/` — example Pine programs
- `tests/` — compiler tests
- `.github/workflows/` — CI
- `README_BUILD.md` — build and compiler notes

## Areas for Contribution

Useful contributions include:

- Lexer and parser improvements
- Semantic analysis and type checking
- Safety and ownership rules
- Diagnostics and error recovery
- AST and IR improvements
- C code generation
- Future native backend work
- Standard-library modules
- Threading, networking, and SIMD support
- Templates/generics and vectors/collections
- Cross-platform compiler support
- Tests and regression coverage
- Documentation and examples
- CMake and CI improvements

## Language Design

Treat syntax and semantic changes as design changes, not implementation details.

A proposed language feature should normally explain:

1. The problem it solves.
2. The proposed syntax and semantics.
3. Interactions with existing features.
4. Invalid/error cases.
5. Representative source examples.
6. Backwards-compatibility considerations.

Avoid adding syntax where existing Pine features can express the same operation clearly.

## Compiler Architecture

Keep the compiler stages separated where practical:

- Lexing
- Parsing
- AST construction
- Semantic analysis
- IR generation
- Code generation/transpilation
- Native/debug artefact generation

Do not bypass semantic or safety checks merely because a backend can represent an operation.

## Safety

Pine aims to make safe code safe by default while retaining explicit low-level facilities such as `unsafe`.

Changes involving pointers, ownership, slices, bounds checks, integer conversions, memory layout, or `unsafe` code should include tests for both valid and invalid programs.

## Testing

Compiler changes should include tests for:

- Valid programs
- Invalid syntax
- Invalid types
- Boundary conditions
- Diagnostics
- Regression cases
- Target-specific behaviour where relevant

When fixing a compiler bug, add a regression test whenever practical.

## Commit Messages

Conventional Commit prefixes are recommended:

```text
feat: add generic vector type
fix: reject invalid slice assignment
docs: clarify ownership rules
test: add nullable pointer diagnostics
refactor: simplify semantic analysis
build: improve Windows compiler detection
ci: expand compiler matrix
```

Keep commits small and focused.

## Pull Requests

A good pull request should include:

- A concise description of the change.
- The motivation for the change.
- Tests demonstrating the behaviour.
- Documentation updates for user-visible language features.
- Compatibility or language-design notes.
- Target/platform information where relevant.

For language changes, include representative Pine source examples.

## Documentation

Update documentation whenever you change:

- Syntax
- Types
- Ownership/safety rules
- Standard-library APIs
- Compiler commands
- Diagnostics
- Target support

Documentation should describe implemented behaviour rather than planned behaviour unless it is explicitly a design document.

## Reporting Bugs

Include:

- Pine commit/version
- Operating system
- Compiler/toolchain used to build Pine
- Command executed
- Minimal Pine source reproducing the issue
- Expected result
- Actual result
- Compiler output

## Security

Do not include credentials, private source, or other sensitive information in issues or pull requests. Use a private security channel when a vulnerability should not be disclosed publicly.

## Licence

Pine is distributed under the **Apache License 2.0**. Contributions should be compatible with the repository's licence and applicable third-party licence requirements.

Thank you for helping develop Pine.
