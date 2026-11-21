# --- fault injection: fmt --write must keep the original on failure ---
# Linux only: the injector interposes libc calls with LD_PRELOAD, and the
# build needs a C compiler. Elsewhere this section is skipped, not failed.
find_program(fmt_cc NAMES cc gcc clang)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND fmt_cc)
  execute_process(COMMAND ${fmt_cc} -shared -fPIC -o ${TMP}/failio.so ${SRC}/tests/fmt/failio.c -ldl
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "building the fmt fault injector failed: ${err}")
  endif()
  # One fault case: run fmt --write on a victim that needs reformatting and
  # check the exit code, the diagnostic, the untouched victim, and that no
  # temp file is left behind.
  function(fmt_fault label fault_env)
    file(WRITE ${TMP}/victim.relic "print( 1 )\n")
    execute_process(COMMAND ${CMAKE_COMMAND} -E env LD_PRELOAD=${TMP}/failio.so ${fault_env} ${FMT_COMMAND} --write ${TMP}/victim.relic
      RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    file(READ ${TMP}/victim.relic kept)
    file(GLOB leftover "${TMP}/*.fmt-tmp*")
    if(NOT code EQUAL 2 OR NOT out STREQUAL "" OR NOT err STREQUAL "relic: cannot write ${TMP}/victim.relic\n" OR NOT kept STREQUAL "print( 1 )\n" OR leftover)
      message(FATAL_ERROR "fmt fault ${label}: exit ${code}, stdout '${out}', stderr '${err}', victim '${kept}', leftover '${leftover}'")
    endif()
  endfunction()
  foreach(fault ${FMT_FAULTS})
    string(TOLOWER "${fault}" label)
    fmt_fault("${label}" "FAIL_FMT_${fault}=1")
  endforeach()
  # Control: with no fault the same write lands.
  file(WRITE ${TMP}/victim.relic "print( 1 )\n")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env LD_PRELOAD=${TMP}/failio.so ${FMT_COMMAND} --write ${TMP}/victim.relic
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  file(READ ${TMP}/victim.relic wrote)
  if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "" OR NOT wrote STREQUAL "print(1)\n")
    message(FATAL_ERROR "fmt fault control: exit ${code}, stdout '${out}', stderr '${err}', victim '${wrote}'")
  endif()
  message("fmt: fault injection passed (write, rename, close failures keep the original)")
else()
  message("fmt: fault injection skipped (needs Linux and a C compiler)")
endif()
