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
execute_process(COMMAND ${test_program} -r "${TMP}/unreadable input.relic"
  RESULT_VARIABLE readable)
if(NOT test_program OR NOT readable STREQUAL "0")
  message(FATAL_ERROR "the main-input fixture was not readable after failure\n${out}${err}")
endif()
message("recorded main-input failure takes precedence over imported-fixture skip")
