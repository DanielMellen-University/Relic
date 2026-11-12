# Verify an imported-fixture skip cannot hide a failed main-input check.
cmake_minimum_required(VERSION 3.16)

execute_process(COMMAND "${CMAKE_COMMAND}"
  "-DRELIC=${RELIC}" "-DSRC=${SRC}" "-DDRIVER=cgen" "-DTOOL=${CMAKE_COMMAND}"
  "-DFORCE_IMPORT_SKIP=1" "-DTMP=${TMP}" -P "${SRC}/tools/check_unreadable_input.cmake"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
if("${out}${err}" MATCHES "unreadable-skip:" AND
   NOT "${out}${err}" MATCHES "compiled: wanted exit 3")
  message("unreadable-skip: ${out}${err}")
  return()
endif()
if(got STREQUAL "0" OR NOT "${out}${err}" MATCHES "compiled: wanted exit 3")
  message(FATAL_ERROR "the main-input failure was hidden by the imported-fixture skip\n${out}${err}")
endif()
if("${out}${err}" MATCHES "unreadable-skip:")
  message(FATAL_ERROR "reported a skip after recording a main-input failure\n${out}${err}")
endif()
find_program(test_program test)
if(NOT test_program)
  message(FATAL_ERROR "test is needed to verify fixture restoration")
endif()

function(check_restored label)
  execute_process(COMMAND ${test_program} -r "${TMP}/unreadable input.relic"
    RESULT_VARIABLE input_readable)
  execute_process(COMMAND ${test_program} -r "${TMP}/dependency.relic"
    RESULT_VARIABLE import_readable)
  if(NOT input_readable STREQUAL "0" OR NOT import_readable STREQUAL "0")
    message(FATAL_ERROR "${label}: input/import fixture was not readable after run\n${out}${err}")
  endif()
endfunction()
check_restored("main-input failure")

execute_process(COMMAND "${CMAKE_COMMAND}"
  "-DRELIC=${RELIC}" "-DSRC=${SRC}" "-DDRIVER=cgen" "-DTMP=${TMP}"
  -P "${SRC}/tools/check_unreadable_input.cmake"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
if(NOT got STREQUAL "0")
  message(FATAL_ERROR "normal cgen unreadable-input check failed\n${out}${err}")
endif()
check_restored("normal pass")

execute_process(COMMAND "${CMAKE_COMMAND}"
  "-DRELIC=${RELIC}" "-DSRC=${SRC}" "-DDRIVER=cgen" "-DFORCE_IMPORT_FAILURE=1"
  "-DTMP=${TMP}" -P "${SRC}/tools/check_unreadable_input.cmake"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
if(got STREQUAL "0" OR NOT "${out}${err}" MATCHES "forced imported-fixture failure")
  message(FATAL_ERROR "forced imported-fixture failure was not reported\n${out}${err}")
endif()
check_restored("forced import failure")
message("unreadable fixtures are restored after passes and failures")
