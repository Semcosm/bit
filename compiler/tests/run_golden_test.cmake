cmake_minimum_required(VERSION 3.20)

function(bit_normalize_text input output_var)
    string(REPLACE "\r\n" "\n" normalized "${input}")
    string(REPLACE "\r" "\n" normalized "${normalized}")
    set(${output_var} "${normalized}" PARENT_SCOPE)
endfunction()

function(bit_read_expected_file path output_var)
    if(DEFINED path AND NOT path STREQUAL "")
        if(NOT EXISTS "${path}")
            message(FATAL_ERROR "missing golden file: ${path}")
        endif()

        file(READ "${path}" content)
    else()
        set(content "")
    endif()

    bit_normalize_text("${content}" content)
    set(${output_var} "${content}" PARENT_SCOPE)
endfunction()

function(bit_assert_text_equal label expected actual)
    bit_normalize_text("${expected}" normalized_expected)
    bit_normalize_text("${actual}" normalized_actual)

    if(NOT normalized_expected STREQUAL normalized_actual)
        message(
            FATAL_ERROR
            "${TEST_NAME}: ${label} mismatch\n"
            "--- expected ---\n${normalized_expected}\n"
            "--- actual ---\n${normalized_actual}\n"
        )
    endif()
endfunction()

if(NOT DEFINED TEST_COMMAND OR TEST_COMMAND STREQUAL "")
    message(FATAL_ERROR "TEST_COMMAND is required")
endif()

if(NOT DEFINED TEST_WORKING_DIRECTORY OR TEST_WORKING_DIRECTORY STREQUAL "")
    set(TEST_WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
endif()

if(NOT DEFINED EXPECT_EXIT_CODE OR EXPECT_EXIT_CODE STREQUAL "")
    set(EXPECT_EXIT_CODE 0)
endif()

if(DEFINED ACTUAL_OUTPUT_FILE AND NOT ACTUAL_OUTPUT_FILE STREQUAL "")
    get_filename_component(output_dir "${ACTUAL_OUTPUT_FILE}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_dir}")
    file(REMOVE "${ACTUAL_OUTPUT_FILE}")
endif()

execute_process(
    COMMAND ${TEST_COMMAND}
    WORKING_DIRECTORY "${TEST_WORKING_DIRECTORY}"
    RESULT_VARIABLE actual_exit_code
    OUTPUT_VARIABLE actual_stdout
    ERROR_VARIABLE actual_stderr
)

if(NOT "${actual_exit_code}" STREQUAL "${EXPECT_EXIT_CODE}")
    message(
        FATAL_ERROR
        "${TEST_NAME}: expected exit code ${EXPECT_EXIT_CODE}, got ${actual_exit_code}\n"
        "--- stdout ---\n${actual_stdout}\n"
        "--- stderr ---\n${actual_stderr}\n"
    )
endif()

bit_read_expected_file("${EXPECT_STDOUT}" expected_stdout)
bit_read_expected_file("${EXPECT_STDERR}" expected_stderr)

bit_assert_text_equal("stdout" "${expected_stdout}" "${actual_stdout}")
bit_assert_text_equal("stderr" "${expected_stderr}" "${actual_stderr}")

if(DEFINED EXPECT_OUTPUT_FILE AND NOT EXPECT_OUTPUT_FILE STREQUAL "")
    if(NOT DEFINED ACTUAL_OUTPUT_FILE OR ACTUAL_OUTPUT_FILE STREQUAL "")
        message(FATAL_ERROR "${TEST_NAME}: EXPECT_OUTPUT_FILE requires ACTUAL_OUTPUT_FILE")
    endif()

    if(NOT EXISTS "${ACTUAL_OUTPUT_FILE}")
        message(FATAL_ERROR "${TEST_NAME}: expected output file was not produced: ${ACTUAL_OUTPUT_FILE}")
    endif()

    file(READ "${EXPECT_OUTPUT_FILE}" expected_output)
    file(READ "${ACTUAL_OUTPUT_FILE}" actual_output)
    bit_assert_text_equal("output file" "${expected_output}" "${actual_output}")
endif()
