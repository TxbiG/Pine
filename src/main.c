#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "codegen.h"
#include "ir.h"
#include "native.h"

#ifdef _WIN32
#define PINE_EXE_SUFFIX ".exe"
#define PINE_NULL_DEVICE "NUL"
#else
#define PINE_EXE_SUFFIX ""
#define PINE_NULL_DEVICE "/dev/null"
#endif

typedef struct {
    char **items;
    size_t count;
} PathList;

static char *copy_text(const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    memcpy(copy, text, length + 1);
    return copy;
}

static int path_list_contains(PathList *list, const char *path) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i], path) == 0) {
            return 1;
        }
    }
    return 0;
}

static void path_list_push(PathList *list, const char *path) {
    size_t next = list->count + 1;
    list->items = realloc(list->items, next * sizeof(char *));
    list->items[list->count] = copy_text(path);
    list->count = next;
}

static void path_list_pop(PathList *list) {
    if (list->count == 0) {
        return;
    }
    free(list->items[list->count - 1]);
    list->count--;
}

static void path_list_free(PathList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
}

static char *read_source_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source = malloc((size_t)len + 1);
    fread(source, 1, (size_t)len, f);
    source[len] = '\0';
    fclose(f);
    return source;
}

static char *directory_name(const char *path) {
    const char *last_slash = strrchr(path, '/');
    const char *last_backslash = strrchr(path, '\\');
    const char *last = last_slash;
    if (!last || (last_backslash && last_backslash > last)) {
        last = last_backslash;
    }
    if (!last) {
        return copy_text(".");
    }

    size_t length = (size_t)(last - path);
    char *dir = malloc(length + 1);
    memcpy(dir, path, length);
    dir[length] = '\0';
    return dir;
}

static char *resolve_import_path(const char *base_dir, const char *module_path) {
    size_t base_length = strlen(base_dir);
    size_t module_length = strlen(module_path);
    char *path = malloc(base_length + 1 + module_length + 5 + 1);
    memcpy(path, base_dir, base_length);
    path[base_length] = '/';
    for (size_t i = 0; i < module_length; i++) {
        path[base_length + 1 + i] = module_path[i] == '.' ? '\\' : module_path[i];
    }
    memcpy(path + base_length + 1 + module_length, ".pine", 6);
    return path;

    /*
    // If the module_path contains a slash or backslash, treat it as a path already.
    if (strchr(module_path, '/') || strchr(module_path, '\\')) {
        size_t base_length = strlen(base_dir);
        size_t module_length = strlen(module_path);
        char *path = malloc(base_length + 1 + module_length + 5 + 1);
        memcpy(path, base_dir, base_length);
        path[base_length] = '/';
        memcpy(path + base_length + 1, module_path, module_length);
        memcpy(path + base_length + 1 + module_length, ".pine", 6);
        return path;
    }

    // Otherwise, existing behavior: dots -> path separators
    size_t base_length = strlen(base_dir);
    size_t module_length = strlen(module_path);
    char *path = malloc(base_length + 1 + module_length + 5 + 1);
    memcpy(path, base_dir, base_length);
    path[base_length] = '/';
    for (size_t i = 0; i < module_length; i++) {
        path[base_length + 1 + i] = module_path[i] == '.' ? '/' : module_path[i];
    }*/
}



// Resolves imports first relative to the importing file, then relative to cwd.
static char *resolve_existing_import_path(const char *base_dir, const char *module_path) {
    char *path = resolve_import_path(base_dir, module_path);
    FILE *file = fopen(path, "rb");
    if (file) {
        fclose(file);
        return path;
    }

    free(path);
    path = resolve_import_path(".", module_path);
    file = fopen(path, "rb");
    if (file) {
        fclose(file);
    }
    return path;
}

static int load_module(const char *path, ASTNode *combined, PathList *loaded, PathList *loading);

static int load_imports(ASTNode *root, const char *base_dir, ASTNode *combined, PathList *loaded, PathList *loading) {
    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *item = root->list.items[i];
        if (item->type != AST_IMPORT_DECL) {
            continue;
        }

        char *import_path = resolve_existing_import_path(base_dir, item->import_decl.path);
        int ok = load_module(import_path, combined, loaded, loading);
        free(import_path);
        if (!ok) {
            return 0;
        }
    }
    return 1;
}

static void move_declarations(ASTNode *root, ASTNode *combined) {
    for (size_t i = 0; i < root->list.count; i++) {
        ASTNode *item = root->list.items[i];
        if (item->type == AST_IMPORT_DECL) {
            continue;
        }
        ast_list_append(combined, item);
        root->list.items[i] = NULL;
    }
}

