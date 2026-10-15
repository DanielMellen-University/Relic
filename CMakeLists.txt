cmake_minimum_required(VERSION 3.16)
project(relic VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

configure_file(src/version.hpp.in ${CMAKE_BINARY_DIR}/generated/version.hpp)

add_library(relic_core STATIC
  src/core.cpp
  src/diag/diag.cpp
  src/lexer/lexer.cpp
  src/ast/ast.cpp
  src/parser/parser.cpp
)
target_include_directories(relic_core PUBLIC src ${CMAKE_BINARY_DIR}/generated)

add_executable(relic src/main.cpp)
target_link_libraries(relic PRIVATE relic_core)

enable_testing()

add_executable(relic_tests tests/unit/main.cpp)
target_link_libraries(relic_tests PRIVATE relic_core)
add_test(NAME unit COMMAND relic_tests)

file(GLOB GOLDEN_TESTS ${CMAKE_SOURCE_DIR}/tests/golden/*.relic)
foreach(t ${GOLDEN_TESTS})
  get_filename_component(name ${t} NAME_WE)
  add_test(NAME golden_${name}
    COMMAND ${CMAKE_COMMAND}
      -DRELIC=$<TARGET_FILE:relic>
      -DTEST=${t}
      -P ${CMAKE_SOURCE_DIR}/tools/run_golden.cmake)
endforeach()
