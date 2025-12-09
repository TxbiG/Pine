#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "parser.h"
#include "codegen.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: mylang <file>\n");
        return 1;
    }

    // Read source file
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source = malloc(len + 1);
    fread(source, 1, len, f);
    source[len] = '\0';
    fclose(f);

    // Run compiler pipeline
    Parser parser;
    parser_init(&parser, source);

    ASTNode *program = parse_program(&parser);

    codegen_generate(program, stdout, TARGET_X86);

    ast_free(program);
    free(source);
    return 0;
}
