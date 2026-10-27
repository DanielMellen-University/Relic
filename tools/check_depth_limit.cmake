# The 10,000 limit on how deep an expression and an `else if` chain go (since 1.0.36):
# 10,000 levels run, 10,001 is a parse error with the same text and position on
# every engine, where a chain of about 55,000 used to overflow the host's stack.
#   cmake -DRELICC=relicc [-DRELIC=relic] -DSRC=source/dir -DTMP=scratch/dir -P tools/check_depth_limit.cmake
# With RELIC (a host build) it runs the VM, --tree, check and the Relic-written
# run.relic and check.relic; relicc only writes the C (a 10,000-link chain takes a
# C compiler seconds), and must give the same error.
file(MAKE_DIRECTORY ${TMP})

# a + a + ... with n operators; m.a.a... with n fields; if with n `else if`.
function(write_cases n tag)
  string(REPEAT " + a" ${n} chain)
  file(WRITE ${TMP}/expr_${tag}.relic "let a = 1\nlet x = a${chain}\nprint(x)\n")
  string(REPEAT ".a" ${n} fields)
  file(WRITE ${TMP}/field_${tag}.relic "let m = {a: 1}\nm.a = m\nlet y = m${fields}\nprint(y == m)\n")
  string(REPEAT "else if x < 0\n    print(0)\n" ${n} links)
  file(WRITE ${TMP}/elif_${tag}.relic "let x = 3\nif x < 0\n    print(0)\n${links}else\n    print(x)\n")
endfunction()
write_cases(10000 ok)
write_cases(10001 bad)

set(msg_expr "error: expression is too deeply nested \\(limit 10000\\)")
set(msg_elif "error: 'else if' chain is too long \\(limit 10000\\)")
# where the error points: the 10,001st `+`, the 10,001st `.`, the 10,001st `else if`'s `if`
set(where_expr "expr_bad.relic:2:40011: ${msg_expr}\n")
set(where_field "field_bad.relic:3:20010: ${msg_expr}\n")
set(where_elif "elif_bad.relic:20004:6: ${msg_elif}\n")

function(expect label code out_re err_re)
  execute_process(COMMAND ${ARGN} WORKING_DIRECTORY ${TMP}
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
  string(REPLACE "\r\n" "\n" err "${err}")
  string(REPLACE "\r\n" "\n" out "${out}")
  if(NOT got STREQUAL code)
    string(SUBSTRING "${out}${err}" 0 400 shown)
    message(FATAL_ERROR "${label}: exit ${got}, wanted ${code}\n${shown}")
  endif()
  if(NOT out MATCHES "${out_re}")
    string(SUBSTRING "${out}" 0 400 shown)
    message(FATAL_ERROR "${label}: stdout did not match '${out_re}'\n${shown}")
  endif()
  if(NOT err MATCHES "${err_re}")
    string(SUBSTRING "${err}" 0 400 shown)
    message(FATAL_ERROR "${label}: stderr did not match '${err_re}'\n${shown}")
  endif()
endfunction()

if(RELIC)
  foreach(mode IN ITEMS vm tree)
    set(flag)
    if(mode STREQUAL "tree")
      set(flag --tree)
    endif()
    expect("${mode} expr_ok" 0 "^10001\n$" "^$" ${RELIC} ${flag} expr_ok.relic)
    expect("${mode} field_ok" 0 "^true\n$" "^$" ${RELIC} ${flag} field_ok.relic)
    expect("${mode} elif_ok" 0 "^3\n$" "^$" ${RELIC} ${flag} elif_ok.relic)
    expect("${mode} expr_bad" 1 "^$" "^${where_expr}" ${RELIC} ${flag} expr_bad.relic)
    expect("${mode} field_bad" 1 "^$" "^${where_field}" ${RELIC} ${flag} field_bad.relic)
    expect("${mode} elif_bad" 1 "^$" "^${where_elif}" ${RELIC} ${flag} elif_bad.relic)
  endforeach()
  expect("check ok" 0 "^$" "^$" ${RELIC} check expr_ok.relic field_ok.relic elif_ok.relic)
  expect("check expr_bad" 1 "^$" "^${where_expr}" ${RELIC} check expr_bad.relic)
  expect("check elif_bad" 1 "^$" "^${where_elif}" ${RELIC} check elif_bad.relic)
  set(run ${SRC}/selfhost/run.relic)
  expect("run.relic expr_ok" 0 "^10001\n$" "^$" ${RELIC} ${run} expr_ok.relic)
  expect("run.relic expr_bad" 1 "^$" "^${where_expr}" ${RELIC} ${run} expr_bad.relic)
  expect("run.relic field_bad" 1 "^$" "^${where_field}" ${RELIC} ${run} field_bad.relic)
  expect("run.relic elif_bad" 1 "^$" "^${where_elif}" ${RELIC} ${run} elif_bad.relic)
  expect("check.relic expr_bad" 1 "^$" "^${where_expr}" ${RELIC} ${SRC}/selfhost/check.relic expr_bad.relic)
endif()

foreach(c IN ITEMS expr field elif)
  expect("relicc ${c}_ok" 0 "^$" "^$" ${RELICC} ${c}_ok.relic -o ${c}_ok.c)
  expect("relicc ${c}_bad" 1 "^$" "^${where_${c}}" ${RELICC} ${c}_bad.relic -o ${c}_bad.c)
endforeach()