static int load_module(const char *path, ASTNode *combined, PathList *loaded, PathList *loading) {
    if (path_list_contains(loaded, path)) {
        return 1;
    }
    if (path_list_contains(loading, path)) {
        fprintf(stderr, "Pine import error: circular import involving '%s'\n", path);
        return 0;
    }

    char *source = read_source_file(path);
    if (!source) {
        return 0;
    }

    path_list_push(loading, path);

    Parser parser;
    parser_init(&parser, source);
    parser_set_source_path(&parser, path);
    ASTNode *root = parse_program(&parser);
    if (parser_error_count(&parser) > 0) {
        fprintf(stderr, "Pine stopped after %d parse error(s) in %s.\n", parser_error_count(&parser), path);
        ast_free(root);
        free(source);
        path_list_pop(loading);
        return 0;
    }

    char *base_dir = directory_name(path);
    int ok = load_imports(root, base_dir, combined, loaded, loading);
    free(base_dir);
    if (ok) {
        move_declarations(root, combined);
        path_list_push(loaded, path);
    }

    ast_free(root);
    free(source);
    path_list_pop(loading);
    return ok;
}

static void print_usage(void) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  pine <file>                 Transpile Pine to C on stdout\n");
    fprintf(stderr, "  pine build <file> [-o out]  Transpile and compile with a C compiler\n");
    fprintf(stderr, "  pine run <file>             Build and run a Pine program\n");
    fprintf(stderr, "  pine test <file>            Build and run a Pine test program\n");
    fprintf(stderr, "  pine ir <file>              Dump lowered Pine IR\n");
    fprintf(stderr, "  pine native <file> [--target name]\n");
    fprintf(stderr, "                              Emit architecture-aware debug native artifact\n");
    fprintf(stderr, "  pine targets                List supported native debug targets\n");
    fprintf(stderr, "  pine check <file>           Parse and type-check without emitting output\n");
    fprintf(stderr, "  pine version                Print compiler version\n");
}

static ASTNode *compile_to_checked_ast(const char *input_path, int *warning_count) {
    ASTNode *program = ast_make_program();
    PathList loaded = {0};
    PathList loading = {0};
    if (!load_module(input_path, program, &loaded, &loading)) {
        path_list_free(&loaded);
        path_list_free(&loading);
        ast_free(program);
        return NULL;
    }

    int local_warning_count = 0;
    if (!sema_analyze(program, &local_warning_count)) {
        if (local_warning_count > 0) {
            fprintf(stderr, "Pine reported %d warning(s).\n", local_warning_count);
        }
        ast_free(program);
        path_list_free(&loaded);
        path_list_free(&loading);
        return NULL;
    }
    if (local_warning_count > 0) {
        fprintf(stderr, "Pine compiled with %d warning(s).\n", local_warning_count);
    }

    if (warning_count) {
        *warning_count = local_warning_count;
    }
    path_list_free(&loaded);
    path_list_free(&loading);
    return program;
}

static int compile_to_c_file(const char *input_path, FILE *out) {
    ASTNode *program = compile_to_checked_ast(input_path, NULL);
    if (!program) {
        return 0;
    }

    codegen_generate_c(program, out);

    ast_free(program);
    return 1;
}

static int command_check(int argc, char **argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }
    ASTNode *program = compile_to_checked_ast(argv[2], NULL);
    if (!program) return 1;
    ast_free(program);
    return 0;
}

static int command_ir(int argc, char **argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    ASTNode *program = compile_to_checked_ast(argv[2], NULL);
    if (!program) {
        return 1;
    }

    IRModule *module = ir_lower_program(program);
    ir_dump_program(module, stdout);
    ir_free_module(module);
    ast_free(program);
    return 0;
}

static int command_native(int argc, char **argv) {
    const char *input_path = NULL;
    NativeTarget target = NATIVE_TARGET_X86_64;

    for (int i = 2; i < argc; i++) {
        const char *argument = argv[i];
        const char *target_name = NULL;
        if (strcmp(argument, "--target") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Pine native error: --target requires a target name.\n");
                return 1;
            }
            target_name = argv[i];
        } else if (strncmp(argument, "--target=", 9) == 0) {
            target_name = argument + 9;
        } else if (argument[0] == '-') {
            fprintf(stderr, "Pine native error: unknown option '%s'.\n", argument);
            return 1;
        } else if (!input_path) {
            input_path = argument;
        } else {
            fprintf(stderr, "Pine native error: unexpected argument '%s'.\n", argument);
            return 1;
        }

        if (target_name && !native_target_parse(target_name, &target)) {
            fprintf(stderr, "Pine native error: unsupported target '%s'.\n", target_name);
            fprintf(stderr, "Supported targets: x86_64, aarch64, riscv64.\n");
            return 1;
        }
    }

    if (!input_path) {
        print_usage();
        return 1;
    }

    ASTNode *program = compile_to_checked_ast(input_path, NULL);
    if (!program) {
        return 1;
    }

    // The native debug backend consumes the same lowered IR as `pine ir`
    // instead of walking the AST a second time.
    IRModule *module = ir_lower_program(program);
    native_emit_debug(module, target, stdout);
    ir_free_module(module);
    ast_free(program);
    return 0;
}

