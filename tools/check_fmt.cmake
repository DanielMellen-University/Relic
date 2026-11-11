# relic fmt: golden files, idempotence, and every .relic file in the tree.
#   cmake -DRELIC=path/to/relic -DSRC=source/dir -DTMP=scratch/dir -P tools/check_fmt.cmake
file(MAKE_DIRECTORY ${TMP})
set(empty ${SRC}/tools/empty.in)

function(fmt_of src out_var err_var code_var)
  execute_process(COMMAND ${RELIC} fmt ${src}
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code)
  string(REPLACE "\r\n" "\n" out "${out}")
  string(REPLACE "\r\n" "\n" err "${err}")
  set(${out_var} "${out}" PARENT_SCOPE)
  set(${err_var} "${err}" PARENT_SCOPE)
  set(${code_var} "${code}" PARENT_SCOPE)
endfunction()

# --- command line ---
execute_process(COMMAND ${RELIC} fmt RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_VARIABLE out)
if(NOT code EQUAL 2 OR NOT out STREQUAL "" OR NOT err MATCHES "^usage: relic ")
  message(FATAL_ERROR "fmt with no file: exit ${code}\n${out}${err}")
endif()
execute_process(COMMAND ${RELIC} fmt --write --check ${SRC}/tests/fmt/in/comments.relic
  RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_VARIABLE out)
if(NOT code EQUAL 2 OR NOT err MATCHES "^usage: relic ")
  message(FATAL_ERROR "fmt --write --check: exit ${code}\n${out}${err}")
endif()
execute_process(COMMAND ${RELIC} fmt --bogus ${SRC}/tests/fmt/in/comments.relic
  RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_VARIABLE out)
if(NOT code EQUAL 2 OR NOT err MATCHES "^usage: relic ")
  message(FATAL_ERROR "fmt --bogus: exit ${code}\n${out}${err}")
endif()
execute_process(COMMAND ${RELIC} fmt no-such-file.relic
  WORKING_DIRECTORY ${TMP} RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_VARIABLE out)
if(NOT code EQUAL 2 OR NOT err STREQUAL "relic: cannot open no-such-file.relic\n")
  message(FATAL_ERROR "fmt missing file: exit ${code}\n${out}${err}")
endif()
file(WRITE ${TMP}/bad.relic "let x =\n")
execute_process(COMMAND ${RELIC} fmt ${TMP}/bad.relic
  RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_VARIABLE out)
string(REPLACE "\r\n" "\n" err "${err}")
string(REPLACE "\r\n" "\n" out "${out}")
# Do not pin the column. file(WRITE) stores CRLF on Windows, and the lexer
# counts that carriage return, so the caret is one column further than on LF.
if(NOT code EQUAL 1 OR NOT out STREQUAL "" OR NOT err MATCHES "expected an expression, got end of line")
  message(FATAL_ERROR "fmt syntax error: exit ${code}\n${out}${err}")
endif()

