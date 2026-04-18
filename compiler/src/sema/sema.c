#include "bit/sema.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

typedef struct BitSemaContext {
    BitSemaDiagnostic diagnostic;
    int failed;
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
    ctx->failed = 1;
    ctx->diagnostic.message = message;
    ctx->diagnostic.span = span;
    return 0;
}

static int bit_string_view_equals(BitStringView value, const char *text) {
    size_t text_length = strlen(text);

    return value.length == text_length && strncmp(value.data, text, value.length) == 0;
}

static int bit_check_expr(BitSemaContext *ctx, const BitExpr *expr) {
    if (!expr) {
        return bit_sema_fail(ctx, "expression is required", bit_empty_span());
    }

    switch (expr->kind) {
        case BIT_EXPR_INTEGER:
            if (expr->as.integer.value > INT32_MAX) {
                return bit_sema_fail(ctx, "integer literal out of range for i32", expr->span);
            }

            return 1;
    }

    return bit_sema_fail(ctx, "unsupported expression", expr->span);
}

static int bit_check_stmt(BitSemaContext *ctx, const BitStmt *stmt) {
    if (!stmt) {
        return bit_sema_fail(ctx, "statement is required", bit_empty_span());
    }

    switch (stmt->kind) {
        case BIT_STMT_RETURN:
            if (!stmt->as.ret.expr) {
                return bit_sema_fail(ctx, "return statement requires an expression", stmt->span);
            }

            return bit_check_expr(ctx, stmt->as.ret.expr);
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
        return bit_sema_fail(ctx, "function body must contain a return statement", function->body.span);
    }

    for (i = 0; i < function->body.stmt_count; ++i) {
        if (!bit_check_stmt(ctx, function->body.stmts[i])) {
            return 0;
        }
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

    ctx.diagnostic.message = NULL;
    ctx.diagnostic.span = bit_empty_span();
    ctx.failed = 0;

    if (!bit_check_module(&ctx, module)) {
        return bit_sema_error_result(
            ctx.diagnostic.message ? ctx.diagnostic.message : "semantic analysis failed",
            ctx.diagnostic.span
        );
    }

    {
        BitSemaResult result;
        result.status = BIT_SEMA_OK;
        result.diagnostic.message = NULL;
        result.diagnostic.span = bit_empty_span();
        return result;
    }
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
