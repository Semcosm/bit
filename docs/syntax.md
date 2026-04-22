# Syntax

当前 bit 原型只实现了一个很小的语法子集，目标是先维持

`source -> lexer -> parser -> sema -> LLVM IR`

这条链路稳定。

## Grammar Sketch

```text
module      ::= function+
function    ::= "fn" IDENT "(" param_list? ")" "->" type block
param_list  ::= param ("," param)*
param       ::= IDENT ":" type
type        ::= "i32" | "bool"
block       ::= "{" stmt* "}"
stmt        ::= let_stmt | return_stmt | if_stmt
let_stmt    ::= "let" IDENT ":" type "=" expr ";"
return_stmt ::= "return" expr ";"
if_stmt     ::= "if" expr block "else" block

expr        ::= equality
equality    ::= comparison (("==" | "!=") comparison)*
comparison  ::= additive (("<" | "<=" | ">" | ">=") additive)*
additive    ::= multiplicative (("+" | "-") multiplicative)*
multiplicative
            ::= unary (("*" | "/") unary)*
unary       ::= "-" unary | primary
primary     ::= INTEGER | "true" | "false" | IDENT | call | "(" expr ")"
call        ::= IDENT "(" arg_list? ")"
arg_list    ::= expr ("," expr)*
```

## Notes

- 当前模块允许多个函数，但必须定义 `main`
- 当前局部绑定现在具有块作用域
- 当前支持 `i32` 和 `bool`
- `-` 同时用于二元减法和一元负号
- 比较表达式当前不支持链式专门语义，只按普通二元优先级解析
- 调用表达式当前只支持按名字调用模块内函数
- parser 在遇到第一处错误时立即停止，并报告 token 与行列号
