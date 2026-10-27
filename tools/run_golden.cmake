# Golden test runner. Usage:
#   cmake -DRELIC=path/to/relic -DTEST=tests/golden/name.relic -P tools/run_golden.cmake
# Optional name.args holds flags to pass first.
# Optional name.in is fed to the program on stdin (a file without it gets no input).
# Files named repl_*.relic are fed to `relic` on stdin instead; stdout is
# compared with .out and stderr with .err (both optional, empty if missing).
# Compares stdout with name.out, or stderr with name.err (and exit code 1) for error tests.
get_filename_component(dir ${TEST} DIRECTORY)
get_filename_component(name ${TEST} NAME_WE)
# Optional name.args: extra flags placed before the file, e.g. --tokens
set(args "")
if(EXISTS ${dir}/${name}.args)
  file(READ ${dir}/${name}.args args)
  string(STRIP "${args}" args)
  separate_arguments(args)
endif()
# Without name.in stdin is empty (an empty file), never the terminal running ctest.
set(stdin_file ${dir}/${name}.in)
if(NOT EXISTS ${stdin_file})
  set(stdin_file ${CMAKE_CURRENT_LIST_DIR}/empty.in)
endif()
string(REGEX MATCH "^repl_" is_repl "${name}")
if(is_repl)
  # REPL tests: the .relic file is typed into `relic` on stdin.
  execute_process(
    COMMAND ${RELIC}
    INPUT_FILE ${TEST}
    WORKING_DIRECTORY ${dir}
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE code)
else()
  execute_process(
    COMMAND ${RELIC} ${args} ${TEST}
    INPUT_FILE ${stdin_file}
    WORKING_DIRECTORY ${dir}
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE code)
endif()
string(REPLACE "\r\n" "\n" out "${out}")
string(REPLACE "\r\n" "\n" err "${err}")

if(is_repl)
  set(want_out "")
  set(want_err "")
  if(EXISTS ${dir}/${name}.out)
    file(READ ${dir}/${name}.out want_out)
  endif()
  if(EXISTS ${dir}/${name}.err)
    file(READ ${dir}/${name}.err want_err)
  endif()
  string(REPLACE "\r\n" "\n" want_out "${want_out}")
  string(REPLACE "\r\n" "\n" want_err "${want_err}")
  if(NOT out STREQUAL want_out)
    message(FATAL_ERROR "${name}: stdout mismatch\n--- want\n${want_out}--- got\n${out}")
  endif()
  if(NOT err STREQUAL want_err)
    message(FATAL_ERROR "${name}: stderr mismatch\n--- want\n${want_err}--- got\n${err}")
  endif()
  return()
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
  if(NOT err STREQUAL want)
    message(FATAL_ERROR "${name}: stderr mismatch\n--- want\n${want}--- got\n${err}")
  endif()
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
