; ModuleID = 'bit_test_module'
source_filename = "compiler/tests/fixtures/irgen/function_call.bit"

define i32 @add(i32 %lhs, i32 %rhs) {
entry:
  %0 = alloca i32, align 4
  %1 = alloca i32, align 4
  store i32 %lhs, ptr %1, align 4
  store i32 %rhs, ptr %0, align 4
  %2 = load i32, ptr %1, align 4
  %3 = load i32, ptr %0, align 4
  %4 = add i32 %2, %3
  ret i32 %4
}

define i32 @main() {
entry:
  %0 = call i32 @add(i32 1, i32 2)
  ret i32 %0
}
