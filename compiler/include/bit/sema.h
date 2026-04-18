#ifndef BIT_SEMA_H
#define BIT_SEMA_H

#include <stdio.h>

#include "bit/ast.h"

typedef enum BitSemaStatus {
    BIT_SEMA_OK = 0,
    BIT_SEMA_ERROR,
} BitSemaStatus;

typedef struct BitSemaDiagnostic {
    const char *message;
    BitSourceSpan span;
} BitSemaDiagnostic;

typedef struct BitSemaResult {
    BitSemaStatus status;
    BitSemaDiagnostic diagnostic;
} BitSemaResult;

BitSemaResult bit_analyze_module(const BitModule *module);
void bit_print_sema_diagnostic(FILE *stream, const BitSemaDiagnostic *diagnostic);

#endif
