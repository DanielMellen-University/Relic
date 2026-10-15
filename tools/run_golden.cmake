# Golden test runner. Usage:
#   cmake -DRELIC=path/to/relic -DTEST=tests/golden/name.relic -P tools/run_golden.cmake
# Compares stdout with name.out, or stderr with name.err for error tests.
get_filename_component(dir ${TEST} DIRECTORY)
get_filename_component(name ${TEST} NAME_WE)
execute_process(
  COMMAND ${RELIC} ${TEST}
  WORKING_DIRECTORY ${dir}
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err
  RESULT_VARIABLE code)
string(REPLACE "\r\n" "\n" out "${out}")
string(REPLACE "\r\n" "\n" err "${err}")

if(EXISTS ${dir}/${name}.err)
  file(READ ${dir}/${name}.err want)
  string(REPLACE "\r\n" "\n" want "${want}")
  string(REPLACE "${dir}/" "" err "${err}")
  if(code EQUAL 0)
    message(FATAL_ERROR "${name}: expected an error, got exit 0")
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
