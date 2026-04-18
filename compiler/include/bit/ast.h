#ifndef BIT_AST_H
#define BIT_AST_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct BitSourceSpan {
    const char *start;
    size_t length;
    size_t line;
    size_t column;
} BitSourceSpan;

typedef struct BitStringView {
    const char *data;
    size_t length;
} BitStringView;

typedef enum BitTypeKind {
    BIT_TYPE_I32 = 0,
} BitTypeKind;

typedef struct BitTypeRef {
    BitTypeKind kind;
    BitSourceSpan span;
} BitTypeRef;

typedef enum BitExprKind {
    BIT_EXPR_INTEGER = 0,
    BIT_EXPR_IDENTIFIER,
} BitExprKind;

typedef struct BitIntegerExpr {
    uint64_t value;
    BitSourceSpan span;
} BitIntegerExpr;

typedef struct BitNameExpr {
    BitStringView name;
    BitSourceSpan span;
} BitNameExpr;

typedef struct BitExpr BitExpr;
typedef struct BitStmt BitStmt;

struct BitExpr {
    BitExprKind kind;
    BitSourceSpan span;
    union {
        BitIntegerExpr integer;
        BitNameExpr name;
    } as;
};

typedef enum BitStmtKind {
    BIT_STMT_LET = 0,
    BIT_STMT_RETURN,
} BitStmtKind;

typedef struct BitLetStmt {
    BitStringView name;
    BitTypeRef type;
    BitExpr *initializer;
    BitSourceSpan span;
} BitLetStmt;

typedef struct BitReturnStmt {
    BitExpr *expr;
    BitSourceSpan span;
} BitReturnStmt;

struct BitStmt {
    BitStmtKind kind;
    BitSourceSpan span;
    union {
        BitLetStmt let;
        BitReturnStmt ret;
    } as;
};

typedef struct BitBlock {
    BitStmt **stmts;
    size_t stmt_count;
    BitSourceSpan span;
} BitBlock;

typedef struct BitFunctionDecl {
    BitStringView name;
    BitTypeRef return_type;
    BitBlock body;
    BitSourceSpan span;
} BitFunctionDecl;

typedef struct BitModule {
    BitFunctionDecl **functions;
    size_t function_count;
    BitSourceSpan span;
} BitModule;

void bit_ast_dump_module(FILE *stream, const BitModule *module);

#endif
