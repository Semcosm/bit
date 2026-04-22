cmake_minimum_required(VERSION 3.20)
project(bit LANGUAGES C)

option(BUILD_TESTING "Build the test suite" ON)

if(BUILD_TESTING)
    enable_testing()
endif()

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

add_library(bit_frontend_core STATIC
    compiler/src/ast/ast.c
    compiler/src/lex/lexer.c
    compiler/src/parse/parser.c
    compiler/src/sema/sema.c
    compiler/src/lex/token.c
    compiler/src/support/arena.c
    compiler/src/support/file.c
)

target_include_directories(bit_frontend_core PUBLIC
    compiler/include
)

add_library(bit_backend_llvm STATIC
    compiler/src/irgen/irgen.c
)

target_include_directories(bit_backend_llvm PUBLIC
    compiler/include
)

target_compile_options(bit_backend_llvm PRIVATE
    ${LLVM_CFLAGS_LIST}
)

target_link_options(bit_backend_llvm PRIVATE
    ${LLVM_LDFLAGS_LIST}
)

target_link_libraries(bit_backend_llvm PRIVATE
    ${LLVM_LIBS_LIST}
    ${LLVM_SYSTEM_LIBS_LIST}
)

add_executable(bitc
    compiler/src/main/main.c
)

target_link_libraries(bitc PRIVATE
    bit_frontend_core
    bit_backend_llvm
)

add_executable(test_lexer
    compiler/tests/test_lexer.c
)

target_link_libraries(test_lexer PRIVATE
    bit_frontend_core
)

add_executable(test_parser
    compiler/tests/test_parser.c
)

target_link_libraries(test_parser PRIVATE
    bit_frontend_core
)

add_executable(test_sema
    compiler/tests/test_sema.c
)

target_link_libraries(test_sema PRIVATE
    bit_frontend_core
)

add_executable(test_irgen
    compiler/tests/test_irgen.c
)

target_link_libraries(test_irgen PRIVATE
    bit_frontend_core
    bit_backend_llvm
)

