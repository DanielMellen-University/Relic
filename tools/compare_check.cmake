# SH4: selfhost/check.relic must print what `relic check` prints, and exit with
# the same code, for every program in tests/check_cases/ (hand-written to cover
# the scope rules) and, with -DCORPUS=1, every .relic file the other
# comparisons read: the goldens and their helper modules, std/, selfhost/,
# examples/ and bench/.
#   cmake -DRELIC=path/to/relic -DSRC=source/dir [-DCORPUS=1] -P tools/compare_check.cmake
file(GLOB files ${SRC}/tests/check_cases/*.relic)
if(CORPUS)
  file(GLOB more ${SRC}/tests/golden/*.relic ${SRC}/tests/golden/modules/*.relic ${SRC}/std/*.relic
    ${SRC}/selfhost/*.relic ${SRC}/examples/*.relic ${SRC}/examples/*/*.relic ${SRC}/bench/*.relic)
  list(APPEND files ${more})
endif()
set(bad 0)
set(total 0)
foreach(f ${files})
  get_filename_component(dir ${f} DIRECTORY)
  get_filename_component(name ${f} NAME)
  math(EXPR total "${total}+1")
  execute_process(COMMAND ${RELIC} check ${name} WORKING_DIRECTORY ${dir}
    OUTPUT_VARIABLE a_out ERROR_VARIABLE a_err RESULT_VARIABLE a_code)
  execute_process(COMMAND ${RELIC} ${SRC}/selfhost/check.relic ${name} WORKING_DIRECTORY ${dir}
    OUTPUT_VARIABLE b_out ERROR_VARIABLE b_err RESULT_VARIABLE b_code)
  foreach(v a_out a_err b_out b_err)
    string(REPLACE "\r\n" "\n" ${v} "${${v}}")
  endforeach()
  if(NOT a_out STREQUAL b_out OR NOT a_err STREQUAL b_err OR NOT a_code STREQUAL b_code)
    message("mismatch: ${name}\n--- host (exit ${a_code})\n${a_out}${a_err}--- relic (exit ${b_code})\n${b_out}${b_err}")
    math(EXPR bad "${bad}+1")
  endif()
endforeach()
if(bad GREATER 0)
  message(FATAL_ERROR "${bad} of ${total} files differ")
endif()
message("all ${total} files match")
