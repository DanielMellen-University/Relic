# check_fmt_isolation.cmake - proves check_fmt.cmake never writes into the
# source tree, under parallel doc_counts load and under a kill mid-run.
# Inputs: -DRELIC= -DSRC= -DTMP= (an empty scratch dir).
# Linux only: the parallel and kill tests need sh backgrounding.
cmake_minimum_required(VERSION 3.20)

if(NOT RELIC OR NOT SRC OR NOT TMP)
  message(FATAL_ERROR "check_fmt_isolation: RELIC, SRC and TMP are required")
endif()
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  message("fmt isolation: skipped (needs Linux sh backgrounding)")
  return()
endif()

# A stray formatter temp under SRC, before or after. Git status is not used
# here: other tests may run in parallel, so only the formatter's own temp
# names count.
function(check_clean label)
  file(GLOB_RECURSE stray "${SRC}/.fmt-corpus.relic" "${SRC}/*.fmt-tmp*")
  if(stray)
    message(FATAL_ERROR "fmt isolation ${label}: formatter temps under SRC: ${stray}")
  endif()
endfunction()
check_clean("before")

# --- parallel: a tight doc_counts loop while check_fmt runs ---
file(WRITE ${TMP}/parallel.sh "#!/bin/sh\nCMAKE=\"$1\"; RELIC=\"$2\"; SRC=\"$3\"; TMP=\"$4\"\nwhile true; do\n  \"$CMAKE\" -DRELIC=\"$RELIC\" -DSRC=\"$SRC\" -DTMP=\"$TMP/doc_counts\" -P \"$SRC/tools/check_doc_counts.cmake\" >> \"$TMP/doc_counts.log\" 2>&1 || echo FAIL >> \"$TMP/doc_counts.log\"\ndone &\nLOOP=$!\n\"$CMAKE\" -DRELIC=\"$RELIC\" -DSRC=\"$SRC\" -DTMP=\"$TMP/fmt\" -P \"$SRC/tools/check_fmt.cmake\" > \"$TMP/fmt.log\" 2>&1\nCODE=$?\nkill $LOOP 2>/dev/null\nwait $LOOP 2>/dev/null\nexit $CODE\n")
execute_process(COMMAND sh ${TMP}/parallel.sh ${CMAKE_COMMAND} ${RELIC} ${SRC} ${TMP}
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "fmt isolation parallel: check_fmt failed with exit ${code}: ${out} ${err}")
endif()
file(READ ${TMP}/doc_counts.log counts_log)
string(FIND "${counts_log}" "FAIL" fail_pos)
if(NOT fail_pos EQUAL -1)
  message(FATAL_ERROR "fmt isolation parallel: doc_counts failed while fmt ran:\n${counts_log}")
endif()
string(FIND "${counts_log}" "docs counts match:" docs_pos)
if(docs_pos EQUAL -1)
  message(FATAL_ERROR "fmt isolation parallel: doc_counts never ran (log empty)")
endif()
check_clean("after parallel")
message("fmt isolation: parallel doc_counts stayed green while fmt ran")

# --- kill: stop check_fmt mid-run, the source tree must stay untouched ---
file(WRITE ${TMP}/kill.sh "#!/bin/sh\nCMAKE=\"$1\"; RELIC=\"$2\"; SRC=\"$3\"; TMP=\"$4\"\n\"$CMAKE\" -DRELIC=\"$RELIC\" -DSRC=\"$SRC\" -DTMP=\"$TMP/fmt_kill\" -P \"$SRC/tools/check_fmt.cmake\" > \"$TMP/fmt_kill.log\" 2>&1 &\nFMT=$!\nsleep 5\nkill -TERM $FMT 2>/dev/null\nsleep 2\nkill -KILL $FMT 2>/dev/null\nwait $FMT 2>/dev/null\nsleep 3\nexit 0\n")
execute_process(COMMAND sh ${TMP}/kill.sh ${CMAKE_COMMAND} ${RELIC} ${SRC} ${TMP}
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "fmt isolation kill: the kill driver failed with exit ${code}")
endif()
check_clean("after kill")
message("fmt isolation: source tree untouched after a killed formatter")
