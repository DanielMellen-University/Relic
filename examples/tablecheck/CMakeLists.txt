cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(tablecheck LANGUAGES C)
  enable_testing()
  # A standalone consumer build: no host and no compiler golden corpus.
  add_subdirectory(../.. relic)
endif()
relic_add_program(tablecheck main.relic)
relic_add_program(tablecheck_type_error tests/type_error.relic)
add_test(NAME tablecheck_goldens
  COMMAND ${CMAKE_COMMAND} -DAPP=${CMAKE_CURRENT_SOURCE_DIR}
    -DEXE=$<TARGET_FILE:tablecheck> -DTYPE_EXE=$<TARGET_FILE:tablecheck_type_error>
    -DRELICC=$<TARGET_FILE:relicc> -DTMP=${CMAKE_CURRENT_BINARY_DIR}/checks
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check.cmake)
