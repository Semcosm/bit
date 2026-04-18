#include "bit/sema.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct BitSemaSymbol {
    BitStringView name;
    BitTypeKind type;
    BitSourceSpan span;
} BitSemaSymbol;

typedef struct BitSemaContext {
    BitSemaDiagnostic diagnostic;
    BitSemaSymbol *symbols;
    size_t symbol_count;
    size_t symbol_capacity;
    BitTypeKind current_return_type;
} BitSemaContext;

static BitSourceSpan bit_empty_span(void) {
    BitSourceSpan span;

    span.start = NULL;
    span.length = 0;
    span.line = 0;
    span.column = 0;
    return span;
}

static BitSemaResult bit_sema_error_result(const char *message, BitSourceSpan span) {
    BitSemaResult result;

    result.status = BIT_SEMA_ERROR;
    result.diagnostic.message = message;
    result.diagnostic.span = span;
    return result;
}

static int bit_sema_fail(BitSemaContext *ctx, const char *message, BitSourceSpan span) {
    ctx->diagnostic.message = message;
    ctx->diagnostic.span = span;
    return 0;
}

static int bit_string_view_equals(BitStringView value, const char *text) {
    size_t text_length = strlen(text);

    return value.length == text_length && strncmp(value.data, text, value.length) == 0;
}

static const BitSemaSymbol *bit_sema_find_symbol(const BitSemaContext *ctx, BitStringView name) {
    size_t i = ctx->symbol_count;

    while (i > 0) {
        const BitSemaSymbol *symbol = &ctx->symbols[i - 1];

        if (symbol->name.length == name.length &&
            strncmp(symbol->name.data, name.data, name.length) == 0) {
            return symbol;
        }

        --i;
    }

    return NULL;
}

static int bit_sema_bind_symbol(BitSemaContext *ctx, BitStringView name, BitTypeKind type, BitSourceSpan span) {
    BitSemaSymbol *new_symbols;

    if (bit_sema_find_symbol(ctx, name)) {
        return bit_sema_fail(ctx, "duplicate local binding", span);
    }

    if (ctx->symbol_count == ctx->symbol_capacity) {
        size_t new_capacity = ctx->symbol_capacity == 0 ? 8 : ctx->symbol_capacity * 2;

        new_symbols = (BitSemaSymbol *)realloc(ctx->symbols, new_capacity * sizeof(BitSemaSymbol));
        if (!new_symbols) {
            return bit_sema_fail(ctx, "out of memory", span);
        }

        ctx->symbols = new_symbols;
        ctx->symbol_capacity = new_capacity;
    }

    ctx->symbols[ctx->symbol_count].name = name;
    ctx->symbols[ctx->symbol_count].type = type;
    ctx->symbols[ctx->symbol_count].span = span;
    ctx->symbol_count += 1;
    return 1;
}

static int bit_check_expr(BitSemaContext *ctx, const BitExpr *expr, BitTypeKind *type_out) {
    if (!expr) {
        return bit_sema_fail(ctx, "expression is required", bit_empty_span());
    }

    switch (expr->kind) {
        case BIT_EXPR_INTEGER:
            if (expr->as.integer.value > INT32_MAX) {
                return bit_sema_fail(ctx, "integer literal out of range for i32", expr->span);
            }

            *type_out = BIT_TYPE_I32;
            return 1;
        case BIT_EXPR_IDENTIFIER: {
            const BitSemaSymbol *symbol = bit_sema_find_symbol(ctx, expr->as.name.name);

            if (!symbol) {
                return bit_sema_fail(ctx, "unknown identifier", expr->span);
            }

            *type_out = symbol->type;
            return 1;
        }
    }

    return bit_sema_fail(ctx, "unsupported expression", expr->span);
}

