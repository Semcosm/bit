#ifndef BIT_IRGEN_H
#define BIT_IRGEN_H

#include <stdio.h>

#include "bit/ast.h"

typedef enum BitIrgenStatus {
    BIT_IRGEN_OK = 0,
    BIT_IRGEN_ERROR,
} BitIrgenStatus;

typedef struct BitIrgenDiagnostic {
    const char *message;
    BitSourceSpan span;
} BitIrgenDiagnostic;

typedef struct BitIrgenOptions {
    const char *module_name;
    const char *source_name;
    int verify_module;
} BitIrgenOptions;

typedef struct BitIrgenResult {
    BitIrgenStatus status;
    BitIrgenDiagnostic diagnostic;
} BitIrgenResult;

BitIrgenResult bit_emit_llvm_ir_file(
    const BitModule *module,
    const BitIrgenOptions *options,
    const char *output_path
);
void bit_print_irgen_diagnostic(FILE *stream, const BitIrgenDiagnostic *diagnostic);

#endif
