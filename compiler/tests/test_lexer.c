#include "bit/lexer.h"

#include <stdio.h>
#include <stdlib.h>

static int bit_read_file(const char *path, char **buffer_out, size_t *length_out) {
    FILE *file;
    char *buffer;
    long file_size;
    size_t bytes_read;

    file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "error: failed to open '%s'\n", path);
        return 1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "error: failed to seek '%s'\n", path);
        fclose(file);
        return 1;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fprintf(stderr, "error: failed to measure '%s'\n", path);
        fclose(file);
        return 1;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "error: failed to rewind '%s'\n", path);
        fclose(file);
        return 1;
    }

    buffer = (char *)malloc((size_t)file_size + 1);
    if (!buffer) {
        fprintf(stderr, "error: out of memory while reading '%s'\n", path);
        fclose(file);
        return 1;
    }

    bytes_read = fread(buffer, 1, (size_t)file_size, file);
    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "error: failed to read '%s'\n", path);
        free(buffer);
        fclose(file);
        return 1;
    }

    buffer[bytes_read] = '\0';
    fclose(file);

    *buffer_out = buffer;
    *length_out = bytes_read;
    return 0;
}

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
    BitLexer lexer;
    BitToken token;
    char *source;
    size_t length;
    int saw_invalid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <input.bit>\n", argv[0]);
        return 1;
    }

    if (bit_read_file(argv[1], &source, &length) != 0) {
        return 1;
    }

    bit_lexer_init(&lexer, source, length);
    saw_invalid = 0;

    do {
        token = bit_lexer_next(&lexer);
        printf("%zu:%zu %-12s ", token.line, token.column, bit_token_kind_name(token.kind));
        bit_print_escaped_slice(token.start, token.length);
        putchar('\n');

        if (token.kind == BIT_TOKEN_INVALID) {
            saw_invalid = 1;
        }
    } while (token.kind != BIT_TOKEN_EOF && token.kind != BIT_TOKEN_INVALID);

    free(source);
    return saw_invalid ? 1 : 0;
}
