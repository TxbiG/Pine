cmake_minimum_required(VERSION 3.20)
project(pine C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(pine
    src/main.c
    src/lexer.c
    src/parser.c
    src/sema.c
    src/ast.c
    src/codegen.c
    src/ir.c
    src/native.c
)

if(MSVC)
    target_compile_options(pine PRIVATE /W4)
else()
    target_compile_options(pine PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
find_package(Python3 COMPONENTS Interpreter)
if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(
        NAME pine_source_syntax
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/source_syntax_check.py"
    )
    add_test(
        NAME pine_regression
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/run_tests.py"
                --pine "$<TARGET_FILE:pine>"
    )
endif()
