# Runs the examples in docs/GUIDE.md and checks the output shown after each.
#   cmake -DRELIC=path/to/relic -DSRC=source/dir -DTMP=dir -P tools/check_guide.cmake
# tools/doc_blocks.relic splits the guide into its ``` blocks. Each block is
# run as a program in TMP; then
#   - one that prints something must be followed by a block of exactly that output,
#   - one whose next block starts "NAME.relic:" is run as NAME.relic and must fail
#     with that block as its error,
#   - one whose first line is "# PATH.relic" is a module, written to TMP/PATH.relic.
# Anything else (an error, or no output) fails the test.
file(REMOVE_RECURSE ${TMP})
file(MAKE_DIRECTORY ${TMP})
execute_process(COMMAND ${RELIC} ${SRC}/tools/doc_blocks.relic ${SRC}/docs/GUIDE.md ${TMP}/blocks
  OUTPUT_VARIABLE count ERROR_VARIABLE err RESULT_VARIABLE code)
string(STRIP "${count}" count)
if(NOT code EQUAL 0 OR count LESS 2)
  message(FATAL_ERROR "doc_blocks.relic failed (exit ${code})\n${count}${err}")
endif()
set(i 0)
set(examples 0)
while(i LESS count)
  file(READ ${TMP}/blocks/block_${i}.txt code_text)
  math(EXPR next "${i} + 1")
  set(next_text "")
  if(next LESS count)
    file(READ ${TMP}/blocks/block_${next}.txt next_text)
  endif()
  if(code_text MATCHES "^# ([A-Za-z0-9_/]+\\.relic)\n")
    file(WRITE ${TMP}/${CMAKE_MATCH_1} "${code_text}")
    math(EXPR i "${i} + 1")
    continue()
  endif()
  if(next_text MATCHES "^([A-Za-z0-9_]+\\.relic):")
    set(name ${CMAKE_MATCH_1})
    file(WRITE ${TMP}/${name} "${code_text}")
    execute_process(COMMAND ${RELIC} ${name} WORKING_DIRECTORY ${TMP} INPUT_FILE ${SRC}/tools/empty.in
      OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code)
    string(REPLACE "\r\n" "\n" err "${err}")
    if(NOT code EQUAL 1 OR NOT err STREQUAL next_text)
      message(FATAL_ERROR "GUIDE.md block ${i} (as ${name}): want exit 1 and\n${next_text}got exit ${code} and\n${err}")
    endif()
  else()
    file(WRITE ${TMP}/example_${i}.relic "${code_text}")
    execute_process(COMMAND ${RELIC} example_${i}.relic WORKING_DIRECTORY ${TMP} INPUT_FILE ${SRC}/tools/empty.in
      OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code)
    string(REPLACE "\r\n" "\n" out "${out}")
    if(NOT code EQUAL 0 OR out STREQUAL "")
      message(FATAL_ERROR "GUIDE.md block ${i} should run and print: exit ${code}\n${code_text}${out}${err}")
    endif()
    if(NOT out STREQUAL next_text)
      message(FATAL_ERROR "GUIDE.md block ${i} prints\n${out}but the guide shows\n${next_text}")
    endif()
  endif()
  math(EXPR examples "${examples} + 1")
  math(EXPR i "${i} + 2")
endwhile()
message("${examples} GUIDE.md examples print what the guide shows")
