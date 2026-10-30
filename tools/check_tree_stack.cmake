# --tree recurses on the native stack, so a big expression inside deep recursion
# (f(n) = f(n - 1) + 1 + 1 ... with 9,990 terms, 700 calls deep) ran out of it
# and crashed (exit 139) before the 800-call limit. Since 1.0.40 it is a clean
# error that try can catch; the VM, which does not recurse natively, still runs it.
#   cmake -DRELIC=relic -DTMP=scratch/dir -P tools/check_tree_stack.cmake
file(MAKE_DIRECTORY ${TMP})
string(REPEAT " + 1" 9990 terms)
file(WRITE ${TMP}/deep.relic "fun f(n)\n    if n == 0\n        return 0\n    return f(n - 1)${terms}\nprint(f(700))\n")
file(WRITE ${TMP}/deep_try.relic "fun f(n)\n    if n == 0\n        return 0\n    return f(n - 1)${terms}\ntry\n    print(f(700))\ncatch e\n    print(\"caught\", e.message)\nprint(f(0))\n")

function(expect label code out_re err_re)
  execute_process(COMMAND ${ARGN} WORKING_DIRECTORY ${TMP}
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
  string(REPLACE "\r\n" "\n" err "${err}")
  string(REPLACE "\r\n" "\n" out "${out}")
  string(SUBSTRING "${out}" 0 300 out_shown)
  string(SUBSTRING "${err}" 0 300 err_shown)
  if(NOT got STREQUAL code)
    message(FATAL_ERROR "${label}: exit ${got}, wanted ${code}\n${out_shown}${err_shown}")
  endif()
  if(NOT out MATCHES "${out_re}")
    message(FATAL_ERROR "${label}: stdout did not match '${out_re}'\n${out_shown}")
  endif()
  if(NOT err MATCHES "${err_re}")
    message(FATAL_ERROR "${label}: stderr did not match '${err_re}'\n${err_shown}")
  endif()
endfunction()

set(msg "too much recursion \\(calls and expressions nest too deeply for the stack\\)")
expect("vm" 0 "^6993000\n$" "^$" ${RELIC} deep.relic)
expect("tree" 1 "^$" "^deep.relic:4:[0-9]+: error: ${msg}\n" ${RELIC} --tree deep.relic)
expect("tree try" 0 "^caught ${msg}\n0\n$" "^$" ${RELIC} --tree deep_try.relic)
message("tree_stack: --tree gives a clean error, the VM runs it")
