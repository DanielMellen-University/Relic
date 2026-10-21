# Rewrites bootstrap/cgen.c from the current sources.
#   cmake -DBUILD=build -P tools/regen_bootstrap.cmake [-DCONFIG=Release]
# BUILD is a configured build folder of this checkout. Each round builds the
# stage 1 compiler (relicc, from the committed C) and has it compile
# selfhost/cgen.relic. If that C differs from the committed file, it is
# written and the round repeats, so a change to how cgen writes C reaches
# the committed file too. It stops when a round changes nothing.
# With the host built (-DRELIC_BUILD_HOST=ON) this also works in one step:
#   relic selfhost/cgen.relic selfhost/cgen.relic --root . -o out.c
# then put the header from tools/bootstrap.cmake on top.
cmake_minimum_required(VERSION 3.16)
include(${CMAKE_CURRENT_LIST_DIR}/bootstrap.cmake)
get_filename_component(SRC ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
if(NOT BUILD)
  message(FATAL_ERROR "usage: cmake -DBUILD=build -P tools/regen_bootstrap.cmake")
endif()
get_filename_component(BUILD ${BUILD} ABSOLUTE)
if(NOT CONFIG)
  set(CONFIG Release)
endif()
set(out ${BUILD}/bootstrap/regen.c)
foreach(round 1 2 3 4 5)
  execute_process(COMMAND ${CMAKE_COMMAND} --build ${BUILD} --config ${CONFIG} --target relicc
    RESULT_VARIABLE code)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "building relicc failed; if the committed C cannot build the new cgen, regenerate it with the host (see the top of this file)")
  endif()
  file(GLOB_RECURSE exes ${BUILD}/relicc*)
  set(tool "")
  foreach(e ${exes})
    get_filename_component(n ${e} NAME)
    if(n STREQUAL "relicc" OR n STREQUAL "relicc.exe")
      set(tool ${e})
    endif()
  endforeach()
  if(NOT tool)
    message(FATAL_ERROR "relicc not found in ${BUILD}")
  endif()
  execute_process(COMMAND ${tool} ${SRC}/selfhost/cgen.relic --root ${SRC} -o ${out}
    RESULT_VARIABLE code ERROR_VARIABLE err)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "relicc exited with ${code}\n${err}")
  endif()
  relic_read_lf(${out} got)
  relic_read_lf(${SRC}/bootstrap/cgen.c have)
  set(want "${RELIC_BOOTSTRAP_HEADER}${got}")
  if(have STREQUAL want)
    message("bootstrap/cgen.c is up to date (round ${round})")
    return()
  endif()
  file(WRITE ${SRC}/bootstrap/cgen.c "${want}")
  message("round ${round}: bootstrap/cgen.c rewritten")
endforeach()
message(FATAL_ERROR "bootstrap/cgen.c still changes after 5 rounds")
