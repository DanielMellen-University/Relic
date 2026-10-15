# Self-host check (M9): the Relic lexer in selfhost/ must print exactly what
# `relic --tokens` prints, for every golden .relic file (errors included).
#   cmake -DRELIC=path/to/relic -DSRC=source/dir -P tools/compare_lexers.cmake
file(GLOB files ${SRC}/tests/golden/*.relic ${SRC}/tests/golden/modules/*.relic ${SRC}/std/*.relic ${SRC}/selfhost/*.relic)
set(bad 0)
foreach(f ${files})
  execute_process(COMMAND ${RELIC} --tokens ${f} OUTPUT_VARIABLE a_out ERROR_VARIABLE a_err)
  execute_process(COMMAND ${RELIC} ${SRC}/selfhost/tokens.relic ${f} OUTPUT_VARIABLE b_out ERROR_VARIABLE b_err)
  if(NOT a_out STREQUAL b_out OR NOT a_err STREQUAL b_err)
    get_filename_component(n ${f} NAME)
    message("mismatch: ${n}\n--- host\n${a_out}${a_err}--- relic\n${b_out}${b_err}")
    math(EXPR bad "${bad}+1")
  endif()
endforeach()
list(LENGTH files total)
if(bad GREATER 0)
  message(FATAL_ERROR "${bad} of ${total} files differ")
endif()
message("all ${total} files match")
