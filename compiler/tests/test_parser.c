#include "bit/arena.h"
#include "bit/ast.h"
#include "bit/file.h"
#include "bit/lexer.h"
#include "bit/parser.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    BitArena *arena;
    BitParseResult parse_result;
    BitToken *tokens;
    char *source;
    size_t token_count;
    size_t length;
    int status = 1;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <input.bit>\n", argv[0]);
        return 1;
    }

    if (bit_read_file(argv[1], &source, &length) != 0) {
        return 1;
    }

    if (bit_lex_all(source, length, &tokens, &token_count) != 0) {
        fprintf(stderr, "error: failed to lex '%s'\n", argv[1]);
        free(source);
        return 1;
    }

    arena = bit_arena_create();
    if (!arena) {
        fprintf(stderr, "error: failed to create arena\n");
        free(tokens);
        free(source);
        return 1;
    }

    parse_result = bit_parse_module(tokens, token_count, arena);
    if (parse_result.status != BIT_PARSE_OK) {
        bit_print_parse_diagnostic(stderr, &parse_result.diagnostic);
        goto cleanup;
    }

    bit_ast_dump_module(stdout, parse_result.module);
    putchar('\n');
    status = 0;

cleanup:
    bit_arena_destroy(arena);
    free(tokens);
    free(source);
    return status;
}
