#include "bit/sema.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct BitSemaLocalSymbol {
    BitStringView name;
    BitTypeKind type;
    BitSourceSpan span;
} BitSemaLocalSymbol;

typedef struct BitSemaFunctionSymbol {
    BitStringView name;
    const BitFunctionDecl *decl;
    BitSourceSpan span;
} BitSemaFunctionSymbol;

typedef struct BitSemaContext {
    BitSemaDiagnostic diagnostic;
    BitSemaFunctionSymbol *functions;
    size_t function_count;
    size_t function_capacity;
    BitSemaLocalSymbol *locals;
    size_t local_count;
    size_t local_capacity;
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

static int bit_string_view_equals(BitStringView left, BitStringView right) {
    return left.length == right.length && strncmp(left.data, right.data, left.length) == 0;
}

static int bit_string_view_equals_text(BitStringView value, const char *text) {
    size_t text_length = strlen(text);

    return value.length == text_length && strncmp(value.data, text, value.length) == 0;
}

static const BitSemaLocalSymbol *bit_sema_find_local(const BitSemaContext *ctx, BitStringView name) {
    size_t i = ctx->local_count;

    while (i > 0) {
        const BitSemaLocalSymbol *local = &ctx->locals[i - 1];

        if (bit_string_view_equals(local->name, name)) {
            return local;
        }

        --i;
    }

    return NULL;
}

static const BitSemaFunctionSymbol *bit_sema_find_function(const BitSemaContext *ctx, BitStringView name) {
    size_t i;

    for (i = 0; i < ctx->function_count; ++i) {
        if (bit_string_view_equals(ctx->functions[i].name, name)) {
            return &ctx->functions[i];
        }
    }

    return NULL;
}

static int bit_sema_bind_local(BitSemaContext *ctx, BitStringView name, BitTypeKind type, BitSourceSpan span) {
    BitSemaLocalSymbol *new_locals;

    if (bit_sema_find_local(ctx, name)) {
        return bit_sema_fail(ctx, "duplicate local binding", span);
    }

    if (ctx->local_count == ctx->local_capacity) {
        size_t new_capacity = ctx->local_capacity == 0 ? 8 : ctx->local_capacity * 2;

        new_locals = (BitSemaLocalSymbol *)realloc(ctx->locals, new_capacity * sizeof(BitSemaLocalSymbol));
        if (!new_locals) {
            return bit_sema_fail(ctx, "out of memory", span);
        }

        ctx->locals = new_locals;
        ctx->local_capacity = new_capacity;
    }

    ctx->locals[ctx->local_count].name = name;
    ctx->locals[ctx->local_count].type = type;
    ctx->locals[ctx->local_count].span = span;
    ctx->local_count += 1;
    return 1;
}

static int bit_sema_bind_function(BitSemaContext *ctx, const BitFunctionDecl *function) {
    BitSemaFunctionSymbol *new_functions;

    if (bit_sema_find_function(ctx, function->name)) {
        return bit_sema_fail(ctx, "duplicate function declaration", function->span);
    }

    if (ctx->function_count == ctx->function_capacity) {
        size_t new_capacity = ctx->function_capacity == 0 ? 8 : ctx->function_capacity * 2;

        new_functions = (BitSemaFunctionSymbol *)realloc(
            ctx->functions,
            new_capacity * sizeof(BitSemaFunctionSymbol)
        );
        if (!new_functions) {
            return bit_sema_fail(ctx, "out of memory", function->span);
        }

        ctx->functions = new_functions;
        ctx->function_capacity = new_capacity;
    }

    ctx->functions[ctx->function_count].name = function->name;
    ctx->functions[ctx->function_count].decl = function;
    ctx->functions[ctx->function_count].span = function->span;
    ctx->function_count += 1;
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
            const BitSemaLocalSymbol *local = bit_sema_find_local(ctx, expr->as.name.name);

            if (!local) {
                return bit_sema_fail(ctx, "unknown identifier", expr->span);
            }

            *type_out = local->type;
            return 1;
        }
        case BIT_EXPR_CALL: {
            const BitSemaFunctionSymbol *function = bit_sema_find_function(ctx, expr->as.call.callee);
            size_t i;

            if (!function) {
                return bit_sema_fail(ctx, "unknown function", expr->span);
            }

            if (expr->as.call.arg_count != function->decl->param_count) {
                return bit_sema_fail(ctx, "function argument count mismatch", expr->span);
            }

            for (i = 0; i < expr->as.call.arg_count; ++i) {
                BitTypeKind arg_type;

                if (!bit_check_expr(ctx, expr->as.call.args[i], &arg_type)) {
                    return 0;
                }

                if (arg_type != function->decl->params[i].type.kind) {
                    return bit_sema_fail(ctx, "function argument type mismatch", expr->as.call.args[i]->span);
                }
            }

            *type_out = function->decl->return_type.kind;
            return 1;
        }
        case BIT_EXPR_UNARY: {
            BitTypeKind operand_type;
            const BitExpr *operand = expr->as.unary.operand;
            uint64_t max_negated_i32 = (uint64_t)INT32_MAX + 1ULL;

            if (!operand) {
                return bit_sema_fail(ctx, "unary expression requires an operand", expr->span);
            }

            if (expr->as.unary.op == BIT_UNARY_OP_NEG && operand->kind == BIT_EXPR_INTEGER) {
                if (operand->as.integer.value > max_negated_i32) {
                    return bit_sema_fail(ctx, "integer literal out of range for i32", operand->span);
                }

                *type_out = BIT_TYPE_I32;
                return 1;
            }

            if (!bit_check_expr(ctx, operand, &operand_type)) {
                return 0;
            }

            if (operand_type != BIT_TYPE_I32) {
                return bit_sema_fail(ctx, "unary operand must be i32", expr->span);
            }

            *type_out = BIT_TYPE_I32;
            return 1;
        }
        case BIT_EXPR_BINARY: {
            BitTypeKind left_type;
            BitTypeKind right_type;

            if (!expr->as.binary.left || !expr->as.binary.right) {
                return bit_sema_fail(ctx, "binary expression requires both operands", expr->span);
            }

            if (!bit_check_expr(ctx, expr->as.binary.left, &left_type)) {
                return 0;
            }

            if (!bit_check_expr(ctx, expr->as.binary.right, &right_type)) {
                return 0;
            }

            if (left_type != BIT_TYPE_I32 || right_type != BIT_TYPE_I32) {
                return bit_sema_fail(ctx, "binary operands must be i32", expr->span);
            }

            *type_out = BIT_TYPE_I32;
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

            return bit_sema_bind_local(
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

    if (function->return_type.kind != BIT_TYPE_I32) {
        return bit_sema_fail(ctx, "functions must return i32", function->return_type.span);
    }

    if (bit_string_view_equals_text(function->name, "main") && function->param_count != 0) {
        return bit_sema_fail(ctx, "main must not take parameters", function->span);
    }

    if (function->body.stmt_count == 0) {
        return bit_sema_fail(ctx, "function body must contain statements", function->body.span);
    }

    ctx->local_count = 0;
    ctx->current_return_type = function->return_type.kind;

    for (i = 0; i < function->param_count; ++i) {
        if (function->params[i].type.kind != BIT_TYPE_I32) {
            return bit_sema_fail(ctx, "function parameters must be i32", function->params[i].span);
        }

        if (!bit_sema_bind_local(
                ctx,
                function->params[i].name,
                function->params[i].type.kind,
                function->params[i].span)) {
            return 0;
        }
    }

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

static int bit_bind_module_functions(BitSemaContext *ctx, const BitModule *module) {
    size_t i;

    for (i = 0; i < module->function_count; ++i) {
        if (!bit_sema_bind_function(ctx, module->functions[i])) {
            return 0;
        }
    }

    return 1;
}

static int bit_check_module(BitSemaContext *ctx, const BitModule *module) {
    size_t i;

    if (!module) {
        return bit_sema_fail(ctx, "module is required", bit_empty_span());
    }

    if (module->function_count == 0) {
        return bit_sema_fail(ctx, "module has no functions", module->span);
    }

    if (!bit_bind_module_functions(ctx, module)) {
        return 0;
    }

    if (!bit_sema_find_function(ctx, (BitStringView){"main", 4})) {
        return bit_sema_fail(ctx, "module must define function 'main'", module->span);
    }

    for (i = 0; i < module->function_count; ++i) {
        if (!bit_check_function(ctx, module->functions[i])) {
            return 0;
        }
    }

    return 1;
}

BitSemaResult bit_analyze_module(const BitModule *module) {
    BitSemaContext ctx;
    BitSemaResult result;

    ctx.diagnostic.message = NULL;
    ctx.diagnostic.span = bit_empty_span();
    ctx.functions = NULL;
    ctx.function_count = 0;
    ctx.function_capacity = 0;
    ctx.locals = NULL;
    ctx.local_count = 0;
    ctx.local_capacity = 0;
    ctx.current_return_type = BIT_TYPE_I32;

    if (!bit_check_module(&ctx, module)) {
        result = bit_sema_error_result(
            ctx.diagnostic.message ? ctx.diagnostic.message : "semantic analysis failed",
            ctx.diagnostic.span
        );
        free(ctx.functions);
        free(ctx.locals);
        return result;
    }

    result.status = BIT_SEMA_OK;
    result.diagnostic.message = NULL;
    result.diagnostic.span = bit_empty_span();
    free(ctx.functions);
    free(ctx.locals);
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