if(BUILD_TESTING)
    set(BIT_TEST_RUNNER "${CMAKE_SOURCE_DIR}/compiler/tests/run_golden_test.cmake")
    set(BIT_TEST_ARTIFACT_DIR "${CMAKE_BINARY_DIR}/test-artifacts")

    file(MAKE_DIRECTORY "${BIT_TEST_ARTIFACT_DIR}")

    function(bit_add_golden_test name target)
        set(one_value_args
            EXPECT_STDOUT
            EXPECT_STDERR
            EXPECT_EXIT_CODE
            EXPECT_OUTPUT_FILE
            ACTUAL_OUTPUT_FILE
        )
        set(multi_value_args
            ARGS
            LABELS
        )

        cmake_parse_arguments(TEST "" "${one_value_args}" "${multi_value_args}" ${ARGN})

        if(NOT DEFINED TEST_EXPECT_EXIT_CODE OR TEST_EXPECT_EXIT_CODE STREQUAL "")
            set(TEST_EXPECT_EXIT_CODE 0)
        endif()

        set(test_command "$<TARGET_FILE:${target}>")
        list(APPEND test_command ${TEST_ARGS})

        set(expect_stdout "")
        set(expect_stderr "")
        set(expect_output_file "")
        set(actual_output_file "")

        if(DEFINED TEST_EXPECT_STDOUT AND NOT TEST_EXPECT_STDOUT STREQUAL "")
            get_filename_component(expect_stdout "${TEST_EXPECT_STDOUT}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
        endif()

        if(DEFINED TEST_EXPECT_STDERR AND NOT TEST_EXPECT_STDERR STREQUAL "")
            get_filename_component(expect_stderr "${TEST_EXPECT_STDERR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
        endif()

        if(DEFINED TEST_EXPECT_OUTPUT_FILE AND NOT TEST_EXPECT_OUTPUT_FILE STREQUAL "")
            get_filename_component(expect_output_file "${TEST_EXPECT_OUTPUT_FILE}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
        endif()

        if(DEFINED TEST_ACTUAL_OUTPUT_FILE AND NOT TEST_ACTUAL_OUTPUT_FILE STREQUAL "")
            get_filename_component(actual_output_file "${TEST_ACTUAL_OUTPUT_FILE}" ABSOLUTE BASE_DIR "${CMAKE_BINARY_DIR}")
        endif()

        add_test(
            NAME ${name}
            COMMAND ${CMAKE_COMMAND}
                "-DTEST_NAME=${name}"
                "-DTEST_WORKING_DIRECTORY=${CMAKE_SOURCE_DIR}"
                "-DTEST_COMMAND=${test_command}"
                "-DEXPECT_EXIT_CODE=${TEST_EXPECT_EXIT_CODE}"
                "-DEXPECT_STDOUT=${expect_stdout}"
                "-DEXPECT_STDERR=${expect_stderr}"
                "-DEXPECT_OUTPUT_FILE=${expect_output_file}"
                "-DACTUAL_OUTPUT_FILE=${actual_output_file}"
                -P "${BIT_TEST_RUNNER}"
        )

        if(TEST_LABELS)
            set_tests_properties(${name} PROPERTIES LABELS "${TEST_LABELS}")
        endif()
    endfunction()

    bit_add_golden_test(
        lexer_valid_tokens
        test_lexer
        ARGS compiler/tests/fixtures/lexer/valid_tokens.bit
        EXPECT_STDOUT compiler/tests/golden/lexer/valid_tokens.stdout
        LABELS lexer
    )

    bit_add_golden_test(
        lexer_invalid_character
        test_lexer
        ARGS compiler/tests/fixtures/lexer/invalid_character.bit
        EXPECT_STDOUT compiler/tests/golden/lexer/invalid_character.stdout
        EXPECT_EXIT_CODE 1
        LABELS lexer
    )

    bit_add_golden_test(
        lexer_control_flow_tokens
        test_lexer
        ARGS compiler/tests/fixtures/lexer/control_flow_tokens.bit
        EXPECT_STDOUT compiler/tests/golden/lexer/control_flow_tokens.stdout
        LABELS lexer
    )

    bit_add_golden_test(
        parser_arithmetic_precedence
        test_parser
        ARGS compiler/tests/fixtures/parser/arithmetic_precedence.bit
        EXPECT_STDOUT compiler/tests/golden/parser/arithmetic_precedence.stdout
        LABELS parser
    )

    bit_add_golden_test(
        parser_unary_precedence
        test_parser
        ARGS compiler/tests/fixtures/parser/unary_precedence.bit
        EXPECT_STDOUT compiler/tests/golden/parser/unary_precedence.stdout
        LABELS parser
    )

    bit_add_golden_test(
        parser_function_call
        test_parser
        ARGS compiler/tests/fixtures/parser/function_call.bit
        EXPECT_STDOUT compiler/tests/golden/parser/function_call.stdout
        LABELS parser
    )

    bit_add_golden_test(
        parser_if_else_bool
        test_parser
        ARGS compiler/tests/fixtures/parser/if_else_bool.bit
        EXPECT_STDOUT compiler/tests/golden/parser/if_else_bool.stdout
        LABELS parser
    )

    bit_add_golden_test(
        parser_missing_semicolon
        test_parser
        ARGS compiler/tests/fixtures/parser/missing_semicolon.bit
        EXPECT_STDERR compiler/tests/golden/parser/missing_semicolon.stderr
        EXPECT_EXIT_CODE 1
        LABELS parser
    )

    bit_add_golden_test(
        sema_valid_negation
        test_sema
        ARGS compiler/tests/fixtures/sema/valid_negation.bit
        EXPECT_STDOUT compiler/tests/golden/sema/valid_negation.stdout
        LABELS sema
    )

    bit_add_golden_test(
        sema_unknown_identifier
        test_sema
        ARGS compiler/tests/fixtures/sema/unknown_identifier.bit
        EXPECT_STDERR compiler/tests/golden/sema/unknown_identifier.stderr
        EXPECT_EXIT_CODE 1
        LABELS sema
    )

    bit_add_golden_test(
        sema_negative_i32_min
        test_sema
        ARGS compiler/tests/fixtures/sema/negative_i32_min.bit
        EXPECT_STDOUT compiler/tests/golden/sema/negative_i32_min.stdout
        LABELS sema
    )

    bit_add_golden_test(
        sema_negative_out_of_range
        test_sema
        ARGS compiler/tests/fixtures/sema/negative_out_of_range.bit
        EXPECT_STDERR compiler/tests/golden/sema/negative_out_of_range.stderr
        EXPECT_EXIT_CODE 1
        LABELS sema
    )

    bit_add_golden_test(
        sema_call_arity_error
        test_sema
        ARGS compiler/tests/fixtures/sema/call_arity_error.bit
        EXPECT_STDERR compiler/tests/golden/sema/call_arity_error.stderr
        EXPECT_EXIT_CODE 1
        LABELS sema
    )

    bit_add_golden_test(
        sema_unknown_function
        test_sema
        ARGS compiler/tests/fixtures/sema/unknown_function.bit
        EXPECT_STDERR compiler/tests/golden/sema/unknown_function.stderr
        EXPECT_EXIT_CODE 1
        LABELS sema
    )

    bit_add_golden_test(
        sema_condition_type_error
        test_sema
        ARGS compiler/tests/fixtures/sema/condition_type_error.bit
        EXPECT_STDERR compiler/tests/golden/sema/condition_type_error.stderr
        EXPECT_EXIT_CODE 1
        LABELS sema
    )

    bit_add_golden_test(
        sema_missing_return_path
        test_sema
        ARGS compiler/tests/fixtures/sema/missing_return_path.bit
        EXPECT_STDERR compiler/tests/golden/sema/missing_return_path.stderr
        EXPECT_EXIT_CODE 1
        LABELS sema
    )

    bit_add_golden_test(
        irgen_arithmetic_precedence
        test_irgen
        ARGS
            compiler/tests/fixtures/irgen/arithmetic_precedence.bit
            ${BIT_TEST_ARTIFACT_DIR}/irgen_arithmetic_precedence.ll
        EXPECT_STDOUT compiler/tests/golden/irgen/arithmetic_precedence.stdout
        EXPECT_OUTPUT_FILE compiler/tests/golden/irgen/arithmetic_precedence.ll
        ACTUAL_OUTPUT_FILE ${BIT_TEST_ARTIFACT_DIR}/irgen_arithmetic_precedence.ll
        LABELS irgen
    )

    bit_add_golden_test(
        irgen_unary_negation
        test_irgen
        ARGS
            compiler/tests/fixtures/irgen/unary_negation.bit
            ${BIT_TEST_ARTIFACT_DIR}/irgen_unary_negation.ll
        EXPECT_STDOUT compiler/tests/golden/irgen/unary_negation.stdout
        EXPECT_OUTPUT_FILE compiler/tests/golden/irgen/unary_negation.ll
        ACTUAL_OUTPUT_FILE ${BIT_TEST_ARTIFACT_DIR}/irgen_unary_negation.ll
        LABELS irgen
    )

    bit_add_golden_test(
        irgen_function_call
        test_irgen
        ARGS
            compiler/tests/fixtures/irgen/function_call.bit
            ${BIT_TEST_ARTIFACT_DIR}/irgen_function_call.ll
        EXPECT_STDOUT compiler/tests/golden/irgen/function_call.stdout
        EXPECT_OUTPUT_FILE compiler/tests/golden/irgen/function_call.ll
        ACTUAL_OUTPUT_FILE ${BIT_TEST_ARTIFACT_DIR}/irgen_function_call.ll
        LABELS irgen
    )

    bit_add_golden_test(
        irgen_if_else_compare
        test_irgen
        ARGS
            compiler/tests/fixtures/irgen/if_else_compare.bit
            ${BIT_TEST_ARTIFACT_DIR}/irgen_if_else_compare.ll
        EXPECT_STDOUT compiler/tests/golden/irgen/if_else_compare.stdout
        EXPECT_OUTPUT_FILE compiler/tests/golden/irgen/if_else_compare.ll
        ACTUAL_OUTPUT_FILE ${BIT_TEST_ARTIFACT_DIR}/irgen_if_else_compare.ll
        LABELS irgen
    )

    bit_add_golden_test(
        irgen_semantic_failure
        test_irgen
        ARGS
            compiler/tests/fixtures/irgen/semantic_failure.bit
            ${BIT_TEST_ARTIFACT_DIR}/irgen_semantic_failure.ll
        EXPECT_STDERR compiler/tests/golden/irgen/semantic_failure.stderr
        EXPECT_EXIT_CODE 1
        LABELS irgen
    )
endif()
