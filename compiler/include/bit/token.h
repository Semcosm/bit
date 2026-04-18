#ifndef BIT_TOKEN_H
#define BIT_TOKEN_H

#include <stddef.h>

typedef enum BitTokenKind {
    BIT_TOKEN_EOF = 0,
    BIT_TOKEN_IDENTIFIER,
    BIT_TOKEN_INTEGER,

    BIT_TOKEN_KW_FN,
    BIT_TOKEN_KW_RETURN,
    BIT_TOKEN_KW_I32,

    BIT_TOKEN_LPAREN,
    BIT_TOKEN_RPAREN,
    BIT_TOKEN_LBRACE,
    BIT_TOKEN_RBRACE,
    BIT_TOKEN_ARROW,
    BIT_TOKEN_COMMA,
    BIT_TOKEN_COLON,
    BIT_TOKEN_SEMICOLON,

    BIT_TOKEN_INVALID
} BitTokenKind;

typedef struct BitToken {
    BitTokenKind kind;
    const char *start;
    size_t length;
    size_t line;
    size_t column;
} BitToken;

const char *bit_token_kind_name(BitTokenKind kind);

#endif
