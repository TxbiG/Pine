# Pine Compiler Documentation

## Overview
- [What is Pine](#what-is-pine)
- [Supported Architectures](#supported-architectures)
- [Memory Safety](#memory-safety)
- [Installing](#installing)
- [Licensing](/LICENSE)

## Language Basics
- [Hello World](#)
- [Syntax Overview](#)
- [Data Types](#)
- [Variables and Constants](#)
- [Control Flow](#)
- [Functions](#)
- [Comments](#)

## Advanced Language Features
- [Enum, Classes and Structs](#)
- [Pattern Matching](#)
- [Memory Management](#)
- [Error Handling](#)
- [Generics](#)
- [Modules and Imports](#)
- [Traits / Interfaces](#)

## Compiler Usage
- [Basic Compilation](#)
- [Compiler Flags and Options](#)
- [Optimisation Levels](#)
- [Cross-Compilation](#)
- [Build Scripts](#)

## Standard Library
- [IO](/lib/stdio.h)
- [Math](/lib/math.h)
- [Time](/lib/time.h)
- [Standard](/lib/stdlib.h)
- [System / OS Interaction](#)

## Interoperability
- [C/C++ / FFI Bindings](#)
- [Calling Pine from Other Languages](#)
- [WebAssembly (soon)](#)

---

# What is Pine
Pine is a modern, statically-typed systems programming language designed for performance, control, and clarity.

Pine combines low-level power with modern language design principles. It is built for developers who want C-level control with a cleaner structure and optional safety features.

Goals:
- Near C/C++ performance
- Strong static typing
- Explicit low-level control
- Optional memory safety
- Familiar and readable syntax

# Supported Architectures
Tier 1 (Fully Supported):
- x86_64
- ARM64 (AArch64)

Tier 2 (Planned):
- RISC-V 64
- WASM32
- ARMv7

Planned Backends:
- Native assembler backend
- C transpiler backend
- LLVM backend (future)

# Memory Safety
Pine provides configurable memory safety modes.

Manual Mode (C-style):
- Raw pointers allowed
- Manual memory management
- Maximum control

Safe Mode (Recommended):
- Bounds-checked arrays
- Automatic zero-initialization
- Safer union behaviour
- Optional lifetime validation (future)

Example:
```
u32[10] arr;  // bounds-checked array
```

Future Safety Roadmap:
- Optional ownership model
- Borrow checking mode
- Safer pointer annotations

Pine does not force safety — developers choose the appropriate level of control for their project.

# Installing
## From source
```
git clone https://github.com/pine-lang/pine
cd pine
make
sudo make install
```
## Building program
```
pine build main.pine
```
## Run Directly
```
pine run main.pine
```
