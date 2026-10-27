# SH4: selfhost/repl.relic must behave like the host's REPL, input for input:
# the same stdout, the same stderr and the same exit code for every REPL script
# there is: the golden tests tests/golden/repl_*.relic and the cases in
# tests/repl_cases/*.in (values, state, errors, blocks, multi-line headers, :quit,
# input that ends in the middle of a block, CRLF, a program reading the same stdin,
# `use`, os.exit). Each script is fed on stdin, in tests/golden (so `use modules/x` works).
#   cmake -DRELIC=path/to/relic -DSRC=source/dir [-DTOOL=path] [-DNESTED=1 -DONLY="a|b"] -P tools/compare_repl.cmake
# With -DTOOL=path, that program (selfhost/repl.relic compiled to C) is checked
# instead of running selfhost/repl.relic on the host.
# With -DNESTED=1, selfhost/repl.relic runs on selfhost/run.relic (the interpreter
# written in Relic), which is slow, so -DONLY="a|b" limits the cases to those names.
file(GLOB scripts ${SRC}/tests/golden/repl_*.relic ${SRC}/tests/repl_cases/*.in)
if(ONLY)
  string(REPLACE "|" ";" only "${ONLY}")
  set(picked "")
  foreach(s ${scripts})
    get_filename_component(n ${s} NAME_WE)
    list(FIND only ${n} at)
    if(NOT at EQUAL -1)
      list(APPEND picked ${s})
    endif()
  endforeach()
  set(scripts ${picked})
endif()
if(TOOL)
  set(mine ${TOOL})
elseif(NESTED)
  set(mine ${RELIC} ${SRC}/selfhost/run.relic ${SRC}/selfhost/repl.relic)
else()
  set(mine ${RELIC} ${SRC}/selfhost/repl.relic)
endif()
set(bad 0)
set(total 0)
foreach(s ${scripts})
  get_filename_component(n ${s} NAME_WE)
  math(EXPR total "${total}+1")
  execute_process(COMMAND ${RELIC} INPUT_FILE ${s} WORKING_DIRECTORY ${SRC}/tests/golden
    OUTPUT_VARIABLE a_out ERROR_VARIABLE a_err RESULT_VARIABLE a_code)
  execute_process(COMMAND ${mine} INPUT_FILE ${s} WORKING_DIRECTORY ${SRC}/tests/golden
    OUTPUT_VARIABLE b_out ERROR_VARIABLE b_err RESULT_VARIABLE b_code)
  foreach(v a_out a_err b_out b_err)
    string(REPLACE "\r\n" "\n" ${v} "${${v}}")
  endforeach()
  if(NOT a_out STREQUAL b_out OR NOT a_err STREQUAL b_err OR NOT a_code STREQUAL b_code)
    message("mismatch: ${n}\n--- host (exit ${a_code})\n${a_out}${a_err}--- relic (exit ${b_code})\n${b_out}${b_err}")
    math(EXPR bad "${bad}+1")
  endif()
endforeach()
if(total EQUAL 0)
  message(FATAL_ERROR "no REPL scripts found")
endif()
if(bad GREATER 0)
  message(FATAL_ERROR "${bad} of ${total} scripts differ")
endif()
message("all ${total} scripts match")
