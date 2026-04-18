#include "bit/lexer.h"

#include <ctype.h>
#include <stdlib.h>

static int bit_lexer_is_at_end(const BitLexer *lexer) {
    return lexer->index >= lexer->length;
}

static char bit_lexer_peek(const BitLexer *lexer) {
    if (bit_lexer_is_at_end(lexer)) {
        return '\0';
    }

    return lexer->source[lexer->index];
}

static char bit_lexer_advance(BitLexer *lexer) {
    char c;

    if (bit_lexer_is_at_end(lexer)) {
        return '\0';
    }

    c = lexer->source[lexer->index];
    lexer->index += 1;

    if (c == '\n') {
        lexer->line += 1;
        lexer->column = 1;
    } else {
        lexer->column += 1;
    }

    return c;
}

static void bit_lexer_skip_whitespace(BitLexer *lexer) {
    for (;;) {
        char c = bit_lexer_peek(lexer);

        switch (c) {
            case ' ':
            case '\r':
            case '\t':
            case '\n':
                bit_lexer_advance(lexer);
                break;
            default:
                return;
        }
    }
}

static BitToken bit_make_token(const BitLexer *lexer, BitTokenKind kind, size_t start, size_t line, size_t column) {
    BitToken token;

    token.kind = kind;
    token.start = lexer->source + start;
    token.length = lexer->index - start;
    token.line = line;
    token.column = column;
    return token;
}

static BitTokenKind bit_identifier_kind(const char *start, size_t length) {
    if (length == 2 && start[0] == 'f' && start[1] == 'n') {
        return BIT_TOKEN_KW_FN;
    }

    if (length == 3 && start[0] == 'i' && start[1] == '3' && start[2] == '2') {
        return BIT_TOKEN_KW_I32;
    }

    if (length == 3 && start[0] == 'l' && start[1] == 'e' && start[2] == 't') {
        return BIT_TOKEN_KW_LET;
    }

    if (length == 6 &&
        start[0] == 'r' &&
        start[1] == 'e' &&
        start[2] == 't' &&
        start[3] == 'u' &&
        start[4] == 'r' &&
        start[5] == 'n') {
        return BIT_TOKEN_KW_RETURN;
    }

    return BIT_TOKEN_IDENTIFIER;
}

static int bit_is_identifier_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int bit_is_identifier_continue(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

void bit_lexer_init(BitLexer *lexer, const char *source, size_t length) {
    lexer->source = source;
    lexer->length = length;
    lexer->index = 0;
    lexer->line = 1;
    lexer->column = 1;
}

BitToken bit_lexer_next(BitLexer *lexer) {
    size_t start;
    size_t line;
    size_t column;
    char c;

    bit_lexer_skip_whitespace(lexer);

    start = lexer->index;
    line = lexer->line;
    column = lexer->column;

    if (bit_lexer_is_at_end(lexer)) {
        return bit_make_token(lexer, BIT_TOKEN_EOF, start, line, column);
    }

    c = bit_lexer_advance(lexer);

    if (bit_is_identifier_start(c)) {
        while (bit_is_identifier_continue(bit_lexer_peek(lexer))) {
            bit_lexer_advance(lexer);
        }

        return bit_make_token(
            lexer,
            bit_identifier_kind(lexer->source + start, lexer->index - start),
            start,
            line,
            column
        );
    }

    if (isdigit((unsigned char)c)) {
        while (isdigit((unsigned char)bit_lexer_peek(lexer))) {
            bit_lexer_advance(lexer);
        }

        return bit_make_token(lexer, BIT_TOKEN_INTEGER, start, line, column);
    }

    switch (c) {
        case '(':
            return bit_make_token(lexer, BIT_TOKEN_LPAREN, start, line, column);
        case ')':
            return bit_make_token(lexer, BIT_TOKEN_RPAREN, start, line, column);
        case '{':
            return bit_make_token(lexer, BIT_TOKEN_LBRACE, start, line, column);
        case '}':
            return bit_make_token(lexer, BIT_TOKEN_RBRACE, start, line, column);
        case ',':
            return bit_make_token(lexer, BIT_TOKEN_COMMA, start, line, column);
        case ':':
            return bit_make_token(lexer, BIT_TOKEN_COLON, start, line, column);
        case '=':
            return bit_make_token(lexer, BIT_TOKEN_EQUAL, start, line, column);
        case ';':
            return bit_make_token(lexer, BIT_TOKEN_SEMICOLON, start, line, column);
        case '-':
            if (bit_lexer_peek(lexer) == '>') {
                bit_lexer_advance(lexer);
                return bit_make_token(lexer, BIT_TOKEN_ARROW, start, line, column);
            }
            break;
    }

    return bit_make_token(lexer, BIT_TOKEN_INVALID, start, line, column);
}

int bit_lex_all(const char *source, size_t length, BitToken **tokens_out, size_t *token_count_out) {
    BitLexer lexer;
    BitToken *tokens = NULL;
    size_t token_count = 0;
    size_t capacity = 0;

    bit_lexer_init(&lexer, source, length);

    for (;;) {
        BitToken token;
        BitToken *new_tokens;

        if (token_count == capacity) {
            size_t new_capacity = capacity == 0 ? 16 : capacity * 2;

            new_tokens = (BitToken *)realloc(tokens, new_capacity * sizeof(BitToken));
            if (!new_tokens) {
                free(tokens);
                return 1;
            }

            tokens = new_tokens;
            capacity = new_capacity;
        }

        token = bit_lexer_next(&lexer);
        tokens[token_count++] = token;

        if (token.kind == BIT_TOKEN_EOF || token.kind == BIT_TOKEN_INVALID) {
            break;
        }
    }

    *tokens_out = tokens;
    *token_count_out = token_count;
    return 0;
}
