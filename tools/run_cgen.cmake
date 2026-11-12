cmake_policy(SET CMP0053 NEW)
include(${CMAKE_CURRENT_LIST_DIR}/error_bytes.cmake)
# Runs one golden program compiled to C by selfhost/cgen.relic. Usage:
#   cmake -DEXE=path/to/cgen_name -DTEST=tests/golden/name.relic -P tools/run_cgen.cmake
# Same checks as run_golden.cmake: stdout against name.out, or stderr against
# name.err (with exit code 1) for error tests. RELIC_LEAK_CHECK=1 makes the program exit 3 if any
# value is still alive at the end. A program cgen could not compile prints
# "cgen-skip: ..." and the test is reported as skipped.
get_filename_component(dir ${TEST} DIRECTORY)
get_filename_component(name ${TEST} NAME_WE)
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef error_id)
set(stderr_file "${CMAKE_CURRENT_BINARY_DIR}/${name}.${error_id}.stderr")
# name.in is the program's stdin; without it stdin is empty.
set(stdin_file ${dir}/${name}.in)
if(NOT EXISTS ${stdin_file})
  set(stdin_file ${CMAKE_CURRENT_LIST_DIR}/empty.in)
endif()
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env RELIC_LEAK_CHECK=1 ${EXE}
  INPUT_FILE ${stdin_file}
  WORKING_DIRECTORY ${dir}
  OUTPUT_VARIABLE out
  ERROR_FILE "${stderr_file}"
  RESULT_VARIABLE code)
file(READ "${stderr_file}" err)
file(READ "${stderr_file}" err_hex HEX)
file(REMOVE "${stderr_file}")
string(REPLACE "\r\n" "\n" out "${out}")
string(REPLACE "\r\n" "\n" err "${err}")
if(out MATCHES "^cgen-skip: ")
  message("${out}")
  return()
endif()
if(code EQUAL 3 AND err MATCHES "leak: ")
  message(FATAL_ERROR "${name}: leaked values\n${err}")
endif()
if(EXISTS ${dir}/${name}.err)
  file(READ ${dir}/${name}.err want)
  string(REPLACE "\r\n" "\n" want "${want}")
  file(TO_CMAKE_PATH "${dir}" dir_fwd)
  file(TO_NATIVE_PATH "${dir}" dir_native)
  string(REPLACE "${dir}/" "" err "${err}")
  string(REPLACE "${dir_fwd}/" "" err "${err}")
  string(REPLACE "${dir_native}\\" "" err "${err}")
  if(NOT code EQUAL 1)
    message(FATAL_ERROR "${name}: expected an error and exit 1, got exit ${code}")
  endif()
  check_error_bytes("${err_hex}" "${dir}/${name}.err" "${dir}" "${name}")
else()
  set(want "")
  if(EXISTS ${dir}/${name}.out)
    file(READ ${dir}/${name}.out want)
    string(REPLACE "\r\n" "\n" want "${want}")
  endif()
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "${name}: exit ${code}\n${err}")
  endif()
  if(NOT out STREQUAL want)
    message(FATAL_ERROR "${name}: stdout mismatch\n--- want\n${want}--- got\n${out}")
  endif()
endif()
