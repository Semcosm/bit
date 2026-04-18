#ifndef BIT_PARSER_H
#define BIT_PARSER_H

#include <stddef.h>
#include <stdio.h>

#include "bit/arena.h"
#include "bit/ast.h"
#include "bit/token.h"

typedef enum BitParseStatus {
    BIT_PARSE_OK = 0,
    BIT_PARSE_ERROR,
} BitParseStatus;

typedef struct BitParseDiagnostic {
    const char *message;
    BitTokenKind got;
    BitTokenKind expected[4];
    size_t expected_count;
    BitSourceSpan span;
} BitParseDiagnostic;

typedef struct BitParseResult {
    BitParseStatus status;
    BitModule *module;
    BitParseDiagnostic diagnostic;
} BitParseResult;

BitParseResult bit_parse_module(const BitToken *tokens, size_t token_count, BitArena *arena);
void bit_print_parse_diagnostic(FILE *stream, const BitParseDiagnostic *diagnostic);

#endif
