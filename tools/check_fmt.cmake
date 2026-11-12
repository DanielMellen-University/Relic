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

# --write normalizes CRLF source to canonical LF output.
file(WRITE ${TMP}/crlf.relic "print( 1 )\r\n")
execute_process(COMMAND ${RELIC} fmt --write ${TMP}/crlf.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
file(READ ${TMP}/crlf.relic crlf_wrote)
if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "" OR NOT crlf_wrote STREQUAL "print(1)\n")
  message(FATAL_ERROR "fmt --write CRLF: exit ${code}\n${out}${err}--- file\n${crlf_wrote}")
endif()

# Formatting adds the canonical final newline when the input has none.
file(WRITE ${TMP}/no_final_newline.relic "print( 1 )")
execute_process(COMMAND ${RELIC} fmt ${TMP}/no_final_newline.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "print(1)\n" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt missing final newline: exit ${code}\n${out}${err}")
endif()

# Empty files stay empty, and comment-only files trim trailing blank lines.
file(WRITE ${TMP}/empty.relic "")
execute_process(COMMAND ${RELIC} fmt ${TMP}/empty.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt empty file: exit ${code}\n${out}${err}")
endif()
file(WRITE ${TMP}/comment_only.relic "# only\n\n")
execute_process(COMMAND ${RELIC} fmt ${TMP}/comment_only.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "# only\n" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt comment-only file: exit ${code}\n${out}${err}")
endif()

# A trailing comment on a multiline call stays on the closing line.
file(WRITE ${TMP}/after_close_comment.relic "print(\n  1\n) # tail\n")
execute_process(COMMAND ${RELIC} fmt ${TMP}/after_close_comment.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "print(\n    1\n)  # tail\n" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt comment after multiline close: exit ${code}\n${out}${err}")
endif()

# With several inputs, --check prints only the paths that would change.
file(WRITE ${TMP}/dirty.relic "${messy}")
file(WRITE ${TMP}/clean.relic "${neat}")
execute_process(COMMAND ${RELIC} fmt --check ${TMP}/dirty.relic ${TMP}/clean.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(REPLACE "\r\n" "\n" out "${out}")
if(NOT code EQUAL 1 OR NOT out STREQUAL "${TMP}/dirty.relic\n" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt --check mixed files: exit ${code}\n${out}${err}")
endif()
file(WRITE ${TMP}/dirty_a.relic "${messy}")
file(WRITE ${TMP}/dirty_b.relic "${messy}")
execute_process(COMMAND ${RELIC} fmt --check ${TMP}/dirty_b.relic ${TMP}/dirty_a.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(REPLACE "\r\n" "\n" out "${out}")
if(NOT code EQUAL 1 OR NOT out STREQUAL "${TMP}/dirty_b.relic\n${TMP}/dirty_a.relic\n" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt --check path order: exit ${code}\n${out}${err}")
endif()
execute_process(COMMAND ${RELIC} fmt --check ${TMP}/missing.relic ${TMP}/dirty_a.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 2 OR NOT out STREQUAL "${TMP}/dirty_a.relic\n" OR
   NOT err STREQUAL "relic: cannot open ${TMP}/missing.relic\n")
  message(FATAL_ERROR "fmt --check missing and dirty: exit ${code}\n${out}${err}")
endif()
set(spaced_file "${TMP}/file with spaces.relic")
file(WRITE "${spaced_file}" "${messy}")
execute_process(COMMAND ${RELIC} fmt --check "${spaced_file}"
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 1 OR NOT out STREQUAL "${spaced_file}\n" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt --check path with spaces: exit ${code}\n${out}${err}")
endif()
file(WRITE ${TMP}/write_dirty.relic "${messy}")
file(WRITE ${TMP}/write_clean.relic "${neat}")
execute_process(COMMAND ${RELIC} fmt --write ${TMP}/write_dirty.relic ${TMP}/write_clean.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
file(READ ${TMP}/write_dirty.relic wrote_dirty)
file(READ ${TMP}/write_clean.relic wrote_clean)
if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "" OR
   NOT wrote_dirty STREQUAL neat OR NOT wrote_clean STREQUAL neat)
  message(FATAL_ERROR "fmt --write multiple files: exit ${code}\n--- stdout\n${out}--- stderr\n${err}--- dirty\n${wrote_dirty}--- clean\n${wrote_clean}")
endif()
file(WRITE ${TMP}/write_bad.relic "let x =\n")
file(WRITE ${TMP}/write_after_bad.relic "${messy}")
execute_process(COMMAND ${RELIC} fmt --write ${TMP}/write_bad.relic ${TMP}/write_after_bad.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(REPLACE "\r\n" "\n" err "${err}")
file(READ ${TMP}/write_bad.relic kept_bad)
file(READ ${TMP}/write_after_bad.relic wrote_after_bad)
if(NOT code EQUAL 1 OR NOT out STREQUAL "" OR NOT err MATCHES "expected an expression, got end of line" OR
   NOT kept_bad STREQUAL "let x =\n" OR NOT wrote_after_bad STREQUAL neat)
  message(FATAL_ERROR "fmt --write after syntax error: exit ${code}\n--- stdout\n${out}--- stderr\n${err}--- bad\n${kept_bad}--- later\n${wrote_after_bad}")
endif()

# --- every .relic file in the tree ---
# The formatted copy lives in a mirror of the source tree under TMP, never
# beside the original: a parallel doc_counts or selfhost glob must never see
# a formatter temp file. The mirror keeps the whole tree layout, so `use`
# of a neighbour (including ../) resolves exactly as in the source tree.
set(mirror ${TMP}/corpus-tree)
file(REMOVE_RECURSE ${mirror})
# An in-tree build directory contains this mirror. Copying a directory named
# only "build" leaves build-host (and any other build*) to be copied into
# itself until the path overflows. build* excludes those names. The live
# build directory is excluded too when it sits inside the source tree under
# some other name. Source entries are not named build*.
get_filename_component(src_abs "${SRC}" ABSOLUTE)
get_filename_component(build_abs "${CMAKE_BINARY_DIR}" ABSOLUTE)
file(RELATIVE_PATH build_rel "${src_abs}" "${build_abs}")
set(build_inside 0)
set(build_name_exclude)
if(NOT build_rel STREQUAL "" AND NOT build_rel STREQUAL "." AND NOT build_rel MATCHES "^\\.\\.")
  set(build_inside 1)
  get_filename_component(build_name "${build_abs}" NAME)
  if(NOT build_name MATCHES "^build")
    set(build_name_exclude PATTERN "${build_name}" EXCLUDE)
  endif()
endif()
file(COPY ${SRC}/ DESTINATION ${mirror}
  PATTERN ".git" EXCLUDE
  PATTERN "build*" EXCLUDE
  ${build_name_exclude}
  PATTERN "*.fmt-tmp*" EXCLUDE
  PATTERN ".fmt-corpus.relic" EXCLUDE)
file(GLOB_RECURSE corpus LIST_DIRECTORIES false ${SRC}/*.relic)
set(same_n 0)
set(changed_n 0)
set(rejected_n 0)
foreach(src ${corpus})
  file(RELATIVE_PATH rel ${SRC} ${src})
  if(rel MATCHES "(^|/)build")
    continue()
  endif()
  if(build_inside)
    string(FIND "${rel}/" "${build_rel}/" build_at)
    if(build_at EQUAL 0)
      continue()
    endif()
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
  # The formatted program has to compile and print the same thing. Both
  # runs happen in the mirror, so the source tree is never written to.
  get_filename_component(rel_dir ${rel} DIRECTORY)
  set(mirror_src ${mirror}/${rel})
  set(mirror_dir ${mirror}/${rel_dir})
  execute_process(COMMAND ${RELIC} ${mirror_src}
    WORKING_DIRECTORY ${mirror_dir} INPUT_FILE ${empty}
    OUTPUT_VARIABLE out_a ERROR_VARIABLE err_a RESULT_VARIABLE code_a
    TIMEOUT 60)
  file(WRITE ${mirror_src} "${once}")
  execute_process(COMMAND ${RELIC} ${mirror_src}
    WORKING_DIRECTORY ${mirror_dir} INPUT_FILE ${empty}
    OUTPUT_VARIABLE out_b ERROR_VARIABLE err_b RESULT_VARIABLE code_b
    TIMEOUT 60)
  configure_file(${src} ${mirror_src} COPYONLY)
  string(REPLACE "\r\n" "\n" out_a "${out_a}")
  string(REPLACE "\r\n" "\n" out_b "${out_b}")
  string(REPLACE "\r\n" "\n" err_a "${err_a}")
  string(REPLACE "\r\n" "\n" err_b "${err_b}")
  string(REPLACE "${mirror_src}" "${src}" out_a "${out_a}")
  string(REPLACE "${mirror_src}" "${src}" err_a "${err_a}")
  string(REPLACE "${mirror_src}" "${src}" out_b "${out_b}")
  string(REPLACE "${mirror_src}" "${src}" err_b "${err_b}")
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
  fmt_fault("flush" "FAIL_FMT_FLUSH=1")
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
