#include "bit/file.h"
#include "bit/lexer.h"

#include <stdio.h>
#include <stdlib.h>

static void bit_print_escaped_slice(const char *start, size_t length) {
    size_t i;

    putchar('"');

    for (i = 0; i < length; ++i) {
        char c = start[i];

        switch (c) {
            case '\n':
                fputs("\\n", stdout);
                break;
            case '\r':
                fputs("\\r", stdout);
                break;
            case '\t':
                fputs("\\t", stdout);
                break;
            case '"':
                fputs("\\\"", stdout);
                break;
            case '\\':
                fputs("\\\\", stdout);
                break;
            default:
                putchar(c);
                break;
        }
    }

    putchar('"');
}

int main(int argc, char **argv) {
    BitToken *tokens;
    char *source;
    size_t token_count;
    size_t length;
    size_t i;
    int saw_invalid;

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

    saw_invalid = 0;

    for (i = 0; i < token_count; ++i) {
        BitToken token = tokens[i];

        printf("%zu:%zu %-12s ", token.line, token.column, bit_token_kind_name(token.kind));
        bit_print_escaped_slice(token.start, token.length);
        putchar('\n');

        if (token.kind == BIT_TOKEN_INVALID) {
            saw_invalid = 1;
        }
    }

    free(tokens);
    free(source);
    return saw_invalid ? 1 : 0;
}
