cmake_minimum_required(VERSION 3.16)
project(relic VERSION 1.0.0 LANGUAGES CXX)

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
  src/interp/value.cpp
  src/interp/interp.cpp
  src/builtins/builtins.cpp
  src/vm/vm.cpp
)
target_include_directories(relic_core PUBLIC src ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(relic_core PRIVATE RELIC_STD_DIR="${CMAKE_SOURCE_DIR}/std/")

add_executable(relic src/main.cpp)
target_link_libraries(relic PRIVATE relic_core)
if(MSVC)
  # The tree-walker recurses once per Relic call; the 1 MB Windows default is
  # too small for the 800-call limit. Match the 8 MB Linux/macOS default.
  target_link_options(relic PRIVATE /STACK:8388608)
endif()

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

add_test(NAME selfhost_lexer
  COMMAND ${CMAKE_COMMAND} -DRELIC=$<TARGET_FILE:relic> -DSRC=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/tools/compare_lexers.cmake)

add_test(NAME selfhost_parser
  COMMAND ${CMAKE_COMMAND} -DRELIC=$<TARGET_FILE:relic> -DSRC=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/tools/compare_parsers.cmake)

add_test(NAME selfhost_interp
  COMMAND ${CMAKE_COMMAND} -DRELIC=$<TARGET_FILE:relic> -DSRC=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/tools/compare_interp.cmake)

add_test(NAME vm_matches_tree_walker
  COMMAND ${CMAKE_COMMAND} -DRELIC=$<TARGET_FILE:relic> -DSRC=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/tools/compare_vm.cmake)
