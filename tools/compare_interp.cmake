# Self-host check (M9): selfhost/run.relic must give the same stdout and stderr
# as the host for every golden program, except the ones listed below.
#   cmake -DRELIC=path/to/relic -DSRC=source/dir -P tools/compare_interp.cmake
# Skipped, with the reason:
#   repl_*         typed into the REPL, not run as files
#   rt_recursion   the host's 800-call limit is reached by the interpreter itself first
#   try_recursion  same reason
#   c_demo         same reason (it also recurses 800 deep)
#   gc_cycles      gc() counts the host heap, which holds the interpreter too
file(GLOB files ${SRC}/tests/golden/*.relic)
set(bad 0)
set(total 0)
foreach(f ${files})
  get_filename_component(n ${f} NAME_WE)
  if(n MATCHES "^repl_" OR n STREQUAL "rt_recursion" OR n STREQUAL "try_recursion" OR n STREQUAL "c_demo" OR n STREQUAL "gc_cycles")
    continue()
  endif()
  math(EXPR total "${total}+1")
  get_filename_component(dir ${f} DIRECTORY)
  execute_process(COMMAND ${RELIC} ${f} WORKING_DIRECTORY ${dir} OUTPUT_VARIABLE a_out ERROR_VARIABLE a_err)
  execute_process(COMMAND ${RELIC} ${SRC}/selfhost/run.relic ${f} WORKING_DIRECTORY ${dir} OUTPUT_VARIABLE b_out ERROR_VARIABLE b_err)
  if(NOT a_out STREQUAL b_out OR NOT a_err STREQUAL b_err)
    message("mismatch: ${n}\n--- host\n${a_out}${a_err}--- relic\n${b_out}${b_err}")
    math(EXPR bad "${bad}+1")
  endif()
endforeach()
if(bad GREATER 0)
  message(FATAL_ERROR "${bad} of ${total} programs differ")
endif()
message("all ${total} programs match")
