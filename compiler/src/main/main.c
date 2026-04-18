#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bit/arena.h"
#include "bit/file.h"
#include "bit/irgen.h"
#include "bit/lexer.h"
#include "bit/parser.h"

static void bit_print_usage(const char *prog) {
    fprintf(stderr, "usage: %s <input.bit> -o <output.ll>\n", prog);
}

int main(int argc, char **argv) {
    BitArena *arena = NULL;
    BitParseResult parse_result;
    BitToken *tokens = NULL;
    char *source = NULL;
    size_t token_count = 0;
    size_t source_length = 0;
    const char *input_path = NULL;
    const char *output_path = NULL;
    int status = 1;

    if (argc < 4) {
        bit_print_usage(argv[0]);
        return 1;
    }

    input_path = argv[1];

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: missing value after -o\n");
                return 1;
            }
            output_path = argv[i + 1];
            ++i;
            continue;
        }

        fprintf(stderr, "error: unknown argument: %s\n", argv[i]);
        bit_print_usage(argv[0]);
        return 1;
    }

    if (!input_path || !output_path) {
        bit_print_usage(argv[0]);
        return 1;
    }

    if (bit_read_file(input_path, &source, &source_length) != 0) {
        goto cleanup;
    }

    if (bit_lex_all(source, source_length, &tokens, &token_count) != 0) {
        fprintf(stderr, "error: failed to lex '%s'\n", input_path);
        goto cleanup;
    }

    arena = bit_arena_create();
    if (!arena) {
        fprintf(stderr, "error: failed to create parser arena\n");
        goto cleanup;
    }

    parse_result = bit_parse_module(tokens, token_count, arena);
    if (parse_result.status != BIT_PARSE_OK) {
        bit_print_parse_diagnostic(stderr, &parse_result.diagnostic);
        goto cleanup;
    }

    printf("bitc: input = %s\n", input_path);
    printf("bitc: output = %s\n", output_path);
    printf("bitc: parsed module successfully\n");
    printf("bitc: stage0 stub compiler active\n");
    status = bit_emit_minimal_module(output_path);

cleanup:
    bit_arena_destroy(arena);
    free(tokens);
    free(source);
    return status;
}
