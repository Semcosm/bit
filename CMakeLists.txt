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
