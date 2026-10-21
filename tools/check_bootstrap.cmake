# Checks that bootstrap/cgen.c is what cgen writes for selfhost/cgen.relic
# from the current sources (after the header in tools/bootstrap.cmake).
#   cmake -DTOOL=exe [-DSCRIPT=selfhost/cgen.relic] -DSRC=source/dir -DOUT=file.c -P tools/check_bootstrap.cmake
# TOOL is a compiled cgen (stage 0 or 1), or with SCRIPT the host `relic`
# running cgen.relic, for the host cross-check.
cmake_minimum_required(VERSION 3.16)
include(${CMAKE_CURRENT_LIST_DIR}/bootstrap.cmake)
execute_process(
  COMMAND ${TOOL} ${SCRIPT} ${SRC}/selfhost/cgen.relic --root ${SRC} -o ${OUT}
  RESULT_VARIABLE code ERROR_VARIABLE err)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "cgen exited with ${code}\n${err}")
endif()
relic_read_lf(${OUT} got)
relic_read_lf(${SRC}/bootstrap/cgen.c have)
if(NOT have STREQUAL "${RELIC_BOOTSTRAP_HEADER}${got}")
  message(FATAL_ERROR "bootstrap/cgen.c is out of date: compare it with ${OUT}\n"
    "Regenerate it with: cmake -DBUILD=<build dir> -P tools/regen_bootstrap.cmake")
endif()
message("bootstrap/cgen.c matches what cgen writes now")
