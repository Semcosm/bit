#include "bit/parser.h"

#include <stdint.h>
#include <stdlib.h>

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

static BitSourceSpan bit_span_from_expr_range(const BitExpr *left, const BitExpr *right) {
    BitSourceSpan span;
    const char *start_ptr = left->span.start;
    const char *end_ptr = right->span.start + right->span.length;

    span.start = start_ptr;
    span.length = (size_t)(end_ptr - start_ptr);
    span.line = left->span.line;
    span.column = left->span.column;
    return span;
}

static BitSourceSpan bit_span_from_token_and_expr(const BitToken *start, const BitExpr *expr) {
    BitSourceSpan span;
    const char *start_ptr = start->start;
    const char *end_ptr = expr->span.start + expr->span.length;

    span.start = start_ptr;
    span.length = (size_t)(end_ptr - start_ptr);
    span.line = start->line;
    span.column = start->column;
    return span;
}

static BitSourceSpan bit_span_from_token_and_token(const BitToken *start, const BitToken *end) {
    return bit_span_from_range(start, end);
}

static BitSourceSpan bit_span_from_spans(BitSourceSpan start, BitSourceSpan end) {
    BitSourceSpan span;
    const char *start_ptr = start.start;
    const char *end_ptr = end.start + end.length;

    span.start = start_ptr;
    span.length = (size_t)(end_ptr - start_ptr);
    span.line = start.line;
    span.column = start.column;
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

static BitBinaryOpKind bit_token_binary_op(BitTokenKind kind) {
    switch (kind) {
        case BIT_TOKEN_PLUS:
            return BIT_BINARY_OP_ADD;
        case BIT_TOKEN_MINUS:
            return BIT_BINARY_OP_SUB;
        case BIT_TOKEN_STAR:
            return BIT_BINARY_OP_MUL;
        case BIT_TOKEN_SLASH:
            return BIT_BINARY_OP_DIV;
        default:
            return BIT_BINARY_OP_ADD;
    }
}

static int bit_parser_binary_precedence(BitTokenKind kind) {
    switch (kind) {
        case BIT_TOKEN_PLUS:
        case BIT_TOKEN_MINUS:
            return 10;
        case BIT_TOKEN_STAR:
        case BIT_TOKEN_SLASH:
            return 20;
        default:
            return -1;
    }
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

static BitExpr *bit_make_integer_expr(BitParser *parser, const BitToken *token, uint64_t value) {
    BitExpr *expr = (BitExpr *)bit_parser_alloc(parser, sizeof(BitExpr));

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

static BitExpr *bit_make_name_expr(BitParser *parser, const BitToken *token) {
    BitExpr *expr = (BitExpr *)bit_parser_alloc(parser, sizeof(BitExpr));

    if (!expr) {
        bit_parser_set_error(parser, "out of memory", token, NULL, 0);
        return NULL;
    }

    expr->kind = BIT_EXPR_IDENTIFIER;
    expr->span = bit_span_from_token(token);
    expr->as.name.name.data = token->start;
    expr->as.name.name.length = token->length;
    expr->as.name.span = expr->span;
    return expr;
}

static BitExpr *bit_make_call_expr(
    BitParser *parser,
    const BitToken *callee_token,
    BitExpr **args,
    size_t arg_count,
    const BitToken *right_paren
) {
    BitExpr *expr = (BitExpr *)bit_parser_alloc(parser, sizeof(BitExpr));

    if (!expr) {
        bit_parser_set_error(parser, "out of memory", callee_token, NULL, 0);
        return NULL;
    }

    expr->kind = BIT_EXPR_CALL;
    expr->span = bit_span_from_token_and_token(callee_token, right_paren);
    expr->as.call.callee.data = callee_token->start;
    expr->as.call.callee.length = callee_token->length;
    expr->as.call.args = args;
    expr->as.call.arg_count = arg_count;
    expr->as.call.span = expr->span;
    return expr;
}

static BitExpr *bit_make_unary_expr(BitParser *parser, BitUnaryOpKind op, const BitToken *operator_token, BitExpr *operand) {
    BitExpr *expr = (BitExpr *)bit_parser_alloc(parser, sizeof(BitExpr));

    if (!expr) {
        bit_parser_set_error(parser, "out of memory", operator_token, NULL, 0);
        return NULL;
    }

    expr->kind = BIT_EXPR_UNARY;
    expr->span = bit_span_from_token_and_expr(operator_token, operand);
    expr->as.unary.op = op;
    expr->as.unary.operand = operand;
    expr->as.unary.span = expr->span;
    return expr;
}

static BitExpr *bit_make_binary_expr(BitParser *parser, BitBinaryOpKind op, BitExpr *left, BitExpr *right) {
    BitExpr *expr = (BitExpr *)bit_parser_alloc(parser, sizeof(BitExpr));

    if (!expr) {
        bit_parser_set_error(parser, "out of memory", bit_parser_current(parser), NULL, 0);
        return NULL;
    }

    expr->kind = BIT_EXPR_BINARY;
    expr->span = bit_span_from_expr_range(left, right);
    expr->as.binary.op = op;
    expr->as.binary.left = left;
    expr->as.binary.right = right;
    expr->as.binary.span = expr->span;
    return expr;
}

static BitExpr *bit_parse_expr(BitParser *parser);

static BitExpr *bit_parse_integer_expr(BitParser *parser) {
    const BitToken *token = bit_parser_expect(parser, BIT_TOKEN_INTEGER, "expected integer literal");
    uint64_t value;

    if (!token) {
        return NULL;
    }

    if (!bit_parse_integer_value(parser, token, &value)) {
        return NULL;
    }

    return bit_make_integer_expr(parser, token, value);
}

static int bit_parse_call_args(BitParser *parser, BitExpr ***args_out, size_t *arg_count_out, const BitToken **right_paren_out) {
    BitExpr **final_args = NULL;
    BitExpr **temp_args = NULL;
    size_t arg_count = 0;
    size_t arg_capacity = 0;
    size_t i;
    const BitToken *right_paren;

    if (!bit_parser_expect(parser, BIT_TOKEN_LPAREN, "expected '('")) {
        return 0;
    }

    while (bit_parser_current(parser)->kind != BIT_TOKEN_RPAREN && !bit_parser_is_at_end(parser)) {
        BitExpr **new_args;
        BitExpr *arg = bit_parse_expr(parser);

        if (!arg) {
            free(temp_args);
            return 0;
        }

        if (arg_count == arg_capacity) {
            size_t new_capacity = arg_capacity == 0 ? 4 : arg_capacity * 2;

            new_args = (BitExpr **)realloc(temp_args, new_capacity * sizeof(BitExpr *));
            if (!new_args) {
                bit_parser_set_error(parser, "out of memory", bit_parser_current(parser), NULL, 0);
                free(temp_args);
                return 0;
            }

            temp_args = new_args;
            arg_capacity = new_capacity;
        }

        temp_args[arg_count++] = arg;

        if (!bit_parser_match(parser, BIT_TOKEN_COMMA)) {
            break;
        }
    }

    right_paren = bit_parser_expect(parser, BIT_TOKEN_RPAREN, "expected ')'");
    if (!right_paren) {
        free(temp_args);
        return 0;
    }

    if (arg_count > 0) {
        final_args = (BitExpr **)bit_parser_alloc(parser, arg_count * sizeof(BitExpr *));
        if (!final_args) {
            bit_parser_set_error(parser, "out of memory", right_paren, NULL, 0);
            free(temp_args);
            return 0;
        }

        for (i = 0; i < arg_count; ++i) {
            final_args[i] = temp_args[i];
        }
    }

    free(temp_args);
    *args_out = final_args;
    *arg_count_out = arg_count;
    *right_paren_out = right_paren;
    return 1;
}

static BitExpr *bit_parse_identifier_or_call_expr(BitParser *parser) {
    BitExpr **args = NULL;
    size_t arg_count = 0;
    const BitToken *token = bit_parser_expect(parser, BIT_TOKEN_IDENTIFIER, "expected identifier");
    const BitToken *right_paren = NULL;

    if (!token) {
        return NULL;
    }

    if (bit_parser_current(parser)->kind != BIT_TOKEN_LPAREN) {
        return bit_make_name_expr(parser, token);
    }

    if (!bit_parse_call_args(parser, &args, &arg_count, &right_paren)) {
        return NULL;
    }

    return bit_make_call_expr(parser, token, args, arg_count, right_paren);
}

static BitExpr *bit_parse_paren_expr(BitParser *parser) {
    BitExpr *expr;

    if (!bit_parser_expect(parser, BIT_TOKEN_LPAREN, "expected '('")) {
        return NULL;
    }

    expr = bit_parse_expr(parser);
    if (!expr) {
        return NULL;
    }

    if (!bit_parser_expect(parser, BIT_TOKEN_RPAREN, "expected ')'")) {
        return NULL;
    }

    return expr;
}

static BitExpr *bit_parse_primary_expr(BitParser *parser) {
    BitTokenKind expected[3] = {BIT_TOKEN_LPAREN, BIT_TOKEN_IDENTIFIER, BIT_TOKEN_INTEGER};
    const BitToken *current = bit_parser_current(parser);

    switch (current->kind) {
        case BIT_TOKEN_INTEGER:
            return bit_parse_integer_expr(parser);
        case BIT_TOKEN_IDENTIFIER:
            return bit_parse_identifier_or_call_expr(parser);
        case BIT_TOKEN_LPAREN:
            return bit_parse_paren_expr(parser);
        case BIT_TOKEN_INVALID:
            bit_parser_set_error(parser, "invalid token", current, NULL, 0);
            return NULL;
        default:
            bit_parser_set_error(parser, "expected expression", current, expected, 3);
            return NULL;
    }
}

static BitExpr *bit_parse_unary_expr(BitParser *parser) {
    const BitToken *current = bit_parser_current(parser);
    BitExpr *operand;

    if (current->kind != BIT_TOKEN_MINUS) {
        return bit_parse_primary_expr(parser);
    }

    bit_parser_advance(parser);
    operand = bit_parse_unary_expr(parser);
    if (!operand) {
        return NULL;
    }

    return bit_make_unary_expr(parser, BIT_UNARY_OP_NEG, current, operand);
}

static BitExpr *bit_parse_binary_expr_rhs(BitParser *parser, int min_precedence, BitExpr *left) {
    for (;;) {
        BitTokenKind operator_kind = bit_parser_current(parser)->kind;
        int precedence = bit_parser_binary_precedence(operator_kind);
        BitExpr *right;

        if (precedence < min_precedence) {
            return left;
        }

        bit_parser_advance(parser);

        right = bit_parse_unary_expr(parser);
        if (!right) {
            return NULL;
        }

        for (;;) {
            BitTokenKind next_kind = bit_parser_current(parser)->kind;
            int next_precedence = bit_parser_binary_precedence(next_kind);

            if (next_precedence <= precedence) {
                break;
            }

            right = bit_parse_binary_expr_rhs(parser, next_precedence, right);
            if (!right) {
                return NULL;
            }
        }

        left = bit_make_binary_expr(parser, bit_token_binary_op(operator_kind), left, right);
        if (!left) {
            return NULL;
        }
    }
}

static BitExpr *bit_parse_expr(BitParser *parser) {
    BitExpr *left = bit_parse_unary_expr(parser);

    if (!left) {
        return NULL;
    }

    return bit_parse_binary_expr_rhs(parser, 0, left);
}

static BitStmt *bit_parse_let_stmt(BitParser *parser) {
    BitStmt *stmt;
    BitTypeRef type;
    BitExpr *initializer;
    const BitToken *let_token = bit_parser_expect(parser, BIT_TOKEN_KW_LET, "expected 'let'");
    const BitToken *name_token;
    const BitToken *semicolon_token;

    if (!let_token) {
        return NULL;
    }

    name_token = bit_parser_expect(parser, BIT_TOKEN_IDENTIFIER, "expected identifier");
    if (!name_token) {
        return NULL;
    }

    if (!bit_parser_expect(parser, BIT_TOKEN_COLON, "expected ':'")) {
        return NULL;
    }

    if (!bit_parse_type_ref(parser, &type)) {
        return NULL;
    }

    if (!bit_parser_expect(parser, BIT_TOKEN_EQUAL, "expected '='")) {
        return NULL;
    }

    initializer = bit_parse_expr(parser);
    if (!initializer) {
        return NULL;
    }

    semicolon_token = bit_parser_expect(parser, BIT_TOKEN_SEMICOLON, "expected ';'");
    if (!semicolon_token) {
        return NULL;
    }

    stmt = (BitStmt *)bit_parser_alloc(parser, sizeof(BitStmt));
    if (!stmt) {
        bit_parser_set_error(parser, "out of memory", let_token, NULL, 0);
        return NULL;
    }

    stmt->kind = BIT_STMT_LET;
    stmt->span = bit_span_from_range(let_token, semicolon_token);
    stmt->as.let.name.data = name_token->start;
    stmt->as.let.name.length = name_token->length;
    stmt->as.let.type = type;
    stmt->as.let.initializer = initializer;
    stmt->as.let.span = stmt->span;
    return stmt;
}

static BitStmt *bit_parse_return_stmt(BitParser *parser) {
    BitStmt *stmt;
    BitExpr *expr;
    const BitToken *return_token = bit_parser_expect(parser, BIT_TOKEN_KW_RETURN, "expected 'return'");
    const BitToken *semicolon_token;

    if (!return_token) {
        return NULL;
    }

    expr = bit_parse_expr(parser);
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

static BitStmt *bit_parse_stmt(BitParser *parser) {
    BitTokenKind expected[2] = {BIT_TOKEN_KW_LET, BIT_TOKEN_KW_RETURN};
    const BitToken *current = bit_parser_current(parser);

    switch (current->kind) {
        case BIT_TOKEN_KW_LET:
            return bit_parse_let_stmt(parser);
        case BIT_TOKEN_KW_RETURN:
            return bit_parse_return_stmt(parser);
        case BIT_TOKEN_INVALID:
            bit_parser_set_error(parser, "invalid token", current, NULL, 0);
            return NULL;
        default:
            bit_parser_set_error(parser, "expected statement", current, expected, 2);
            return NULL;
    }
}

static int bit_parse_block(BitParser *parser, BitBlock *block_out) {
    BitStmt **final_stmts = NULL;
    BitStmt **temp_stmts = NULL;
    size_t stmt_count = 0;
    size_t stmt_capacity = 0;
    const BitToken *left_brace = bit_parser_expect(parser, BIT_TOKEN_LBRACE, "expected '{'");
    const BitToken *right_brace;
    size_t i;

    if (!left_brace) {
        return 0;
    }

    while (bit_parser_current(parser)->kind != BIT_TOKEN_RBRACE && !bit_parser_is_at_end(parser)) {
        BitStmt *stmt;
        BitStmt **new_stmts;

        stmt = bit_parse_stmt(parser);
        if (!stmt) {
            free(temp_stmts);
            return 0;
        }

        if (stmt_count == stmt_capacity) {
            size_t new_capacity = stmt_capacity == 0 ? 4 : stmt_capacity * 2;

            new_stmts = (BitStmt **)realloc(temp_stmts, new_capacity * sizeof(BitStmt *));
            if (!new_stmts) {
                bit_parser_set_error(parser, "out of memory", bit_parser_current(parser), NULL, 0);
                free(temp_stmts);
                return 0;
            }

            temp_stmts = new_stmts;
            stmt_capacity = new_capacity;
        }

        temp_stmts[stmt_count++] = stmt;
    }

    right_brace = bit_parser_expect(parser, BIT_TOKEN_RBRACE, "expected '}'");
    if (!right_brace) {
        free(temp_stmts);
        return 0;
    }

    if (stmt_count > 0) {
        final_stmts = (BitStmt **)bit_parser_alloc(parser, stmt_count * sizeof(BitStmt *));
        if (!final_stmts) {
            bit_parser_set_error(parser, "out of memory", left_brace, NULL, 0);
            free(temp_stmts);
            return 0;
        }

        for (i = 0; i < stmt_count; ++i) {
            final_stmts[i] = temp_stmts[i];
        }
    }

    free(temp_stmts);

    block_out->stmts = final_stmts;
    block_out->stmt_count = stmt_count;
    block_out->span = bit_span_from_range(left_brace, right_brace);
    return 1;
}

static int bit_parse_param_decl(BitParser *parser, BitParamDecl *param_out) {
    const BitToken *name_token = bit_parser_expect(parser, BIT_TOKEN_IDENTIFIER, "expected identifier");

    if (!name_token) {
        return 0;
    }

    if (!bit_parser_expect(parser, BIT_TOKEN_COLON, "expected ':'")) {
        return 0;
    }

    if (!bit_parse_type_ref(parser, &param_out->type)) {
        return 0;
    }

    param_out->name.data = name_token->start;
    param_out->name.length = name_token->length;
    param_out->span = bit_span_from_range(name_token, bit_parser_previous(parser));
    return 1;
}

static int bit_parse_param_list(BitParser *parser, BitParamDecl **params_out, size_t *param_count_out) {
    BitParamDecl *final_params = NULL;
    BitParamDecl *temp_params = NULL;
    size_t param_count = 0;
    size_t param_capacity = 0;
    size_t i;

    while (bit_parser_current(parser)->kind != BIT_TOKEN_RPAREN && !bit_parser_is_at_end(parser)) {
        BitParamDecl *new_params;
        BitParamDecl param;

        if (!bit_parse_param_decl(parser, &param)) {
            free(temp_params);
            return 0;
        }

        if (param_count == param_capacity) {
            size_t new_capacity = param_capacity == 0 ? 4 : param_capacity * 2;

            new_params = (BitParamDecl *)realloc(temp_params, new_capacity * sizeof(BitParamDecl));
            if (!new_params) {
                bit_parser_set_error(parser, "out of memory", bit_parser_current(parser), NULL, 0);
                free(temp_params);
                return 0;
            }

            temp_params = new_params;
            param_capacity = new_capacity;
        }

        temp_params[param_count++] = param;

        if (!bit_parser_match(parser, BIT_TOKEN_COMMA)) {
            break;
        }
    }

    if (param_count > 0) {
        final_params = (BitParamDecl *)bit_parser_alloc(parser, param_count * sizeof(BitParamDecl));
        if (!final_params) {
            bit_parser_set_error(parser, "out of memory", bit_parser_current(parser), NULL, 0);
            free(temp_params);
            return 0;
        }

        for (i = 0; i < param_count; ++i) {
            final_params[i] = temp_params[i];
        }
    }

    free(temp_params);
    *params_out = final_params;
    *param_count_out = param_count;
    return 1;
}

static BitFunctionDecl *bit_parse_function_decl(BitParser *parser) {
    BitFunctionDecl *function;
    const BitToken *fn_token = bit_parser_expect(parser, BIT_TOKEN_KW_FN, "expected 'fn'");
    const BitToken *name_token;
    BitParamDecl *params = NULL;
    size_t param_count = 0;
    BitTypeRef return_type;
    BitBlock body;

    if (!fn_token) {
        return NULL;
    }

    name_token = bit_parser_expect(parser, BIT_TOKEN_IDENTIFIER, "expected identifier");
    if (!name_token) {
        return NULL;
    }

    if (!bit_parser_expect(parser, BIT_TOKEN_LPAREN, "expected '('")) {
        return NULL;
    }

    if (!bit_parse_param_list(parser, &params, &param_count)) {
        return NULL;
    }

    if (!bit_parser_expect(parser, BIT_TOKEN_RPAREN, "expected ')'")) {
        return NULL;
    }

    if (!bit_parser_expect(parser, BIT_TOKEN_ARROW, "expected '->'")) {
        return NULL;
    }

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
    function->params = params;
    function->param_count = param_count;
    function->return_type = return_type;
    function->body = body;
    function->span = bit_span_from_range(fn_token, bit_parser_previous(parser));
    return function;
}

BitParseResult bit_parse_module(const BitToken *tokens, size_t token_count, BitArena *arena) {
    BitParseResult result;
    BitParser parser;
    BitModule *module;
    BitFunctionDecl **functions = NULL;
    BitFunctionDecl **temp_functions = NULL;
    size_t function_count = 0;
    size_t function_capacity = 0;
    const BitToken *eof_token;
    size_t i;

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

    while (bit_parser_current(&parser)->kind != BIT_TOKEN_EOF && !bit_parser_is_at_end(&parser)) {
        BitFunctionDecl *function = bit_parse_function_decl(&parser);
        BitFunctionDecl **new_functions;

        if (!function) {
            free(temp_functions);
            result.diagnostic = parser.diagnostic;
            return result;
        }

        if (function_count == function_capacity) {
            size_t new_capacity = function_capacity == 0 ? 4 : function_capacity * 2;

            new_functions = (BitFunctionDecl **)realloc(temp_functions, new_capacity * sizeof(BitFunctionDecl *));
            if (!new_functions) {
                free(temp_functions);
                bit_parser_set_error(&parser, "out of memory", bit_parser_current(&parser), NULL, 0);
                result.diagnostic = parser.diagnostic;
                return result;
            }

            temp_functions = new_functions;
            function_capacity = new_capacity;
        }

        temp_functions[function_count++] = function;
    }

    eof_token = bit_parser_expect(&parser, BIT_TOKEN_EOF, "expected end of file");
    if (!eof_token) {
        free(temp_functions);
        result.diagnostic = parser.diagnostic;
        return result;
    }

    if (function_count > 0) {
        functions = (BitFunctionDecl **)bit_parser_alloc(&parser, function_count * sizeof(BitFunctionDecl *));
        if (!functions) {
            free(temp_functions);
            bit_parser_set_error(&parser, "out of memory", eof_token, NULL, 0);
            result.diagnostic = parser.diagnostic;
            return result;
        }

        for (i = 0; i < function_count; ++i) {
            functions[i] = temp_functions[i];
        }
    }

    module = (BitModule *)bit_parser_alloc(&parser, sizeof(BitModule));
    if (!module) {
        free(temp_functions);
        bit_parser_set_error(&parser, "out of memory", eof_token, NULL, 0);
        result.diagnostic = parser.diagnostic;
        return result;
    }

    module->functions = functions;
    module->function_count = function_count;
    module->span = function_count > 0
        ? bit_span_from_spans(functions[0]->span, functions[function_count - 1]->span)
        : bit_span_from_token(eof_token);
    free(temp_functions);

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
