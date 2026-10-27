# selfhost/run.relic's command line: exit codes and the "cannot open" message
# must be the host's (2 for a missing file, 1 for an error nobody caught, the
# program's own os.exit code passed through); no file at all is a usage error (2).
#   cmake -DRELIC=path/to/relic -DSRC=source/dir -P tools/check_selfhost_cli.cmake
set(run ${SRC}/selfhost/run.relic)
set(golden ${SRC}/tests/golden)

function(expect label code err_text)
  execute_process(COMMAND ${RELIC} ${ARGN} WORKING_DIRECTORY ${golden}
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
  if(NOT got STREQUAL code)
    message(FATAL_ERROR "${label}: exit ${got}, wanted ${code}\n${out}${err}")
  endif()
  if(NOT err_text STREQUAL "" AND NOT err MATCHES "${err_text}")
    message(FATAL_ERROR "${label}: stderr did not match '${err_text}'\n${err}")
  endif()
endfunction()

expect("no file" 2 "^usage: relic selfhost/run.relic FILE" ${run})
expect("missing file" 2 "^relic: cannot open nofile.relic\n$" ${run} nofile.relic)
expect("host, missing file" 2 "^relic: cannot open nofile.relic\n$" nofile.relic)
expect("uncaught error" 1 "division by zero" ${run} rt_div_zero.relic)
expect("host, uncaught error" 1 "division by zero" rt_div_zero.relic)
expect("syntax error" 1 "error:" ${run} err_bang.relic)
expect("clean run" 0 "" ${run} hello.relic)
message("selfhost/run.relic exit codes match the host")
