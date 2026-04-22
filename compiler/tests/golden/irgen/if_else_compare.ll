; ModuleID = 'bit_test_module'
source_filename = "compiler/tests/fixtures/irgen/if_else_compare.bit"

define i32 @main() {
entry:
  %0 = alloca i1, align 1
  %1 = alloca i1, align 1
  %2 = alloca i32, align 4
  store i32 1, ptr %2, align 4
  %3 = load i32, ptr %2, align 4
  %4 = icmp slt i32 %3, 2
  br i1 %4, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  store i1 true, ptr %1, align 1
  br label %if.end

if.else:                                          ; preds = %entry
  store i1 false, ptr %0, align 1
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  ret i32 0
}
