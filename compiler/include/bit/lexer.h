#ifndef BIT_LEXER_H
#define BIT_LEXER_H

#include <stddef.h>

#include "bit/token.h"

typedef struct BitLexer {
    const char *source;
    size_t length;
    size_t index;
    size_t line;
    size_t column;
} BitLexer;

void bit_lexer_init(BitLexer *lexer, const char *source, size_t length);
BitToken bit_lexer_next(BitLexer *lexer);

#endif
