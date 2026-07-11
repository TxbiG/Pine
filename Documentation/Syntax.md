# Pine Syntax

This document describes the syntax currently supported by the Pine compiler.

## Comments

```pine
// line comment

/* block comment */
```

## Primitive Types

```pine
u8   u16   u32   u64
i8   i16   i32   i64
bool
char
string
float
double
void
```

`u128`, `u256`, `i128`, and `i256` are tokenized but not fully implemented in the semantic type system yet.

## Variables And Constants

```pine
i32 count = 10;
bool ready = true;
const i32 limit = 4;
```

Global variables and constants are supported. `const` globals require initializers.

## Functions

```pine
i32 add(i32 a, i32 b) {
    return a + b;
}
```

Function declarations use C-like type-first syntax.

## Control Flow

```pine
if (count > 0) {
    count = count - 1;
} else {
    count = 0;
}

while (count > 0) {
    count = count - 1;
}

for (i32 i = 0; i < 4; i = i + 1) {
    continue;
}

switch (count) {
    case 0:
        break;
    default:
        break;
}
```

Supported statements:

- `if` / `else`
- `while`
- C-style `for`
- `switch`, `case`, `default`
- `break`
- `continue`
- `return`

## Operators

Arithmetic:

```pine
a + b
a - b
a * b
a / b
a % b
```

Comparison:

```pine
a == b
a != b
a < b
a <= b
a > b
a >= b
```

Logical:

```pine
!ok
a && b
a || b
```

Bitwise:

```pine
a & b
a | b
a ^ b
~a
a << b
a >> b
```

## Structs

```pine
struct Vec2 {
    i32 x;
    i32 y;
}

i32 sum(Vec2 value) {
    return value.x + value.y;
}
```

Struct declarations, field declarations, and field access are supported.

## Fixed-Size Arrays

```pine
i32 values[4];
i32 first = values[0];
```

Fixed-size arrays carry their length through semantic analysis. Indexing emits a runtime bounds check, and constant indexes outside the array length are rejected at compile time.

Array initializer literals are not implemented yet.

## Slices

```pine
i32 first([]i32 values) {
    return values[0];
}
```

Slices use `[]T` syntax. The C backend represents primitive slices as pointer-plus-length structs such as:

```c
typedef struct pine_slice_i32 {
    int32_t *data;
    size_t length;
} pine_slice_i32;
```

Slice indexing emits a runtime bounds check.

## Pointers And Unsafe

```pine
i32 value = 10;
i32* ptr = &value;

unsafe {
    value = *ptr;
}
```

Pointer types use `T*`. Address-of uses `&value`. Raw pointer dereference uses `*ptr` and requires an `unsafe` block.

## Nullable Values

```pine
i32 read_or_zero(i32*? maybe_value) {
    if (maybe_value != null) {
        unsafe {
            return *maybe_value;
        }
    }

    return 0;
}
```

Nullable types use a trailing `?`. The `null` literal can be assigned to nullable values. A nullable value must be checked before normal use.

The first implemented narrowing form is:

```pine
if (value != null) {
    // value is treated as non-null here
}
```

## Ownership Foundation

```pine
struct Token {
    i32 value;
}

Token next = move(token);
```

`move(value)` is a compiler-recognized operation. Non-copy values moved this way are marked unavailable, and later use is diagnosed as use-after-move.

Current copy defaults are conservative:

- primitive numeric values are copyable
- raw pointers and slices are copyable
- structs and arrays are treated as non-copy for move diagnostics

## Imports

```pine
import std.math;
import std.array;
```

Imports use dotted module paths. The compiler first searches relative to the importing file, then relative to the current working directory.

Imported top-level declarations are loaded before semantic analysis. Duplicate imported files are skipped, and simple circular imports are rejected.

## Standard Library Seed

Current source-only modules:

```pine
import std.math;
import std.array;
```

Examples:

```pine
i32 value = std_square_i32(6);
```

## IR Dump

```sh
pine ir examples/simple.pine
```

The IR dump is a compiler-development format. It is not yet the production backend.

## Native Debug Artifact

```sh
pine native examples/simple.pine
```

The native backend currently emits a debug-only stack-machine-shaped text artifact. It is not executable machine code yet and has no ABI or object-file format.

The debug artifact now includes generated labels and jumps for core control flow, simple function frame slots for parameters and locals, and debug-only storage layout metadata. `examples/native_flow.pine` covers control flow, `examples/native_calls.pine` covers multiple functions and calls, and `examples/native_layout.pine` covers frame and struct layout output.