static int bit_check_stmt(BitSemaContext *ctx, const BitStmt *stmt) {
    BitTypeKind expr_type;

    if (!stmt) {
        return bit_sema_fail(ctx, "statement is required", bit_empty_span());
    }

    switch (stmt->kind) {
        case BIT_STMT_LET:
            if (!stmt->as.let.initializer) {
                return bit_sema_fail(ctx, "let statement requires an initializer", stmt->span);
            }

            if (!bit_check_expr(ctx, stmt->as.let.initializer, &expr_type)) {
                return 0;
            }

            if (stmt->as.let.type.kind != expr_type) {
                return bit_sema_fail(ctx, "initializer type does not match local binding type", stmt->span);
            }

            return bit_sema_bind_symbol(
                ctx,
                stmt->as.let.name,
                stmt->as.let.type.kind,
                stmt->as.let.span
            );
        case BIT_STMT_RETURN:
            if (!stmt->as.ret.expr) {
                return bit_sema_fail(ctx, "return statement requires an expression", stmt->span);
            }

            if (!bit_check_expr(ctx, stmt->as.ret.expr, &expr_type)) {
                return 0;
            }

            if (expr_type != ctx->current_return_type) {
                return bit_sema_fail(ctx, "return type does not match function return type", stmt->span);
            }

            return 1;
    }

    return bit_sema_fail(ctx, "unsupported statement", stmt->span);
}

static int bit_check_function(BitSemaContext *ctx, const BitFunctionDecl *function) {
    size_t i;

    if (!function) {
        return bit_sema_fail(ctx, "function is required", bit_empty_span());
    }

    if (!bit_string_view_equals(function->name, "main")) {
        return bit_sema_fail(ctx, "expected function name 'main'", function->span);
    }

    if (function->return_type.kind != BIT_TYPE_I32) {
        return bit_sema_fail(ctx, "main must return i32", function->return_type.span);
    }

    if (function->body.stmt_count == 0) {
        return bit_sema_fail(ctx, "function body must contain statements", function->body.span);
    }

    ctx->symbol_count = 0;
    ctx->current_return_type = function->return_type.kind;

    for (i = 0; i < function->body.stmt_count; ++i) {
        if (function->body.stmts[i]->kind == BIT_STMT_RETURN && i + 1 != function->body.stmt_count) {
            return bit_sema_fail(ctx, "return must be the final statement in a block", function->body.stmts[i]->span);
        }

        if (!bit_check_stmt(ctx, function->body.stmts[i])) {
            return 0;
        }
    }

    if (function->body.stmts[function->body.stmt_count - 1]->kind != BIT_STMT_RETURN) {
        return bit_sema_fail(ctx, "function body must end with return", function->body.span);
    }

    return 1;
}

static int bit_check_module(BitSemaContext *ctx, const BitModule *module) {
    if (!module) {
        return bit_sema_fail(ctx, "module is required", bit_empty_span());
    }

    if (module->function_count != 1) {
        return bit_sema_fail(ctx, "module must contain exactly one function", module->span);
    }

    return bit_check_function(ctx, module->functions[0]);
}

BitSemaResult bit_analyze_module(const BitModule *module) {
    BitSemaContext ctx;
    BitSemaResult result;

    ctx.diagnostic.message = NULL;
    ctx.diagnostic.span = bit_empty_span();
    ctx.symbols = NULL;
    ctx.symbol_count = 0;
    ctx.symbol_capacity = 0;
    ctx.current_return_type = BIT_TYPE_I32;

    if (!bit_check_module(&ctx, module)) {
        result = bit_sema_error_result(
            ctx.diagnostic.message ? ctx.diagnostic.message : "semantic analysis failed",
            ctx.diagnostic.span
        );
        free(ctx.symbols);
        return result;
    }

    result.status = BIT_SEMA_OK;
    result.diagnostic.message = NULL;
    result.diagnostic.span = bit_empty_span();
    free(ctx.symbols);
    return result;
}

void bit_print_sema_diagnostic(FILE *stream, const BitSemaDiagnostic *diagnostic) {
    fprintf(
        stream,
        "sema error: %s at %zu:%zu\n",
        diagnostic->message ? diagnostic->message : "unknown error",
        diagnostic->span.line,
        diagnostic->span.column
    );
}