# --- goldens: input, expected output, and a second format that matches the first ---
file(GLOB golden_in ${SRC}/tests/fmt/in/*.relic)
list(LENGTH golden_in golden_n)
if(golden_n LESS 1)
  message(FATAL_ERROR "no tests/fmt/in/*.relic files")
endif()
foreach(src ${golden_in})
  get_filename_component(name ${src} NAME_WE)
  set(want_path ${SRC}/tests/fmt/out/${name}.fmt)
  if(NOT EXISTS ${want_path})
    message(FATAL_ERROR "${name}: missing tests/fmt/out/${name}.fmt")
  endif()
  file(READ ${want_path} want)
  string(REPLACE "\r\n" "\n" want "${want}")
  fmt_of(${src} once err code)
  if(NOT code EQUAL 0 OR NOT once STREQUAL want)
    message(FATAL_ERROR "${name}: format does not match tests/fmt/out/${name}.fmt (exit ${code})\n--- want\n${want}--- got\n${once}--- err\n${err}")
  endif()
  file(WRITE ${TMP}/once.relic "${once}")
  fmt_of(${TMP}/once.relic twice err2 code2)
  if(NOT code2 EQUAL 0 OR NOT twice STREQUAL once)
    message(FATAL_ERROR "${name}: formatting twice changed the text (exit ${code2})\n--- once\n${once}--- twice\n${twice}")
  endif()
endforeach()

# --check and --write on a copy of the readme example
file(READ ${SRC}/tests/fmt/in/readme_example.relic messy)
file(READ ${SRC}/tests/fmt/out/readme_example.fmt neat)
string(REPLACE "\r\n" "\n" messy "${messy}")
string(REPLACE "\r\n" "\n" neat "${neat}")
file(WRITE ${TMP}/readme.relic "${messy}")
execute_process(COMMAND ${RELIC} fmt --check ${TMP}/readme.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(REPLACE "\r\n" "\n" out "${out}")
if(NOT code EQUAL 1 OR NOT out STREQUAL "${TMP}/readme.relic\n")
  message(FATAL_ERROR "fmt --check dirty: exit ${code}\n${out}${err}")
endif()
execute_process(COMMAND ${RELIC} fmt --write ${TMP}/readme.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt --write: exit ${code}\n${out}${err}")
endif()
file(READ ${TMP}/readme.relic wrote)
string(REPLACE "\r\n" "\n" wrote "${wrote}")
if(NOT wrote STREQUAL neat)
  message(FATAL_ERROR "fmt --write did not write the canonical text\n${wrote}")
endif()
execute_process(COMMAND ${RELIC} fmt --check ${TMP}/readme.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt --check clean: exit ${code}\n${out}${err}")
endif()

# --- every .relic file in the tree ---
file(GLOB_RECURSE corpus LIST_DIRECTORIES false ${SRC}/*.relic)
set(same_n 0)
set(changed_n 0)
set(rejected_n 0)
foreach(src ${corpus})
  file(RELATIVE_PATH rel ${SRC} ${src})
  if(rel MATCHES "(^|/)build")
    continue()
  endif()
  fmt_of(${src} once err code)
  execute_process(COMMAND ${RELIC} --ast ${src}
    OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE ast_code)
  if(NOT code EQUAL 0)
    if(ast_code EQUAL 0)
      message(FATAL_ERROR "${rel}: fmt rejected a file that parses\n${err}")
    endif()
    math(EXPR rejected_n "${rejected_n} + 1")
    continue()
  endif()
  if(NOT ast_code EQUAL 0)
    message(FATAL_ERROR "${rel}: fmt accepted a file that does not parse")
  endif()
  file(WRITE ${TMP}/once.relic "${once}")
  fmt_of(${TMP}/once.relic twice err2 code2)
  if(NOT code2 EQUAL 0 OR NOT twice STREQUAL once)
    message(FATAL_ERROR "${rel}: formatting twice changed the text (exit ${code2})")
  endif()
  file(READ ${src} orig)
  string(REPLACE "\r\n" "\n" orig "${orig}")
  if(orig STREQUAL once)
    math(EXPR same_n "${same_n} + 1")
    continue()
  endif()
  # The formatted program has to compile and print the same thing. It sits next
  # to the original so `use` of a neighbour still resolves.
  get_filename_component(dir ${src} DIRECTORY)
  get_filename_component(base ${src} NAME)
  set(tmp ${dir}/.fmt-corpus.relic)
  file(WRITE ${tmp} "${once}")
  execute_process(COMMAND ${RELIC} ${src}
    WORKING_DIRECTORY ${dir} INPUT_FILE ${empty}
    OUTPUT_VARIABLE out_a ERROR_VARIABLE err_a RESULT_VARIABLE code_a
    TIMEOUT 60)
  execute_process(COMMAND ${RELIC} ${tmp}
    WORKING_DIRECTORY ${dir} INPUT_FILE ${empty}
    OUTPUT_VARIABLE out_b ERROR_VARIABLE err_b RESULT_VARIABLE code_b
    TIMEOUT 60)
  file(REMOVE ${tmp})
  string(REPLACE "\r\n" "\n" out_a "${out_a}")
  string(REPLACE "\r\n" "\n" out_b "${out_b}")
  string(REPLACE "\r\n" "\n" err_a "${err_a}")
  string(REPLACE "\r\n" "\n" err_b "${err_b}")
  string(REPLACE "${tmp}" "${src}" out_b "${out_b}")
  string(REPLACE "${tmp}" "${src}" err_b "${err_b}")
  string(REPLACE ".fmt-corpus.relic" "${base}" out_b "${out_b}")
  string(REPLACE ".fmt-corpus.relic" "${base}" err_b "${err_b}")
  if(NOT code_a EQUAL code_b OR NOT out_a STREQUAL out_b OR NOT err_a STREQUAL err_b)
    message(FATAL_ERROR "${rel}: formatted program differs (exit ${code_a} vs ${code_b})\n--- stdout orig\n${out_a}--- stdout fmt\n${out_b}--- stderr orig\n${err_a}--- stderr fmt\n${err_b}")
  endif()
  math(EXPR changed_n "${changed_n} + 1")
endforeach()
message("fmt: ${golden_n} goldens, ${same_n} files already canonical, ${changed_n} reformatted with the same output, ${rejected_n} files rejected (they do not parse)")

# --- fault injection: fmt --write must keep the original on failure ---
# Linux only: the injector interposes libc calls with LD_PRELOAD, and the
# build needs a C compiler. Elsewhere this section is skipped, not failed.
find_program(fmt_cc NAMES cc gcc clang)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND fmt_cc)
  execute_process(COMMAND ${fmt_cc} -shared -fPIC -o ${TMP}/failio.so ${SRC}/tests/fmt/failio.c -ldl
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "building the fmt fault injector failed: ${err}")
  endif()
  # One fault case: run fmt --write on a victim that needs reformatting and
  # check the exit code, the diagnostic, the untouched victim, and that no
  # temp file is left behind.
  function(fmt_fault label fault_env)
    file(WRITE ${TMP}/victim.relic "print( 1 )\n")
    execute_process(COMMAND ${CMAKE_COMMAND} -E env LD_PRELOAD=${TMP}/failio.so ${fault_env} ${RELIC} fmt --write ${TMP}/victim.relic
      RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    file(READ ${TMP}/victim.relic kept)
    file(GLOB leftover "${TMP}/*.fmt-tmp*")
    if(NOT code EQUAL 2 OR NOT out STREQUAL "" OR NOT err STREQUAL "relic: cannot write ${TMP}/victim.relic\n" OR NOT kept STREQUAL "print( 1 )\n" OR leftover)
      message(FATAL_ERROR "fmt fault ${label}: exit ${code}, stdout '${out}', stderr '${err}', victim '${kept}', leftover '${leftover}'")
    endif()
  endfunction()
  fmt_fault("write" "FAIL_FMT_WRITE=1")
  fmt_fault("rename" "FAIL_FMT_RENAME=1")
  fmt_fault("close" "FAIL_FMT_CLOSE=1")
  # Control: with no fault the same write lands.
  file(WRITE ${TMP}/victim.relic "print( 1 )\n")
  execute_process(COMMAND ${CMAKE_COMMAND} -E env LD_PRELOAD=${TMP}/failio.so ${RELIC} fmt --write ${TMP}/victim.relic
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  file(READ ${TMP}/victim.relic wrote)
  if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "" OR NOT wrote STREQUAL "print(1)\n")
    message(FATAL_ERROR "fmt fault control: exit ${code}, stdout '${out}', stderr '${err}', victim '${wrote}'")
  endif()
  message("fmt: fault injection passed (write, rename, close failures keep the original)")
else()
  message("fmt: fault injection skipped (needs Linux and a C compiler)")
endif()
