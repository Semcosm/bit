#include "bit/ast.h"

#include <inttypes.h>

static void bit_ast_print_indent(FILE *stream, int indent) {
    int i;

    for (i = 0; i < indent; ++i) {
        fputs("  ", stream);
    }
}

static const char *bit_type_kind_name(BitTypeKind kind) {
    switch (kind) {
        case BIT_TYPE_I32:
            return "i32";
    }

    return "unknown";
}

static void bit_ast_dump_expr(FILE *stream, const BitExpr *expr, int indent) {
    bit_ast_print_indent(stream, indent);

    switch (expr->kind) {
        case BIT_EXPR_INTEGER:
            fprintf(stream, "(int %" PRIu64 ")", expr->as.integer.value);
            return;
    }
}

static void bit_ast_dump_stmt(FILE *stream, const BitStmt *stmt, int indent) {
    switch (stmt->kind) {
        case BIT_STMT_RETURN:
            bit_ast_print_indent(stream, indent);
            fputs("(return\n", stream);
            bit_ast_dump_expr(stream, stmt->as.ret.expr, indent + 1);
            fputc(')', stream);
            return;
    }
}

static void bit_ast_dump_block(FILE *stream, const BitBlock *block, int indent) {
    size_t i;

    bit_ast_print_indent(stream, indent);
    fputs("(block", stream);

    if (block->stmt_count == 0) {
        fputc(')', stream);
        return;
    }

    fputc('\n', stream);

    for (i = 0; i < block->stmt_count; ++i) {
        bit_ast_dump_stmt(stream, block->stmts[i], indent + 1);
        if (i + 1 < block->stmt_count) {
            fputc('\n', stream);
        }
    }

    fputc(')', stream);
}

static void bit_ast_dump_function(FILE *stream, const BitFunctionDecl *function, int indent) {
    bit_ast_print_indent(stream, indent);
    fprintf(
        stream,
        "(fn name=\"%.*s\" ret=%s\n",
        (int)function->name.length,
        function->name.data,
        bit_type_kind_name(function->return_type.kind)
    );
    bit_ast_dump_block(stream, &function->body, indent + 1);
    fputc(')', stream);
}

void bit_ast_dump_module(FILE *stream, const BitModule *module) {
    size_t i;

    fputs("(module", stream);

    if (!module || module->function_count == 0) {
        fputc(')', stream);
        return;
    }

    fputc('\n', stream);

    for (i = 0; i < module->function_count; ++i) {
        bit_ast_dump_function(stream, module->functions[i], 1);
        if (i + 1 < module->function_count) {
            fputc('\n', stream);
        }
    }

    fputc(')', stream);
}
