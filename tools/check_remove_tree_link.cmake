# files.remove_tree removes a link, not what it points at (since 1.0.52).
#   cmake -DEXE=path/to/relic-or-compiled-program [-DPROGRAM=file.relic] -DTMP=dir -P tools/check_remove_tree_link.cmake
# EXE is the host relic (with PROGRAM, tests/links/remove_tree_link.relic) or
# that program compiled to C. Skipped where links cannot be made.
file(REMOVE_RECURSE ${TMP})
file(MAKE_DIRECTORY ${TMP}/outside ${TMP}/t/sub)
file(WRITE ${TMP}/outside/keep.txt "keep\n")
file(WRITE ${TMP}/t/sub/a.txt "a\n")
file(WRITE ${TMP}/t/b.txt "b\n")
file(CREATE_LINK ${TMP}/outside ${TMP}/t/link RESULT r SYMBOLIC)
if(NOT r EQUAL 0)
  message("links cannot be made here (${r}), skipped")
  return()
endif()
file(CREATE_LINK ${TMP}/nowhere ${TMP}/t/dangling RESULT r SYMBOLIC)
if(NOT r EQUAL 0)
  message("links cannot be made here (${r}), skipped")
  return()
endif()
execute_process(COMMAND ${EXE} ${PROGRAM} WORKING_DIRECTORY ${TMP}
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code)
string(REPLACE "\r\n" "\n" out "${out}")
if(NOT code EQUAL 0 OR NOT out STREQUAL "true false\ndone\n")
  message(FATAL_ERROR "remove_tree with links: exit ${code}\n${out}${err}")
endif()
if(NOT EXISTS ${TMP}/outside/keep.txt)
  message(FATAL_ERROR "remove_tree went through a link and removed outside/keep.txt")
endif()
message("remove_tree removed the links, not what they point at")
