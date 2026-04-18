#include "bit/token.h"

const char *bit_token_kind_name(BitTokenKind kind) {
    switch (kind) {
        case BIT_TOKEN_EOF:
            return "EOF";
        case BIT_TOKEN_IDENTIFIER:
            return "IDENT";
        case BIT_TOKEN_INTEGER:
            return "INTEGER";
        case BIT_TOKEN_KW_FN:
            return "KW_FN";
        case BIT_TOKEN_KW_RETURN:
            return "KW_RETURN";
        case BIT_TOKEN_KW_I32:
            return "KW_I32";
        case BIT_TOKEN_LPAREN:
            return "LPAREN";
        case BIT_TOKEN_RPAREN:
            return "RPAREN";
        case BIT_TOKEN_LBRACE:
            return "LBRACE";
        case BIT_TOKEN_RBRACE:
            return "RBRACE";
        case BIT_TOKEN_ARROW:
            return "ARROW";
        case BIT_TOKEN_COMMA:
            return "COMMA";
        case BIT_TOKEN_COLON:
            return "COLON";
        case BIT_TOKEN_SEMICOLON:
            return "SEMICOLON";
        case BIT_TOKEN_INVALID:
            return "INVALID";
    }

    return "UNKNOWN";
}
