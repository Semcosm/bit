#include "bit/irgen.h"

#include <stdlib.h>
#include <string.h>

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>

typedef struct BitIrgenLocal {
    BitStringView name;
    LLVMTypeRef type;
    LLVMValueRef storage;
} BitIrgenLocal;

typedef struct BitIrgenFunction {
    BitStringView name;
    LLVMTypeRef type;
    LLVMValueRef value;
    const BitFunctionDecl *decl;
} BitIrgenFunction;

typedef struct BitIrgenContext {
    LLVMContextRef llctx;
    LLVMModuleRef llmod;
    LLVMBuilderRef builder;

    LLVMTypeRef i32_type;
    LLVMTypeRef bool_type;

    BitIrgenDiagnostic diagnostic;
    BitIrgenFunction *functions;
    size_t function_count;
    size_t function_capacity;
    BitIrgenLocal *locals;
    size_t local_count;
    size_t local_capacity;
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

static int bit_string_view_equals(BitStringView left, BitStringView right) {
    return left.length == right.length && strncmp(left.data, right.data, left.length) == 0;
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
    ctx->bool_type = LLVMInt1TypeInContext(ctx->llctx);
    ctx->functions = NULL;
    ctx->function_count = 0;
    ctx->function_capacity = 0;
    ctx->locals = NULL;
    ctx->local_count = 0;
    ctx->local_capacity = 0;
    return 1;
}

static void bit_irgen_dispose(BitIrgenContext *ctx) {
    free(ctx->functions);
    free(ctx->locals);

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

static LLVMValueRef bit_create_entry_alloca(BitIrgenContext *ctx, LLVMTypeRef type) {
    LLVMValueRef function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
    LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(function);
    LLVMValueRef first_instruction = LLVMGetFirstInstruction(entry);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(ctx->llctx);
    LLVMValueRef alloca;

    if (!builder) {
        bit_irgen_fail(ctx, "failed to create temporary LLVM builder", bit_empty_span());
        return NULL;
    }

    if (first_instruction) {
        LLVMPositionBuilderBefore(builder, first_instruction);
    } else {
        LLVMPositionBuilderAtEnd(builder, entry);
    }

    alloca = LLVMBuildAlloca(builder, type, "");
    LLVMDisposeBuilder(builder);
    return alloca;
}

static LLVMTypeRef bit_lower_type(BitIrgenContext *ctx, const BitTypeRef *type) {
    switch (type->kind) {
        case BIT_TYPE_I32:
            return ctx->i32_type;
        case BIT_TYPE_BOOL:
            return ctx->bool_type;
    }

    bit_irgen_fail(ctx, "unsupported type", type->span);
    return NULL;
}

static LLVMTypeRef bit_lower_function_type(BitIrgenContext *ctx, const BitFunctionDecl *function) {
    LLVMTypeRef return_type;
    LLVMTypeRef *param_types = NULL;
    LLVMTypeRef function_type;
    size_t i;

    return_type = bit_lower_type(ctx, &function->return_type);
    if (!return_type) {
        return NULL;
    }

    if (function->param_count > 0) {
        param_types = (LLVMTypeRef *)malloc(function->param_count * sizeof(LLVMTypeRef));
        if (!param_types) {
            bit_irgen_fail(ctx, "out of memory", function->span);
            return NULL;
        }

        for (i = 0; i < function->param_count; ++i) {
            param_types[i] = bit_lower_type(ctx, &function->params[i].type);
            if (!param_types[i]) {
                free(param_types);
                return NULL;
            }
        }
    }

    function_type = LLVMFunctionType(
        return_type,
        param_types,
        (unsigned int)function->param_count,
        0
    );
    free(param_types);
    return function_type;
}

static const BitIrgenFunction *bit_irgen_find_function(const BitIrgenContext *ctx, BitStringView name) {
    size_t i;

    for (i = 0; i < ctx->function_count; ++i) {
        if (bit_string_view_equals(ctx->functions[i].name, name)) {
            return &ctx->functions[i];
        }
    }

    return NULL;
}

static int bit_irgen_bind_function(
    BitIrgenContext *ctx,
    BitStringView name,
    LLVMTypeRef type,
    LLVMValueRef value,
    const BitFunctionDecl *decl,
    BitSourceSpan span
) {
    BitIrgenFunction *new_functions;

    if (ctx->function_count == ctx->function_capacity) {
        size_t new_capacity = ctx->function_capacity == 0 ? 8 : ctx->function_capacity * 2;

        new_functions = (BitIrgenFunction *)realloc(
            ctx->functions,
            new_capacity * sizeof(BitIrgenFunction)
        );
        if (!new_functions) {
            return bit_irgen_fail(ctx, "out of memory", span);
        }

        ctx->functions = new_functions;
        ctx->function_capacity = new_capacity;
    }

    ctx->functions[ctx->function_count].name = name;
    ctx->functions[ctx->function_count].type = type;
    ctx->functions[ctx->function_count].value = value;
    ctx->functions[ctx->function_count].decl = decl;
    ctx->function_count += 1;
    return 1;
}

static const BitIrgenLocal *bit_irgen_find_local(const BitIrgenContext *ctx, BitStringView name) {
    size_t i = ctx->local_count;

    while (i > 0) {
        const BitIrgenLocal *local = &ctx->locals[i - 1];

        if (bit_string_view_equals(local->name, name)) {
            return local;
        }

        --i;
    }

    return NULL;
}

static int bit_irgen_bind_local(BitIrgenContext *ctx, BitStringView name, LLVMTypeRef type, LLVMValueRef storage, BitSourceSpan span) {
    BitIrgenLocal *new_locals;

    if (ctx->local_count == ctx->local_capacity) {
        size_t new_capacity = ctx->local_capacity == 0 ? 8 : ctx->local_capacity * 2;

        new_locals = (BitIrgenLocal *)realloc(ctx->locals, new_capacity * sizeof(BitIrgenLocal));
        if (!new_locals) {
            return bit_irgen_fail(ctx, "out of memory", span);
        }

        ctx->locals = new_locals;
        ctx->local_capacity = new_capacity;
    }

    ctx->locals[ctx->local_count].name = name;
    ctx->locals[ctx->local_count].type = type;
    ctx->locals[ctx->local_count].storage = storage;
    ctx->local_count += 1;
    return 1;
}

static int bit_lower_block(BitIrgenContext *ctx, const BitBlock *block);

static LLVMValueRef bit_lower_expr(BitIrgenContext *ctx, const BitExpr *expr) {
    switch (expr->kind) {
        case BIT_EXPR_INTEGER:
            return LLVMConstInt(ctx->i32_type, expr->as.integer.value, 0);
        case BIT_EXPR_BOOL:
            return LLVMConstInt(ctx->bool_type, expr->as.boolean.value != 0, 0);
        case BIT_EXPR_IDENTIFIER: {
            const BitIrgenLocal *local = bit_irgen_find_local(ctx, expr->as.name.name);

            if (!local) {
                bit_irgen_fail(ctx, "unresolved local binding", expr->span);
                return NULL;
            }

            return LLVMBuildLoad2(ctx->builder, local->type, local->storage, "");
        }
        case BIT_EXPR_CALL: {
            const BitIrgenFunction *function = bit_irgen_find_function(ctx, expr->as.call.callee);
            LLVMValueRef *args = NULL;
            LLVMValueRef value;
            size_t i;

            if (!function) {
                bit_irgen_fail(ctx, "unresolved function", expr->span);
                return NULL;
            }

            if (expr->as.call.arg_count > 0) {
                args = (LLVMValueRef *)malloc(expr->as.call.arg_count * sizeof(LLVMValueRef));
                if (!args) {
                    bit_irgen_fail(ctx, "out of memory", expr->span);
                    return NULL;
                }

                for (i = 0; i < expr->as.call.arg_count; ++i) {
                    args[i] = bit_lower_expr(ctx, expr->as.call.args[i]);
                    if (!args[i]) {
                        free(args);
                        return NULL;
                    }
                }
            }

            value = LLVMBuildCall2(
                ctx->builder,
                function->type,
                function->value,
                args,
                (unsigned int)expr->as.call.arg_count,
                ""
            );
            free(args);
            return value;
        }
        case BIT_EXPR_UNARY: {
            LLVMValueRef operand;

            if (!expr->as.unary.operand) {
                bit_irgen_fail(ctx, "unary expression requires an operand", expr->span);
                return NULL;
            }

            operand = bit_lower_expr(ctx, expr->as.unary.operand);
            if (!operand) {
                return NULL;
            }

            switch (expr->as.unary.op) {
                case BIT_UNARY_OP_NEG:
                    return LLVMBuildNeg(ctx->builder, operand, "");
            }

            bit_irgen_fail(ctx, "unsupported unary expression", expr->span);
            return NULL;
        }
        case BIT_EXPR_BINARY: {
            LLVMValueRef left;
            LLVMValueRef right;

            if (!expr->as.binary.left || !expr->as.binary.right) {
                bit_irgen_fail(ctx, "binary expression requires both operands", expr->span);
                return NULL;
            }

            left = bit_lower_expr(ctx, expr->as.binary.left);
            if (!left) {
                return NULL;
            }

            right = bit_lower_expr(ctx, expr->as.binary.right);
            if (!right) {
                return NULL;
            }

            switch (expr->as.binary.op) {
                case BIT_BINARY_OP_ADD:
                    return LLVMBuildAdd(ctx->builder, left, right, "");
                case BIT_BINARY_OP_SUB:
                    return LLVMBuildSub(ctx->builder, left, right, "");
                case BIT_BINARY_OP_MUL:
                    return LLVMBuildMul(ctx->builder, left, right, "");
                case BIT_BINARY_OP_DIV:
                    return LLVMBuildSDiv(ctx->builder, left, right, "");
                case BIT_BINARY_OP_EQUAL:
                    return LLVMBuildICmp(ctx->builder, LLVMIntEQ, left, right, "");
                case BIT_BINARY_OP_NOT_EQUAL:
                    return LLVMBuildICmp(ctx->builder, LLVMIntNE, left, right, "");
                case BIT_BINARY_OP_LESS:
                    return LLVMBuildICmp(ctx->builder, LLVMIntSLT, left, right, "");
                case BIT_BINARY_OP_LESS_EQUAL:
                    return LLVMBuildICmp(ctx->builder, LLVMIntSLE, left, right, "");
                case BIT_BINARY_OP_GREATER:
                    return LLVMBuildICmp(ctx->builder, LLVMIntSGT, left, right, "");
                case BIT_BINARY_OP_GREATER_EQUAL:
                    return LLVMBuildICmp(ctx->builder, LLVMIntSGE, left, right, "");
            }
        }
    }

    bit_irgen_fail(ctx, "unsupported expression", expr->span);
    return NULL;
}

static int bit_lower_stmt(BitIrgenContext *ctx, const BitStmt *stmt) {
    LLVMBasicBlockRef current_block = LLVMGetInsertBlock(ctx->builder);

    if (current_block && LLVMGetBasicBlockTerminator(current_block)) {
        return bit_irgen_fail(ctx, "statement is unreachable", stmt->span);
    }

    switch (stmt->kind) {
        case BIT_STMT_LET: {
            LLVMTypeRef type;
            LLVMValueRef initializer;
            LLVMValueRef storage;

            if (!stmt->as.let.initializer) {
                return bit_irgen_fail(ctx, "let statement requires an initializer", stmt->span);
            }

            type = bit_lower_type(ctx, &stmt->as.let.type);
            if (!type) {
                return 0;
            }

            initializer = bit_lower_expr(ctx, stmt->as.let.initializer);
            if (!initializer) {
                return 0;
            }

            storage = bit_create_entry_alloca(ctx, type);
            if (!storage) {
                return 0;
            }

            LLVMBuildStore(ctx->builder, initializer, storage);
            return bit_irgen_bind_local(ctx, stmt->as.let.name, type, storage, stmt->span);
        }
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
        case BIT_STMT_IF: {
            LLVMValueRef condition;
            LLVMValueRef function;
            LLVMBasicBlockRef then_block;
            LLVMBasicBlockRef else_block;
            LLVMBasicBlockRef merge_block;
            LLVMBasicBlockRef then_end;
            LLVMBasicBlockRef else_end;
            int needs_merge = 0;

            if (!stmt->as.if_stmt.condition) {
                return bit_irgen_fail(ctx, "if statement requires a condition", stmt->span);
            }

            condition = bit_lower_expr(ctx, stmt->as.if_stmt.condition);
            if (!condition) {
                return 0;
            }

            function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(ctx->builder));
            then_block = LLVMAppendBasicBlockInContext(ctx->llctx, function, "if.then");
            else_block = LLVMAppendBasicBlockInContext(ctx->llctx, function, "if.else");
            merge_block = LLVMAppendBasicBlockInContext(ctx->llctx, function, "if.end");

            LLVMBuildCondBr(ctx->builder, condition, then_block, else_block);

            LLVMPositionBuilderAtEnd(ctx->builder, then_block);
            if (!bit_lower_block(ctx, &stmt->as.if_stmt.then_block)) {
                return 0;
            }

            then_end = LLVMGetInsertBlock(ctx->builder);
            if (!LLVMGetBasicBlockTerminator(then_end)) {
                LLVMBuildBr(ctx->builder, merge_block);
                needs_merge = 1;
            }

            LLVMPositionBuilderAtEnd(ctx->builder, else_block);
            if (!bit_lower_block(ctx, &stmt->as.if_stmt.else_block)) {
                return 0;
            }

            else_end = LLVMGetInsertBlock(ctx->builder);
            if (!LLVMGetBasicBlockTerminator(else_end)) {
                LLVMBuildBr(ctx->builder, merge_block);
                needs_merge = 1;
            }

            if (needs_merge) {
                LLVMPositionBuilderAtEnd(ctx->builder, merge_block);
            } else {
                LLVMDeleteBasicBlock(merge_block);
                LLVMPositionBuilderAtEnd(ctx->builder, else_end);
            }

            return 1;
        }
    }

    return bit_irgen_fail(ctx, "unsupported statement", stmt->span);
}

static int bit_lower_block(BitIrgenContext *ctx, const BitBlock *block) {
    size_t i;
    size_t saved_local_count = ctx->local_count;

    for (i = 0; i < block->stmt_count; ++i) {
        if (!bit_lower_stmt(ctx, block->stmts[i])) {
            ctx->local_count = saved_local_count;
            return 0;
        }
    }

    ctx->local_count = saved_local_count;
    return 1;
}

static int bit_lower_function(BitIrgenContext *ctx, const BitFunctionDecl *function) {
    const BitIrgenFunction *ir_function;
    LLVMBasicBlockRef entry;
    LLVMBasicBlockRef current_block;
    size_t i;

    ir_function = bit_irgen_find_function(ctx, function->name);
    if (!ir_function) {
        return bit_irgen_fail(ctx, "failed to find declared function", function->span);
    }

    ctx->local_count = 0;
    entry = LLVMAppendBasicBlockInContext(ctx->llctx, ir_function->value, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);

    for (i = 0; i < function->param_count; ++i) {
        LLVMValueRef param_value = LLVMGetParam(ir_function->value, (unsigned int)i);
        LLVMTypeRef param_type = bit_lower_type(ctx, &function->params[i].type);
        LLVMValueRef storage;

        if (!param_type) {
            return 0;
        }

        LLVMSetValueName2(param_value, function->params[i].name.data, function->params[i].name.length);
        storage = bit_create_entry_alloca(ctx, param_type);
        if (!storage) {
            return 0;
        }

        LLVMBuildStore(ctx->builder, param_value, storage);
        if (!bit_irgen_bind_local(ctx, function->params[i].name, param_type, storage, function->params[i].span)) {
            return 0;
        }
    }

    if (!bit_lower_block(ctx, &function->body)) {
        return 0;
    }

    current_block = LLVMGetInsertBlock(ctx->builder);
    if (!current_block || !LLVMGetBasicBlockTerminator(current_block)) {
        return bit_irgen_fail(ctx, "function body did not produce a terminator", function->body.span);
    }

    return 1;
}

static int bit_declare_function(BitIrgenContext *ctx, const BitFunctionDecl *function) {
    LLVMTypeRef function_type = bit_lower_function_type(ctx, function);
    LLVMValueRef function_value;

    if (!function_type) {
        return 0;
    }

    function_value = LLVMAddFunction(ctx->llmod, "", function_type);
    if (!function_value) {
        return bit_irgen_fail(ctx, "failed to create function", function->span);
    }

    LLVMSetValueName2(function_value, function->name.data, function->name.length);
    return bit_irgen_bind_function(
        ctx,
        function->name,
        function_type,
        function_value,
        function,
        function->span
    );
}

static int bit_lower_module(BitIrgenContext *ctx, const BitModule *module) {
    size_t i;

    if (!module || module->function_count == 0) {
        return bit_irgen_fail(ctx, "module has no functions", bit_empty_span());
    }

    for (i = 0; i < module->function_count; ++i) {
        if (!bit_declare_function(ctx, module->functions[i])) {
            return 0;
        }
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
    ctx.bool_type = NULL;
    ctx.diagnostic.message = NULL;
    ctx.diagnostic.span = bit_empty_span();
    ctx.functions = NULL;
    ctx.function_count = 0;
    ctx.function_capacity = 0;
    ctx.locals = NULL;
    ctx.local_count = 0;
    ctx.local_capacity = 0;
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
