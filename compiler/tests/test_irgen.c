#include "bit/arena.h"
#include "bit/file.h"
#include "bit/irgen.h"
#include "bit/lexer.h"
#include "bit/parser.h"
#include "bit/sema.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    BitArena *arena = NULL;
    BitIrgenOptions irgen_options;
    BitIrgenResult irgen_result;
    BitParseResult parse_result;
    BitSemaResult sema_result;
    BitToken *tokens = NULL;
    char *source = NULL;
    size_t token_count = 0;
    size_t source_length = 0;
    int status = 1;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <input.bit> <output.ll>\n", argv[0]);
        return 1;
    }

    if (bit_read_file(argv[1], &source, &source_length) != 0) {
        goto cleanup;
    }

    if (bit_lex_all(source, source_length, &tokens, &token_count) != 0) {
        fprintf(stderr, "error: failed to lex '%s'\n", argv[1]);
        goto cleanup;
    }

    arena = bit_arena_create();
    if (!arena) {
        fprintf(stderr, "error: failed to create arena\n");
        goto cleanup;
    }

    parse_result = bit_parse_module(tokens, token_count, arena);
    if (parse_result.status != BIT_PARSE_OK) {
        bit_print_parse_diagnostic(stderr, &parse_result.diagnostic);
        goto cleanup;
    }

    sema_result = bit_analyze_module(parse_result.module);
    if (sema_result.status != BIT_SEMA_OK) {
        bit_print_sema_diagnostic(stderr, &sema_result.diagnostic);
        goto cleanup;
    }

    irgen_options.module_name = "bit_test_module";
    irgen_options.source_name = argv[1];
    irgen_options.verify_module = 1;

    irgen_result = bit_emit_llvm_ir_file(parse_result.module, &irgen_options, argv[2]);
    if (irgen_result.status != BIT_IRGEN_OK) {
        bit_print_irgen_diagnostic(stderr, &irgen_result.diagnostic);
        goto cleanup;
    }

    puts("irgen ok");
    status = 0;

cleanup:
    bit_arena_destroy(arena);
    free(tokens);
    free(source);
    return status;
}
