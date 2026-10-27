# relicc's command line: exit codes 0 written, 1 a lex or parse error, 3 bad usage
# or a file that cannot be opened (the message names the file, not a std/ call).
#   cmake -DRELICC=path/to/relicc -DSRC=source/dir -DTMP=scratch/dir -P tools/check_relicc_cli.cmake
file(MAKE_DIRECTORY ${TMP})
file(WRITE ${TMP}/ok.relic "print(1)\n")
file(WRITE ${TMP}/bad.relic "print(\n")

function(expect label code err_re)
  execute_process(COMMAND ${RELICC} ${ARGN} WORKING_DIRECTORY ${TMP}
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
  string(REPLACE "\r\n" "\n" err "${err}")
  if(NOT got STREQUAL code)
    message(FATAL_ERROR "${label}: exit ${got}, wanted ${code}\n${out}${err}")
  endif()
  if(NOT err MATCHES "${err_re}")
    message(FATAL_ERROR "${label}: stderr did not match '${err_re}'\n${err}")
  endif()
endfunction()

expect("no arguments" 3 "^usage: " )
expect("no -o" 3 "^usage: " ok.relic)
expect("clean file" 0 "^$" ok.relic -o ok.c)
expect("syntax error" 1 "bad.relic:" bad.relic -o bad.c)
expect("missing file" 3 "^relicc: cannot open nofile.relic\n$" nofile.relic -o x.c)
expect("a folder" 3 "^relicc: cannot open \\.\n$" . -o x.c)
if(NOT EXISTS ${TMP}/ok.c)
  message(FATAL_ERROR "ok.c was not written")
endif()
