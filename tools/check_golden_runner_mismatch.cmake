# The Relic golden runner must fail a known-bad stdout, and say so.
#   cmake -DRUNNER=build/golden_runner -DEXE=build/cgen_hello -DTMP=dir -P tools/check_golden_runner_mismatch.cmake
# EXE is a compiled program that prints "hello\n". The fixture says otherwise.
file(REMOVE_RECURSE ${TMP})
file(MAKE_DIRECTORY ${TMP})
file(WRITE ${TMP}/bad.relic "print(\"hello\")\n")
file(WRITE ${TMP}/bad.out "goodbye\n")
execute_process(
  COMMAND ${RUNNER} --cgen ${EXE} ${TMP}/bad.relic
  RESULT_VARIABLE code
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)
if(code EQUAL 0)
  message(FATAL_ERROR "golden runner exited 0 on a stdout mismatch\n${out}${err}")
endif()
if(NOT err MATCHES "stdout mismatch")
  message(FATAL_ERROR "golden runner exit ${code} did not report a stdout mismatch:\n${out}${err}")
endif()
message("golden runner rejected a stdout mismatch (exit ${code})")
