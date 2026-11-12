# Verify an imported-fixture skip cannot hide a failed main-input check.
cmake_minimum_required(VERSION 3.16)

execute_process(COMMAND "${CMAKE_COMMAND}"
  "-DRELIC=${RELIC}" "-DSRC=${SRC}" "-DDRIVER=cgen" "-DTOOL=${CMAKE_COMMAND}"
  "-DFORCE_IMPORT_SKIP=1" "-DTMP=${TMP}" -P "${SRC}/tools/check_unreadable_input.cmake"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
string(REPLACE "unreadable-skip:" "unreadable-skip (failure):" failure_output "${out}${err}")
if("${out}${err}" MATCHES "unreadable-skip:" AND
   NOT "${out}${err}" MATCHES "compiled: wanted exit 3")
  message("unreadable-skip: ${out}${err}")
  return()
endif()
if(got STREQUAL "0" OR NOT "${out}${err}" MATCHES "compiled: wanted exit 3")
  message(FATAL_ERROR "the main-input failure was hidden by the imported-fixture skip\n${failure_output}")
endif()
if("${out}${err}" MATCHES "unreadable-skip:")
  message(FATAL_ERROR "reported a skip after recording a main-input failure\n${failure_output}")
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
    string(REPLACE "unreadable-skip:" "unreadable-skip (failure):" failure_output "${out}${err}")
    message(FATAL_ERROR "${label}: input/import fixture was not readable after run\n${failure_output}")
  endif()
endfunction()
check_restored("main-input failure")

execute_process(COMMAND "${CMAKE_COMMAND}"
  "-DRELIC=${RELIC}" "-DSRC=${SRC}" "-DDRIVER=cgen" "-DTMP=${TMP}"
  -P "${SRC}/tools/check_unreadable_input.cmake"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
if(NOT got STREQUAL "0")
  string(REPLACE "unreadable-skip:" "unreadable-skip (failure):" failure_output "${out}${err}")
  message(FATAL_ERROR "normal cgen unreadable-input check failed\n${failure_output}")
endif()
check_restored("normal pass")

execute_process(COMMAND "${CMAKE_COMMAND}"
  "-DRELIC=${RELIC}" "-DSRC=${SRC}" "-DDRIVER=cgen" "-DFORCE_IMPORT_FAILURE=1"
  "-DTMP=${TMP}" -P "${SRC}/tools/check_unreadable_input.cmake"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
if(got STREQUAL "0" OR NOT "${out}${err}" MATCHES "forced imported-fixture failure")
  string(REPLACE "unreadable-skip:" "unreadable-skip (failure):" failure_output "${out}${err}")
  message(FATAL_ERROR "forced imported-fixture failure was not reported\n${failure_output}")
endif()
check_restored("forced import failure")

set(marker_project "${TMP}/marker_project")
set(marker_build "${TMP}/marker_build")
file(MAKE_DIRECTORY "${marker_project}")
file(WRITE "${marker_project}/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.16)\nproject(marker_failure NONE)\nenable_testing()\n"
  "add_test(NAME marker_failure COMMAND \"${CMAKE_COMMAND}\" -P \"${marker_project}/fail.cmake\")\n"
  "set_tests_properties(marker_failure PROPERTIES SKIP_REGULAR_EXPRESSION \"unreadable-skip:\")\n")
file(WRITE "${marker_project}/fail.cmake"
  "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E echo \"unreadable-skip: forced failure\" OUTPUT_VARIABLE raw ERROR_VARIABLE raw_err)\n"
  "string(REPLACE \"unreadable-skip:\" \"unreadable-skip (failure):\" diagnostic \"\${raw}\${raw_err}\")\n"
  "message(FATAL_ERROR \"forced failure output was: \\n\${diagnostic}\")\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${marker_project}" -B "${marker_build}"
  OUTPUT_VARIABLE configure_out ERROR_VARIABLE configure_err RESULT_VARIABLE configure_code)
if(NOT configure_code STREQUAL "0")
  message(FATAL_ERROR "could not configure marker classification check\n${configure_out}${configure_err}")
endif()
execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${marker_build}" --output-on-failure
  OUTPUT_VARIABLE marker_out ERROR_VARIABLE marker_err RESULT_VARIABLE marker_code)
if(marker_code STREQUAL "0" OR NOT "${marker_out}${marker_err}" MATCHES "Failed" OR
   "${marker_out}${marker_err}" MATCHES "Skipped")
  message(FATAL_ERROR "CTest did not classify a marked forced failure as Failed\n${marker_out}${marker_err}")
endif()
message("unreadable fixtures are restored after passes and failures")
