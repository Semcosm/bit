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
        case BIT_TYPE_BOOL:
            return "bool";
    }

    return "unknown";
}

static const char *bit_binary_op_name(BitBinaryOpKind op) {
    switch (op) {
        case BIT_BINARY_OP_ADD:
            return "+";
        case BIT_BINARY_OP_SUB:
            return "-";
        case BIT_BINARY_OP_MUL:
            return "*";
        case BIT_BINARY_OP_DIV:
            return "/";
        case BIT_BINARY_OP_EQUAL:
            return "==";
        case BIT_BINARY_OP_NOT_EQUAL:
            return "!=";
        case BIT_BINARY_OP_LESS:
            return "<";
        case BIT_BINARY_OP_LESS_EQUAL:
            return "<=";
        case BIT_BINARY_OP_GREATER:
            return ">";
        case BIT_BINARY_OP_GREATER_EQUAL:
            return ">=";
    }

    return "?";
}

static const char *bit_unary_op_name(BitUnaryOpKind op) {
    switch (op) {
        case BIT_UNARY_OP_NEG:
            return "-";
    }

    return "?";
}

static void bit_ast_dump_param(FILE *stream, const BitParamDecl *param, int indent) {
    bit_ast_print_indent(stream, indent);
    fprintf(
        stream,
        "(param name=\"%.*s\" type=%s)",
        (int)param->name.length,
        param->name.data,
        bit_type_kind_name(param->type.kind)
    );
}

static void bit_ast_dump_block(FILE *stream, const BitBlock *block, int indent);

static void bit_ast_dump_expr(FILE *stream, const BitExpr *expr, int indent) {
    bit_ast_print_indent(stream, indent);

    switch (expr->kind) {
        case BIT_EXPR_INTEGER:
            fprintf(stream, "(int %" PRIu64 ")", expr->as.integer.value);
            return;
        case BIT_EXPR_BOOL:
            fprintf(stream, "(bool %s)", expr->as.boolean.value ? "true" : "false");
            return;
        case BIT_EXPR_IDENTIFIER:
            fprintf(
                stream,
                "(name \"%.*s\")",
                (int)expr->as.name.name.length,
                expr->as.name.name.data
            );
            return;
        case BIT_EXPR_CALL: {
            size_t i;

            fprintf(
                stream,
                "(call callee=\"%.*s\"",
                (int)expr->as.call.callee.length,
                expr->as.call.callee.data
            );

            if (expr->as.call.arg_count == 0) {
                fputc(')', stream);
                return;
            }

            fputc('\n', stream);
            for (i = 0; i < expr->as.call.arg_count; ++i) {
                bit_ast_dump_expr(stream, expr->as.call.args[i], indent + 1);
                if (i + 1 < expr->as.call.arg_count) {
                    fputc('\n', stream);
                }
            }
            fputc(')', stream);
            return;
        }
        case BIT_EXPR_UNARY:
            fprintf(stream, "(unary op=\"%s\"\n", bit_unary_op_name(expr->as.unary.op));
            bit_ast_dump_expr(stream, expr->as.unary.operand, indent + 1);
            fputc(')', stream);
            return;
        case BIT_EXPR_BINARY:
            fprintf(stream, "(binary op=\"%s\"\n", bit_binary_op_name(expr->as.binary.op));
            bit_ast_dump_expr(stream, expr->as.binary.left, indent + 1);
            fputc('\n', stream);
            bit_ast_dump_expr(stream, expr->as.binary.right, indent + 1);
            fputc(')', stream);
            return;
    }
}

static void bit_ast_dump_stmt(FILE *stream, const BitStmt *stmt, int indent) {
    switch (stmt->kind) {
        case BIT_STMT_LET:
            bit_ast_print_indent(stream, indent);
            fprintf(
                stream,
                "(let name=\"%.*s\" type=%s\n",
                (int)stmt->as.let.name.length,
                stmt->as.let.name.data,
                bit_type_kind_name(stmt->as.let.type.kind)
            );
            bit_ast_dump_expr(stream, stmt->as.let.initializer, indent + 1);
            fputc(')', stream);
            return;
        case BIT_STMT_RETURN:
            bit_ast_print_indent(stream, indent);
            fputs("(return\n", stream);
            bit_ast_dump_expr(stream, stmt->as.ret.expr, indent + 1);
            fputc(')', stream);
            return;
        case BIT_STMT_IF:
            bit_ast_print_indent(stream, indent);
            fputs("(if\n", stream);
            bit_ast_dump_expr(stream, stmt->as.if_stmt.condition, indent + 1);
            fputc('\n', stream);
            bit_ast_dump_block(stream, &stmt->as.if_stmt.then_block, indent + 1);
            fputc('\n', stream);
            bit_ast_dump_block(stream, &stmt->as.if_stmt.else_block, indent + 1);
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
    size_t i;

    bit_ast_print_indent(stream, indent);
    fprintf(
        stream,
        "(fn name=\"%.*s\" ret=%s\n",
        (int)function->name.length,
        function->name.data,
        bit_type_kind_name(function->return_type.kind)
    );

    bit_ast_print_indent(stream, indent + 1);
    fputs("(params", stream);
    if (function->param_count == 0) {
        fputs(")\n", stream);
    } else {
        fputc('\n', stream);
        for (i = 0; i < function->param_count; ++i) {
            bit_ast_dump_param(stream, &function->params[i], indent + 2);
            if (i + 1 < function->param_count) {
                fputc('\n', stream);
            }
        }
        fputs(")\n", stream);
    }

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
