; ModuleID = 'bit_test_module'
source_filename = "compiler/tests/fixtures/irgen/arithmetic_precedence.bit"

define i32 @main() {
entry:
  %0 = alloca i32, align 4
  store i32 7, ptr %0, align 4
  %1 = load i32, ptr %0, align 4
  %2 = sub i32 %1, 1
  ret i32 %2
}
