cmake_minimum_required(VERSION 3.20)
project(bit LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

execute_process(
    COMMAND llvm-config --cflags
    OUTPUT_VARIABLE LLVM_CFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND llvm-config --ldflags
    OUTPUT_VARIABLE LLVM_LDFLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND llvm-config --libs core
    OUTPUT_VARIABLE LLVM_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND llvm-config --system-libs
    OUTPUT_VARIABLE LLVM_SYSTEM_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

separate_arguments(LLVM_CFLAGS_LIST NATIVE_COMMAND "${LLVM_CFLAGS}")
separate_arguments(LLVM_LDFLAGS_LIST NATIVE_COMMAND "${LLVM_LDFLAGS}")
separate_arguments(LLVM_LIBS_LIST NATIVE_COMMAND "${LLVM_LIBS}")
separate_arguments(LLVM_SYSTEM_LIBS_LIST NATIVE_COMMAND "${LLVM_SYSTEM_LIBS}")

add_executable(bitc
    compiler/src/main/main.c
    compiler/src/irgen/irgen.c
    compiler/src/ast/ast.c
    compiler/src/lex/lexer.c
    compiler/src/parse/parser.c
    compiler/src/lex/token.c
    compiler/src/support/arena.c
    compiler/src/support/file.c
)

target_include_directories(bitc PRIVATE
    compiler/include
)

target_compile_options(bitc PRIVATE
    ${LLVM_CFLAGS_LIST}
)

target_link_options(bitc PRIVATE
    ${LLVM_LDFLAGS_LIST}
)

target_link_libraries(bitc PRIVATE
    ${LLVM_LIBS_LIST}
    ${LLVM_SYSTEM_LIBS_LIST}
)

add_executable(test_lexer
    compiler/tests/test_lexer.c
    compiler/src/lex/lexer.c
    compiler/src/lex/token.c
    compiler/src/support/file.c
)

target_include_directories(test_lexer PRIVATE
    compiler/include
)

add_executable(test_parser
    compiler/tests/test_parser.c
    compiler/src/ast/ast.c
    compiler/src/lex/lexer.c
    compiler/src/parse/parser.c
    compiler/src/lex/token.c
    compiler/src/support/arena.c
    compiler/src/support/file.c
)

target_include_directories(test_parser PRIVATE
    compiler/include
)
