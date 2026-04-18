#include "bit/irgen.h"

#include <stdint.h>
#include <string.h>

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>

typedef struct BitIrgenContext {
    LLVMContextRef llctx;
    LLVMModuleRef llmod;
    LLVMBuilderRef builder;

    LLVMTypeRef i32_type;
    LLVMValueRef current_function;

    BitIrgenDiagnostic diagnostic;
    int failed;
} BitIrgenContext;

static BitSourceSpan bit_empty_span(void) {
    BitSourceSpan span;

    span.start = NULL;
    span.length = 0;
    span.line = 0;
    span.column = 0;
    return span;
}

static BitIrgenResult bit_irgen_error_result(const char *message, BitSourceSpan span) {
    BitIrgenResult result;

    result.status = BIT_IRGEN_ERROR;
    result.diagnostic.message = message;
    result.diagnostic.span = span;
    return result;
}

static int bit_irgen_fail(BitIrgenContext *ctx, const char *message, BitSourceSpan span) {
    ctx->failed = 1;
    ctx->diagnostic.message = message;
    ctx->diagnostic.span = span;
    return 0;
}

static int bit_irgen_init(BitIrgenContext *ctx, const BitIrgenOptions *options) {
    const char *module_name = "bit_module";
    const char *source_name = NULL;

    if (options && options->module_name) {
        module_name = options->module_name;
    }

    if (options && options->source_name) {
        source_name = options->source_name;
    }

    ctx->llctx = LLVMContextCreate();
    if (!ctx->llctx) {
        return bit_irgen_fail(ctx, "failed to create LLVM context", bit_empty_span());
    }

    ctx->llmod = LLVMModuleCreateWithNameInContext(module_name, ctx->llctx);
    if (!ctx->llmod) {
        return bit_irgen_fail(ctx, "failed to create LLVM module", bit_empty_span());
    }

    if (source_name) {
        LLVMSetSourceFileName(ctx->llmod, source_name, strlen(source_name));
    }

    ctx->builder = LLVMCreateBuilderInContext(ctx->llctx);
    if (!ctx->builder) {
        return bit_irgen_fail(ctx, "failed to create LLVM builder", bit_empty_span());
    }

    ctx->i32_type = LLVMInt32TypeInContext(ctx->llctx);
    ctx->current_function = NULL;
    return 1;
}

static void bit_irgen_dispose(BitIrgenContext *ctx) {
    if (ctx->builder) {
        LLVMDisposeBuilder(ctx->builder);
    }

    if (ctx->llmod) {
        LLVMDisposeModule(ctx->llmod);
    }

    if (ctx->llctx) {
        LLVMContextDispose(ctx->llctx);
    }
}

static LLVMTypeRef bit_lower_type(BitIrgenContext *ctx, const BitTypeRef *type) {
    switch (type->kind) {
        case BIT_TYPE_I32:
            return ctx->i32_type;
    }

    bit_irgen_fail(ctx, "unsupported type", type->span);
    return NULL;
}

static LLVMValueRef bit_lower_expr(BitIrgenContext *ctx, const BitExpr *expr) {
    switch (expr->kind) {
        case BIT_EXPR_INTEGER:
            if (expr->as.integer.value > INT32_MAX) {
                bit_irgen_fail(ctx, "integer literal out of range for i32", expr->span);
                return NULL;
            }

            return LLVMConstInt(ctx->i32_type, expr->as.integer.value, 0);
    }

    bit_irgen_fail(ctx, "unsupported expression", expr->span);
    return NULL;
}

static int bit_lower_stmt(BitIrgenContext *ctx, const BitStmt *stmt) {
    switch (stmt->kind) {
        case BIT_STMT_RETURN: {
            LLVMValueRef value;

            if (!stmt->as.ret.expr) {
                return bit_irgen_fail(ctx, "return statement requires an expression", stmt->span);
            }

            value = bit_lower_expr(ctx, stmt->as.ret.expr);
            if (!value) {
                return 0;
            }

            LLVMBuildRet(ctx->builder, value);
            return 1;
        }
    }

    return bit_irgen_fail(ctx, "unsupported statement", stmt->span);
}

static int bit_lower_block(BitIrgenContext *ctx, const BitBlock *block) {
    size_t i;

    for (i = 0; i < block->stmt_count; ++i) {
        if (!bit_lower_stmt(ctx, block->stmts[i])) {
            return 0;
        }
    }

    return 1;
}

