#include "bit/parser.h"

#include <stdint.h>

typedef struct BitParser {
    const BitToken *tokens;
    size_t token_count;
    size_t index;
    BitArena *arena;
    BitParseDiagnostic diagnostic;
} BitParser;

static BitSourceSpan bit_span_from_token(const BitToken *token) {
    BitSourceSpan span;

    span.start = token->start;
    span.length = token->length;
    span.line = token->line;
    span.column = token->column;
    return span;
}

static BitSourceSpan bit_span_from_range(const BitToken *start, const BitToken *end) {
    BitSourceSpan span;
    const char *start_ptr = start->start;
    const char *end_ptr = end->start + end->length;

    span.start = start_ptr;
    span.length = (size_t)(end_ptr - start_ptr);
    span.line = start->line;
    span.column = start->column;
    return span;
}

static const BitToken *bit_parser_fallback_token(void) {
    static const BitToken token = {
        BIT_TOKEN_EOF,
        NULL,
        0,
        1,
        1,
    };

    return &token;
}

static const BitToken *bit_parser_current(BitParser *parser) {
    if (!parser->tokens || parser->token_count == 0) {
        return bit_parser_fallback_token();
    }

    if (parser->index >= parser->token_count) {
        return &parser->tokens[parser->token_count - 1];
    }

    return &parser->tokens[parser->index];
}

static const BitToken *bit_parser_previous(BitParser *parser) {
    if (!parser->tokens || parser->token_count == 0) {
        return bit_parser_fallback_token();
    }

    if (parser->index == 0) {
        return &parser->tokens[0];
    }

    return &parser->tokens[parser->index - 1];
}

static int bit_parser_is_at_end(BitParser *parser) {
    return bit_parser_current(parser)->kind == BIT_TOKEN_EOF;
}

static const BitToken *bit_parser_advance(BitParser *parser) {
    const BitToken *current = bit_parser_current(parser);

    if (!bit_parser_is_at_end(parser) && parser->index + 1 < parser->token_count) {
        parser->index += 1;
    }

    return current;
}

static int bit_parser_match(BitParser *parser, BitTokenKind kind) {
    if (bit_parser_current(parser)->kind != kind) {
        return 0;
    }

    bit_parser_advance(parser);
    return 1;
}

static void bit_parser_set_error(
    BitParser *parser,
    const char *message,
    const BitToken *got,
    const BitTokenKind *expected,
    size_t expected_count
) {
    size_t i;

    parser->diagnostic.message = message;
    parser->diagnostic.got = got ? got->kind : BIT_TOKEN_EOF;
    parser->diagnostic.expected_count = expected_count > 4 ? 4 : expected_count;
    parser->diagnostic.span = got ? bit_span_from_token(got) : bit_span_from_token(bit_parser_fallback_token());

    for (i = 0; i < parser->diagnostic.expected_count; ++i) {
        parser->diagnostic.expected[i] = expected[i];
    }
}

static const BitToken *bit_parser_expect(BitParser *parser, BitTokenKind kind, const char *message) {
    BitTokenKind expected_kind = kind;
    const BitToken *current = bit_parser_current(parser);

    if (current->kind == BIT_TOKEN_INVALID) {
        bit_parser_set_error(parser, "invalid token", current, NULL, 0);
        return NULL;
    }

    if (!bit_parser_match(parser, kind)) {
        bit_parser_set_error(parser, message, current, &expected_kind, 1);
        return NULL;
    }

    return bit_parser_previous(parser);
}

static void *bit_parser_alloc(BitParser *parser, size_t size) {
    return bit_arena_alloc(parser->arena, size);
}

static int bit_parse_integer_value(BitParser *parser, const BitToken *token, uint64_t *value_out) {
    uint64_t value = 0;
    size_t i;

    for (i = 0; i < token->length; ++i) {
        uint64_t digit = (uint64_t)(token->start[i] - '0');

        if (value > UINT64_MAX / 10 || (value == UINT64_MAX / 10 && digit > UINT64_MAX % 10)) {
            bit_parser_set_error(parser, "integer literal out of range", token, NULL, 0);
            return 0;
        }

        value = value * 10 + digit;
    }

    *value_out = value;
    return 1;
}

static int bit_parse_type_ref(BitParser *parser, BitTypeRef *type_out) {
    const BitToken *token = bit_parser_expect(parser, BIT_TOKEN_KW_I32, "expected type");

    if (!token) {
        return 0;
    }

    type_out->kind = BIT_TYPE_I32;
    type_out->span = bit_span_from_token(token);
    return 1;
}

static BitExpr *bit_parse_integer_expr(BitParser *parser) {
    BitExpr *expr;
    const BitToken *token = bit_parser_expect(parser, BIT_TOKEN_INTEGER, "expected integer literal");
    uint64_t value;

    if (!token) {
        return NULL;
    }

    if (!bit_parse_integer_value(parser, token, &value)) {
        return NULL;
    }

    expr = (BitExpr *)bit_parser_alloc(parser, sizeof(BitExpr));
    if (!expr) {
        bit_parser_set_error(parser, "out of memory", token, NULL, 0);
        return NULL;
    }

    expr->kind = BIT_EXPR_INTEGER;
    expr->span = bit_span_from_token(token);
    expr->as.integer.value = value;
    expr->as.integer.span = expr->span;
    return expr;
}