static char *replace_extension(const char *path, const char *extension) {
    const char *last_slash = strrchr(path, '/');
    const char *last_backslash = strrchr(path, '\\');
    const char *last_separator = last_slash;
    if (!last_separator || (last_backslash && last_backslash > last_separator)) {
        last_separator = last_backslash;
    }

    const char *last_dot = strrchr(path, '.');
    size_t base_length = strlen(path);
    if (last_dot && (!last_separator || last_dot > last_separator)) {
        base_length = (size_t)(last_dot - path);
    }

    size_t extension_length = strlen(extension);
    char *output = malloc(base_length + extension_length + 1);
    memcpy(output, path, base_length);
    memcpy(output + base_length, extension, extension_length + 1);
    return output;
}

static int command_succeeds(const char *command) {
    char probe[256];
    snprintf(probe, sizeof(probe), "%s --version > %s 2> %s", command, PINE_NULL_DEVICE, PINE_NULL_DEVICE);
    return system(probe) == 0;
}

static const char *find_c_compiler(void) {
    const char *env = getenv("CC");
    if (env && env[0] != '\0') {
        return env;
    }
    if (command_succeeds("cc")) return "cc";
    if (command_succeeds("gcc")) return "gcc";
    if (command_succeeds("clang")) return "clang";
    return NULL;
}

static int write_c_output(const char *input_path, const char *c_path) {
    FILE *out = fopen(c_path, "wb");
    if (!out) {
        perror(c_path);
        return 0;
    }
    int ok = compile_to_c_file(input_path, out);
    fclose(out);
    return ok;
}

static int build_executable(const char *input_path, const char *exe_path) {
    char *c_path = replace_extension(input_path, ".pine.c");
    if (!write_c_output(input_path, c_path)) {
        free(c_path);
        return 0;
    }

    const char *compiler = find_c_compiler();
    if (!compiler) {
        fprintf(stderr, "Pine build error: no C compiler found. Set CC or install cc, gcc, or clang.\n");
        fprintf(stderr, "Generated C was written to %s.\n", c_path);
        free(c_path);
        return 0;
    }

    char command[1024];
    snprintf(command, sizeof(command), "%s \"%s\" -o \"%s\"", compiler, c_path, exe_path);
    fprintf(stderr, "Pine build: %s\n", command);
    int result = system(command);
    free(c_path);
    return result == 0;
}

static int command_build(int argc, char **argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    char *default_output = replace_extension(argv[2], PINE_EXE_SUFFIX);
    const char *output = default_output;
    if (argc >= 5 && strcmp(argv[3], "-o") == 0) {
        output = argv[4];
    }

    int ok = build_executable(argv[2], output);
    if (ok) {
        fprintf(stderr, "Pine build: wrote %s\n", output);
    }
    free(default_output);
    return ok ? 0 : 1;
}

static int run_executable(const char *exe_path) {
    char command[1024];
#ifdef _WIN32
    snprintf(command, sizeof(command), "\"%s\"", exe_path);
#else
    snprintf(command, sizeof(command), "\"./%s\"", exe_path);
#endif
    return system(command);
}

static int command_run_or_test(int argc, char **argv, int is_test) {
    if (argc < 3) {
        print_usage();
        return 1;
    }

    char *exe_path = replace_extension(argv[2], PINE_EXE_SUFFIX);
    if (!build_executable(argv[2], exe_path)) {
        free(exe_path);
        return 1;
    }

    int result = run_executable(exe_path);
    if (is_test) {
        if (result == 0) {
            fprintf(stderr, "Pine test: passed\n");
        } else {
            fprintf(stderr, "Pine test: failed with exit code %d\n", result);
        }
    }
    free(exe_path);
    return result == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "build") == 0) {
        return command_build(argc, argv);
    }
    if (strcmp(argv[1], "run") == 0) {
        return command_run_or_test(argc, argv, 0);
    }
    if (strcmp(argv[1], "test") == 0) {
        return command_run_or_test(argc, argv, 1);
    }
    if (strcmp(argv[1], "check") == 0) {
        return command_check(argc, argv);
    }
    if (strcmp(argv[1], "version") == 0) {
        fprintf(stdout, "pine 0.1-dev\n");
        return 0;
    }
    if (strcmp(argv[1], "ir") == 0) {
        return command_ir(argc, argv);
    }
    if (strcmp(argv[1], "native") == 0) {
        return command_native(argc, argv);
    }
    if (strcmp(argv[1], "targets") == 0) {
        native_list_targets(stdout);
        return 0;
    }
    if (strcmp(argv[1], "transpile") == 0) {
        if (argc < 3) {
            print_usage();
            return 1;
        }
        return compile_to_c_file(argv[2], stdout) ? 0 : 1;
    }

    return compile_to_c_file(argv[1], stdout) ? 0 : 1;
}
