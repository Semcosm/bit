# Syntax

当前 bit 原型只实现了一个很小的语法子集，目标是先维持

`source -> lexer -> parser -> sema -> LLVM IR`

这条链路稳定。

## Grammar Sketch

```text
module      ::= function
function    ::= "fn" IDENT "(" ")" "->" type block
type        ::= "i32"
block       ::= "{" stmt* "}"
stmt        ::= let_stmt | return_stmt
let_stmt    ::= "let" IDENT ":" type "=" expr ";"
return_stmt ::= "return" expr ";"

expr        ::= additive
additive    ::= multiplicative (("+" | "-") multiplicative)*
multiplicative
            ::= unary (("*" | "/") unary)*
unary       ::= "-" unary | primary
primary     ::= INTEGER | IDENT | "(" expr ")"
```

## Notes

- 当前只允许一个函数，并且必须命名为 `main`
- 当前只支持 `i32`
- `-` 同时用于二元减法和一元负号
- parser 在遇到第一处错误时立即停止，并报告 token 与行列号
