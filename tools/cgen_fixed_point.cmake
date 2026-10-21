# Toward C4: cgen compiled by cgen (stage 1) must write the same C for
# selfhost/cgen.relic as cgen running on the host did (stage 0). Then
# stage 2, built from that C, is the same program as stage 1.
#   cmake -DSTAGE1=exe -DSRC=source/dir -DSTAGE0_C=file.c -DOUT=file.c -P tools/cgen_fixed_point.cmake
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env RELIC_LEAK_CHECK=1 ${STAGE1} ${SRC}/selfhost/cgen.relic -o ${OUT}
  RESULT_VARIABLE code ERROR_VARIABLE err)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "stage 1 cgen exited with ${code}\n${err}")
endif()
file(READ ${STAGE0_C} want)
file(READ ${OUT} got)
if(NOT got STREQUAL want)
  message(FATAL_ERROR "stage 1 output differs from stage 0 output: compare ${OUT} with ${STAGE0_C}")
endif()
message("stage 1 output equals stage 0 output")
