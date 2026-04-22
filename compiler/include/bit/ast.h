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
    BIT_TYPE_BOOL,
} BitTypeKind;

typedef struct BitTypeRef {
    BitTypeKind kind;
    BitSourceSpan span;
} BitTypeRef;

typedef enum BitBinaryOpKind {
    BIT_BINARY_OP_ADD = 0,
    BIT_BINARY_OP_SUB,
    BIT_BINARY_OP_MUL,
    BIT_BINARY_OP_DIV,
    BIT_BINARY_OP_EQUAL,
    BIT_BINARY_OP_NOT_EQUAL,
    BIT_BINARY_OP_LESS,
    BIT_BINARY_OP_LESS_EQUAL,
    BIT_BINARY_OP_GREATER,
    BIT_BINARY_OP_GREATER_EQUAL,
} BitBinaryOpKind;

typedef enum BitUnaryOpKind {
    BIT_UNARY_OP_NEG = 0,
} BitUnaryOpKind;

typedef enum BitExprKind {
    BIT_EXPR_INTEGER = 0,
    BIT_EXPR_BOOL,
    BIT_EXPR_IDENTIFIER,
    BIT_EXPR_CALL,
    BIT_EXPR_UNARY,
    BIT_EXPR_BINARY,
} BitExprKind;

typedef struct BitIntegerExpr {
    uint64_t value;
    BitSourceSpan span;
} BitIntegerExpr;

typedef struct BitBoolExpr {
    int value;
    BitSourceSpan span;
} BitBoolExpr;

typedef struct BitNameExpr {
    BitStringView name;
    BitSourceSpan span;
} BitNameExpr;

typedef struct BitExpr BitExpr;
typedef struct BitStmt BitStmt;
typedef struct BitBlock BitBlock;

typedef struct BitCallExpr {
    BitStringView callee;
    BitExpr **args;
    size_t arg_count;
    BitSourceSpan span;
} BitCallExpr;

typedef struct BitUnaryExpr {
    BitUnaryOpKind op;
    BitExpr *operand;
    BitSourceSpan span;
} BitUnaryExpr;

typedef struct BitBinaryExpr {
    BitBinaryOpKind op;
    BitExpr *left;
    BitExpr *right;
    BitSourceSpan span;
} BitBinaryExpr;

struct BitExpr {
    BitExprKind kind;
    BitSourceSpan span;
    union {
        BitIntegerExpr integer;
        BitBoolExpr boolean;
        BitNameExpr name;
        BitCallExpr call;
        BitUnaryExpr unary;
        BitBinaryExpr binary;
    } as;
};

typedef enum BitStmtKind {
    BIT_STMT_LET = 0,
    BIT_STMT_RETURN,
    BIT_STMT_IF,
} BitStmtKind;

struct BitBlock {
    BitStmt **stmts;
    size_t stmt_count;
    BitSourceSpan span;
};

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

typedef struct BitIfStmt {
    BitExpr *condition;
    struct BitBlock then_block;
    struct BitBlock else_block;
    BitSourceSpan span;
} BitIfStmt;

struct BitStmt {
    BitStmtKind kind;
    BitSourceSpan span;
    union {
        BitLetStmt let;
        BitReturnStmt ret;
        BitIfStmt if_stmt;
    } as;
};

typedef struct BitParamDecl {
    BitStringView name;
    BitTypeRef type;
    BitSourceSpan span;
} BitParamDecl;

typedef struct BitFunctionDecl {
    BitStringView name;
    BitParamDecl *params;
    size_t param_count;
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