static BitStmt *bit_parse_return_stmt(BitParser *parser) {
    BitStmt *stmt;
    BitExpr *expr;
    const BitToken *return_token = bit_parser_expect(parser, BIT_TOKEN_KW_RETURN, "expected 'return'");
    const BitToken *semicolon_token;

    if (!return_token) {
        return NULL;
    }

    expr = bit_parse_integer_expr(parser);
    if (!expr) {
        return NULL;
    }

    semicolon_token = bit_parser_expect(parser, BIT_TOKEN_SEMICOLON, "expected ';'");
    if (!semicolon_token) {
        return NULL;
    }

    stmt = (BitStmt *)bit_parser_alloc(parser, sizeof(BitStmt));
    if (!stmt) {
        bit_parser_set_error(parser, "out of memory", return_token, NULL, 0);
        return NULL;
    }

    stmt->kind = BIT_STMT_RETURN;
    stmt->span = bit_span_from_range(return_token, semicolon_token);
    stmt->as.ret.expr = expr;
    stmt->as.ret.span = stmt->span;
    return stmt;
}

static int bit_parse_block(BitParser *parser, BitBlock *block_out) {
    BitStmt *stmt;
    BitStmt **stmts;
    const BitToken *left_brace = bit_parser_expect(parser, BIT_TOKEN_LBRACE, "expected '{'");
    const BitToken *right_brace;

    if (!left_brace) {
        return 0;
    }

    stmt = bit_parse_return_stmt(parser);
    if (!stmt) {
        return 0;
    }

    right_brace = bit_parser_expect(parser, BIT_TOKEN_RBRACE, "expected '}'");
    if (!right_brace) {
        return 0;
    }

    stmts = (BitStmt **)bit_parser_alloc(parser, sizeof(BitStmt *));
    if (!stmts) {
        bit_parser_set_error(parser, "out of memory", left_brace, NULL, 0);
        return 0;
    }

    stmts[0] = stmt;
    block_out->stmts = stmts;
    block_out->stmt_count = 1;
    block_out->span = bit_span_from_range(left_brace, right_brace);
    return 1;
}

static BitFunctionDecl *bit_parse_function_decl(BitParser *parser) {
    BitFunctionDecl *function;
    const BitToken *fn_token = bit_parser_expect(parser, BIT_TOKEN_KW_FN, "expected 'fn'");
    const BitToken *name_token;
    const BitToken *left_paren;
    const BitToken *right_paren;
    const BitToken *arrow_token;
    BitTypeRef return_type;
    BitBlock body;

    if (!fn_token) {
        return NULL;
    }

    name_token = bit_parser_expect(parser, BIT_TOKEN_IDENTIFIER, "expected identifier");
    if (!name_token) {
        return NULL;
    }

    left_paren = bit_parser_expect(parser, BIT_TOKEN_LPAREN, "expected '('");
    if (!left_paren) {
        return NULL;
    }

    right_paren = bit_parser_expect(parser, BIT_TOKEN_RPAREN, "expected ')'");
    if (!right_paren) {
        return NULL;
    }

    arrow_token = bit_parser_expect(parser, BIT_TOKEN_ARROW, "expected '->'");
    if (!arrow_token) {
        return NULL;
    }

    (void)left_paren;
    (void)right_paren;
    (void)arrow_token;

    if (!bit_parse_type_ref(parser, &return_type)) {
        return NULL;
    }

    if (!bit_parse_block(parser, &body)) {
        return NULL;
    }

    function = (BitFunctionDecl *)bit_parser_alloc(parser, sizeof(BitFunctionDecl));
    if (!function) {
        bit_parser_set_error(parser, "out of memory", fn_token, NULL, 0);
        return NULL;
    }

    function->name.data = name_token->start;
    function->name.length = name_token->length;
    function->return_type = return_type;
    function->body = body;
    function->span = bit_span_from_range(fn_token, bit_parser_previous(parser));
    return function;
}

BitParseResult bit_parse_module(const BitToken *tokens, size_t token_count, BitArena *arena) {
    BitParseResult result;
    BitParser parser;
    BitModule *module;
    BitFunctionDecl *function;
    BitFunctionDecl **functions;
    const BitToken *eof_token;

    result.status = BIT_PARSE_ERROR;
    result.module = NULL;
    result.diagnostic.message = NULL;
    result.diagnostic.got = BIT_TOKEN_EOF;
    result.diagnostic.expected_count = 0;
    result.diagnostic.span = bit_span_from_token(bit_parser_fallback_token());

    parser.tokens = tokens;
    parser.token_count = token_count;
    parser.index = 0;
    parser.arena = arena;
    parser.diagnostic = result.diagnostic;

    function = bit_parse_function_decl(&parser);
    if (!function) {
        result.diagnostic = parser.diagnostic;
        return result;
    }

    eof_token = bit_parser_expect(&parser, BIT_TOKEN_EOF, "expected end of file");
    if (!eof_token) {
        result.diagnostic = parser.diagnostic;
        return result;
    }

    functions = (BitFunctionDecl **)bit_parser_alloc(&parser, sizeof(BitFunctionDecl *));
    if (!functions) {
        bit_parser_set_error(&parser, "out of memory", eof_token, NULL, 0);
        result.diagnostic = parser.diagnostic;
        return result;
    }

    module = (BitModule *)bit_parser_alloc(&parser, sizeof(BitModule));
    if (!module) {
        bit_parser_set_error(&parser, "out of memory", eof_token, NULL, 0);
        result.diagnostic = parser.diagnostic;
        return result;
    }

    functions[0] = function;
    module->functions = functions;
    module->function_count = 1;
    module->span = function->span;

    result.status = BIT_PARSE_OK;
    result.module = module;
    result.diagnostic = parser.diagnostic;
    return result;
}

void bit_print_parse_diagnostic(FILE *stream, const BitParseDiagnostic *diagnostic) {
    const char *message = diagnostic->message ? diagnostic->message : "parse error";

    fprintf(
        stream,
        "parse error: %s, got %s at %zu:%zu\n",
        message,
        bit_token_kind_name(diagnostic->got),
        diagnostic->span.line,
        diagnostic->span.column
    );
}
