; ModuleID = 'bit_test_module'
source_filename = "compiler/tests/fixtures/irgen/unary_negation.bit"

define i32 @main() {
entry:
  %0 = alloca i32, align 4
  store i32 5, ptr %0, align 4
  %1 = load i32, ptr %0, align 4
  %2 = mul i32 %1, 2
  %3 = sub i32 0, %2
  ret i32 %3
}