static int bit_lower_function(BitIrgenContext *ctx, const BitFunctionDecl *function) {
    LLVMTypeRef return_type;
    LLVMTypeRef fn_type;
    LLVMValueRef fn_value;
    LLVMBasicBlockRef entry;
    LLVMBasicBlockRef current_block;

    return_type = bit_lower_type(ctx, &function->return_type);
    if (!return_type) {
        return 0;
    }

    fn_type = LLVMFunctionType(return_type, NULL, 0, 0);
    fn_value = LLVMAddFunction(ctx->llmod, "", fn_type);
    if (!fn_value) {
        return bit_irgen_fail(ctx, "failed to create function", function->span);
    }

    LLVMSetValueName2(fn_value, function->name.data, function->name.length);
    ctx->current_function = fn_value;
    entry = LLVMAppendBasicBlockInContext(ctx->llctx, fn_value, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);

    if (!bit_lower_block(ctx, &function->body)) {
        return 0;
    }

    current_block = LLVMGetInsertBlock(ctx->builder);
    if (!current_block || !LLVMGetBasicBlockTerminator(current_block)) {
        return bit_irgen_fail(ctx, "function body did not produce a terminator", function->body.span);
    }

    return 1;
}

static int bit_lower_module(BitIrgenContext *ctx, const BitModule *module) {
    size_t i;

    if (!module || module->function_count == 0) {
        return bit_irgen_fail(ctx, "module has no functions", bit_empty_span());
    }

    for (i = 0; i < module->function_count; ++i) {
        if (!bit_lower_function(ctx, module->functions[i])) {
            return 0;
        }
    }

    return 1;
}

static int bit_verify_module(BitIrgenContext *ctx) {
    char *error = NULL;

    if (LLVMVerifyModule(ctx->llmod, LLVMReturnStatusAction, &error)) {
        if (error) {
            LLVMDisposeMessage(error);
        }

        return bit_irgen_fail(ctx, "invalid LLVM module", bit_empty_span());
    }

    return 1;
}

static int bit_write_module_to_file(BitIrgenContext *ctx, const char *output_path) {
    char *error = NULL;

    if (LLVMPrintModuleToFile(ctx->llmod, output_path, &error) != 0) {
        if (error) {
            LLVMDisposeMessage(error);
        }

        return bit_irgen_fail(ctx, "failed to write LLVM IR file", bit_empty_span());
    }

    return 1;
}

BitIrgenResult bit_emit_llvm_ir_file(
    const BitModule *module,
    const BitIrgenOptions *options,
    const char *output_path
) {
    BitIrgenContext ctx;
    BitIrgenResult result;
    int verify_module = 1;

    if (!module) {
        return bit_irgen_error_result("module is required", bit_empty_span());
    }

    if (!output_path) {
        return bit_irgen_error_result("output path is required", bit_empty_span());
    }

    ctx.llctx = NULL;
    ctx.llmod = NULL;
    ctx.builder = NULL;
    ctx.i32_type = NULL;
    ctx.current_function = NULL;
    ctx.diagnostic.message = NULL;
    ctx.diagnostic.span = bit_empty_span();
    ctx.failed = 0;

    if (options) {
        verify_module = options->verify_module != 0;
    }

    if (!bit_irgen_init(&ctx, options)) {
        result = bit_irgen_error_result(
            ctx.diagnostic.message ? ctx.diagnostic.message : "failed to initialize IR generation",
            ctx.diagnostic.span
        );
        bit_irgen_dispose(&ctx);
        return result;
    }

    if (!bit_lower_module(&ctx, module)) {
        goto done;
    }

    if (verify_module && !bit_verify_module(&ctx)) {
        goto done;
    }

    if (!bit_write_module_to_file(&ctx, output_path)) {
        goto done;
    }

done:
    if (ctx.failed) {
        result = bit_irgen_error_result(
            ctx.diagnostic.message ? ctx.diagnostic.message : "IR generation failed",
            ctx.diagnostic.span
        );
    } else {
        result.status = BIT_IRGEN_OK;
        result.diagnostic.message = NULL;
        result.diagnostic.span = bit_empty_span();
    }

    bit_irgen_dispose(&ctx);
    return result;
}

void bit_print_irgen_diagnostic(FILE *stream, const BitIrgenDiagnostic *diagnostic) {
    fprintf(
        stream,
        "irgen error: %s at %zu:%zu\n",
        diagnostic->message ? diagnostic->message : "unknown error",
        diagnostic->span.line,
        diagnostic->span.column
    );
}
