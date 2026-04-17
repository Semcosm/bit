#include "bit/irgen.h"

#include <stdio.h>

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>

int bit_emit_minimal_module(const char *output_path) {
    int status = 1;
    char *error = NULL;

    LLVMContextRef context = LLVMContextCreate();
    if (!context) {
        fprintf(stderr, "error: failed to create LLVM context\n");
        return 1;
    }

    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("bit_module", context);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);

    LLVMTypeRef i32_type = LLVMInt32TypeInContext(context);
    LLVMTypeRef fn_type = LLVMFunctionType(i32_type, NULL, 0, 0);
    LLVMValueRef fn_main = LLVMAddFunction(module, "main", fn_type);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn_main, "entry");
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildRet(builder, LLVMConstInt(i32_type, 0, 0));

    if (LLVMVerifyModule(module, LLVMReturnStatusAction, &error)) {
        fprintf(stderr, "error: invalid LLVM module: %s\n", error);
        LLVMDisposeMessage(error);
        error = NULL;
        goto cleanup;
    }

    if (LLVMPrintModuleToFile(module, output_path, &error) != 0) {
        fprintf(stderr, "error: failed to write IR file '%s': %s\n", output_path, error);
        LLVMDisposeMessage(error);
        error = NULL;
        goto cleanup;
    }

    status = 0;

cleanup:
    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(context);
    return status;
}
