#!/usr/bin/env python3
"""Parse Pine's C sources with pycparser when a native compiler is unavailable."""

from pathlib import Path
import re
import sys

from pycparser import c_parser


ROOT = Path(__file__).resolve().parents[1]
FILES = [
    "src/lexer.h",
    "src/ast.h",
    "src/parser.h",
    "src/sema.h",
    "src/codegen.h",
    "src/ir.h",
    "src/native.h",
    "src/lexer.c",
    "src/ast.c",
    "src/parser.c",
    "src/sema.c",
    "src/codegen.c",
    "src/ir.c",
    "src/native.c",
    "src/main.c",
]


def sanitize(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"^\s*#.*$", "", text, flags=re.M)
    return text


prefix = """
typedef unsigned long size_t;
typedef long int64_t;
typedef unsigned long uint64_t;
typedef int FILE;
void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
size_t strlen(const char *);
void *memcpy(void *, const void *, size_t);
int snprintf(char *, size_t, const char *, ...);
int fprintf(FILE *, const char *, ...);
int fputc(int, FILE *);
long strtol(const char *, char **, int);
"""

translation_unit = prefix + "\n".join(
    sanitize((ROOT / path).read_text(encoding="utf-8")) for path in FILES
)

try:
    c_parser.CParser().parse(translation_unit, filename="pine_combined.c")
except Exception as exc:
    print(f"C source syntax check failed: {exc}", file=sys.stderr)
    raise SystemExit(1)

print("C source syntax check passed")
