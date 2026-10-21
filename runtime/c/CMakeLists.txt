# The C runtime for compiled Relic (milestone C1). Plain C99.
# Built as part of the main project, or on its own:
#   cmake -S runtime/c -B build-c && cmake --build build-c && ctest --test-dir build-c
cmake_minimum_required(VERSION 3.16)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(relic_c_runtime LANGUAGES C)
  enable_testing()
else()
  enable_language(C)
endif()
get_filename_component(RELIC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)

option(RELIC_C_WERROR "Treat C runtime warnings as errors" OFF)
option(RELIC_C_SANITIZE "Build the C runtime with ASan and UBSan (gcc/clang)" OFF)

set(RL_SOURCES rl_core.c rl_ops.c rl_builtins.c rl_host.c rl_gfx_stub.c)
add_library(relic_rt STATIC ${RL_SOURCES})
target_include_directories(relic_rt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(relic_rt PRIVATE RL_STD_DIR="${RELIC_ROOT}/std/")

# MSVC has no C99 switch; C11 mode is the closest and takes C99 code.
if(MSVC)
  set(RL_C_STANDARD 11)
else()
  set(RL_C_STANDARD 99)
endif()

function(relic_c_target t)
  set_target_properties(${t} PROPERTIES C_STANDARD ${RL_C_STANDARD} C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options(${t} PRIVATE /W4)
    target_compile_definitions(${t} PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(RELIC_C_WERROR)
      target_compile_options(${t} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${t} PRIVATE -Wall -Wextra -pedantic)
    if(RELIC_C_WERROR)
      target_compile_options(${t} PRIVATE -Werror)
    endif()
    if(RELIC_C_SANITIZE)
      target_compile_options(${t} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${t} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endfunction()

relic_c_target(relic_rt)
if(NOT WIN32)
  target_link_libraries(relic_rt PUBLIC m)
endif()

add_executable(relic_rt_tests tests/test_runtime.c)
target_link_libraries(relic_rt_tests PRIVATE relic_rt)
relic_c_target(relic_rt_tests)
add_test(NAME c_runtime COMMAND relic_rt_tests WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(relic_rt_demo tests/demo.c)
target_link_libraries(relic_rt_demo PRIVATE relic_rt)
relic_c_target(relic_rt_demo)
add_test(NAME c_demo
  COMMAND ${CMAKE_COMMAND}
    -DEXE=$<TARGET_FILE:relic_rt_demo>
    -DEXPECTED=${RELIC_ROOT}/tests/golden/c_demo.out
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_output.cmake)
