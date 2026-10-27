# SH4: selfhost/check.relic must print what `relic check` prints, and exit with
# the same code, for every program in tests/check_cases/ (hand-written to cover
# the scope rules) and, with -DCORPUS=1, every .relic file the other
# comparisons read: the goldens and their helper modules, std/, selfhost/,
# examples/ and bench/. Also checks a few runs with several files, some missing.
#   cmake -DRELIC=path/to/relic -DSRC=source/dir [-DCORPUS=1] [-DTOOL=path] -P tools/compare_check.cmake
# With -DTOOL=path, that program (selfhost/check.relic compiled to C) is checked
# instead of running selfhost/check.relic on the host.
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
  if(TOOL)
    set(mine ${TOOL})
  else()
    set(mine ${RELIC} ${SRC}/selfhost/check.relic)
  endif()
  execute_process(COMMAND ${mine} ${name} WORKING_DIRECTORY ${dir}
    OUTPUT_VARIABLE b_out ERROR_VARIABLE b_err RESULT_VARIABLE b_code)
  foreach(v a_out a_err b_out b_err)
    string(REPLACE "\r\n" "\n" ${v} "${${v}}")
  endforeach()
  if(NOT a_out STREQUAL b_out OR NOT a_err STREQUAL b_err OR NOT a_code STREQUAL b_code)
    message("mismatch: ${name}\n--- host (exit ${a_code})\n${a_out}${a_err}--- relic (exit ${b_code})\n${b_out}${b_err}")
    math(EXPR bad "${bad}+1")
  endif()
endforeach()
# Several files in one run, a file that cannot be opened (first, last, between),
# and a clean file next to a bad one: the exit code is the first nonzero one,
# except that a missing file always gives 2.
set(cases ${SRC}/tests/check_cases)
set(combos
  "clean.relic|undefined.relic"
  "undefined.relic|clean.relic"
  "nofile.relic|undefined.relic"
  "undefined.relic|nofile.relic"
  "undefined.relic|nofile.relic|syntax_error.relic"
  "syntax_error.relic|undefined.relic"
  "nofile.relic|clean.relic"
  "clean.relic|clean.relic")
foreach(combo ${combos})
  string(REPLACE "|" ";" argv "${combo}")
  math(EXPR total "${total}+1")
  execute_process(COMMAND ${RELIC} check ${argv} WORKING_DIRECTORY ${cases}
    OUTPUT_VARIABLE a_out ERROR_VARIABLE a_err RESULT_VARIABLE a_code)
  if(TOOL)
    set(mine ${TOOL})
  else()
    set(mine ${RELIC} ${SRC}/selfhost/check.relic)
  endif()
  execute_process(COMMAND ${mine} ${argv} WORKING_DIRECTORY ${cases}
    OUTPUT_VARIABLE b_out ERROR_VARIABLE b_err RESULT_VARIABLE b_code)
  foreach(v a_out a_err b_out b_err)
    string(REPLACE "\r\n" "\n" ${v} "${${v}}")
  endforeach()
  if(NOT a_out STREQUAL b_out OR NOT a_err STREQUAL b_err OR NOT a_code STREQUAL b_code)
    message("mismatch: ${combo}\n--- host (exit ${a_code})\n${a_out}${a_err}--- relic (exit ${b_code})\n${b_out}${b_err}")
    math(EXPR bad "${bad}+1")
  endif()
endforeach()
if(bad GREATER 0)
  message(FATAL_ERROR "${bad} of ${total} runs differ")
endif()
message("all ${total} runs match")
