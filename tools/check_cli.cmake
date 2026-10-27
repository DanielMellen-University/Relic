# The command line as docs/CLI.md describes it: exit codes, usage errors, args
# passed through, relic check, and the REPL on piped input.
#   cmake -DRELIC=path/to/relic -DSRC=source/dir -DTMP=scratch/dir -P tools/check_cli.cmake
file(MAKE_DIRECTORY ${TMP})
file(WRITE ${TMP}/args.relic "print(args())\n")
file(WRITE ${TMP}/clean.relic "let x = 1\nprint(x)\n")
file(WRITE ${TMP}/typo.relic "print(prnt)\n")
file(WRITE ${TMP}/exit3.relic "use std/os\nprint(\"before\")\nos.exit(3)\nprint(\"after\")\n")
file(WRITE ${TMP}/boom.relic "print(\"before\")\nprint(1 / 0)\n")
file(WRITE ${TMP}/chunks.relic "print(1 / 0)\nprint(\"still here\")\n")

# expect(label code out_regex err_regex ARGS...); stdin from the variable `stdin_file` if set
function(expect label code out_re err_re)
  if(stdin_file)
    execute_process(COMMAND ${RELIC} ${ARGN} WORKING_DIRECTORY ${TMP} INPUT_FILE ${stdin_file}
      OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
  else()
    execute_process(COMMAND ${RELIC} ${ARGN} WORKING_DIRECTORY ${TMP}
      OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
  endif()
  string(REPLACE "\r\n" "\n" out "${out}")
  string(REPLACE "\r\n" "\n" err "${err}")
  if(NOT got STREQUAL code)
    message(FATAL_ERROR "${label}: exit ${got}, wanted ${code}\n${out}${err}")
  endif()
  if(NOT out MATCHES "${out_re}")
    message(FATAL_ERROR "${label}: stdout did not match '${out_re}'\n${out}")
  endif()
  if(NOT err MATCHES "${err_re}")
    message(FATAL_ERROR "${label}: stderr did not match '${err_re}'\n${err}")
  endif()
endfunction()

set(usage "^usage: relic ")

expect("version" 0 "^relic [0-9]+\\.[0-9]+\\.[0-9]+\n$" "^$" --version)
expect("help" 0 "^$" "${usage}" --help)
expect("unknown flag" 2 "^$" "${usage}" --bogus)
expect("--tokens without a file" 2 "^$" "${usage}" --tokens)
expect("--ast with extra words" 2 "^$" "${usage}" --ast clean.relic extra)
expect("missing file" 2 "^$" "^relic: cannot open nofile.relic\n$" nofile.relic)
expect("missing file after --tree" 2 "^$" "^relic: cannot open nofile.relic\n$" --tree nofile.relic)

# Anything after the file goes to the program, flags included, on every engine.
expect("args" 0 "^\\[\"--x\", \"y\"\\]\n$" "^$" args.relic --x y)
expect("args, --tree" 0 "^\\[\"--x\"\\]\n$" "^$" --tree args.relic --x)
expect("args, --vm" 0 "^\\[\"1\"\\]\n$" "^$" --vm args.relic 1)

# Exit codes of a run: error 1, os.exit(n) as given, output before either is kept.
expect("runtime error" 1 "^before\n$" "division by zero" boom.relic)
expect("runtime error, --tree" 1 "^before\n$" "division by zero" --tree boom.relic)
expect("os.exit" 3 "^before\n$" "^$" exit3.relic)

# relic check
expect("check clean" 0 "^$" "^$" check clean.relic)
expect("check typo" 1 "^$" "typo.relic:1:7: warning: 'prnt' is not defined \\(did you mean 'print'\\?\\)" check typo.relic)
expect("check several" 1 "^$" "typo.relic:1:7: warning" check clean.relic typo.relic)
expect("check missing" 2 "^$" "^relic: cannot open nofile.relic\n$" check nofile.relic)
expect("check without files" 2 "^$" "${usage}" check)

# The REPL on piped input: no prompts, an error does not stop it, exit 0
set(stdin_file ${TMP}/chunks.relic)
expect("repl pipe" 0 "^still here\n$" "<repl>:1:9: error: division by zero" )
message("command line matches CLI.md")
