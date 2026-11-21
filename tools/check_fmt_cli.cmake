# Shared formatter CLI and golden checks. FMT_COMMAND is a command list.
file(MAKE_DIRECTORY ${TMP})
set(empty ${SRC}/tools/empty.in)

function(fmt_of src out_var err_var code_var)
  execute_process(COMMAND ${FMT_COMMAND} ${src}
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code)
  string(REPLACE "\r\n" "\n" out "${out}")
  string(REPLACE "\r\n" "\n" err "${err}")
  set(${out_var} "${out}" PARENT_SCOPE)
  set(${err_var} "${err}" PARENT_SCOPE)
  set(${code_var} "${code}" PARENT_SCOPE)
endfunction()

# --- command line ---
execute_process(COMMAND ${FMT_COMMAND} RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_VARIABLE out)
if(NOT code EQUAL 2 OR NOT out STREQUAL "" OR NOT err MATCHES "^usage: relic( |-)")
  message(FATAL_ERROR "fmt with no file: exit ${code}\n${out}${err}")
endif()
execute_process(COMMAND ${FMT_COMMAND} --write --check ${SRC}/tests/fmt/in/comments.relic
  RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_VARIABLE out)
if(NOT code EQUAL 2 OR NOT err MATCHES "^usage: relic( |-)")
  message(FATAL_ERROR "fmt --write --check: exit ${code}\n${out}${err}")
endif()
execute_process(COMMAND ${FMT_COMMAND} --bogus ${SRC}/tests/fmt/in/comments.relic
  RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_VARIABLE out)
if(NOT code EQUAL 2 OR NOT err MATCHES "^usage: relic( |-)")
  message(FATAL_ERROR "fmt --bogus: exit ${code}\n${out}${err}")
endif()
execute_process(COMMAND ${FMT_COMMAND} no-such-file.relic
  WORKING_DIRECTORY ${TMP} RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_VARIABLE out)
if(NOT code EQUAL 2 OR NOT err STREQUAL "relic: cannot open no-such-file.relic\n")
  message(FATAL_ERROR "fmt missing file: exit ${code}\n${out}${err}")
endif()
# file(WRITE) uses text mode on Windows. Normalize this fixture explicitly:
# --write must later preserve its exact LF bytes when parsing fails.
file(WRITE ${TMP}/bad.in "let x =\n")
configure_file(${TMP}/bad.in ${TMP}/bad.relic @ONLY NEWLINE_STYLE LF)
execute_process(COMMAND ${FMT_COMMAND} ${TMP}/bad.relic
  RESULT_VARIABLE code ERROR_VARIABLE err OUTPUT_VARIABLE out)
string(REPLACE "\r\n" "\n" err "${err}")
string(REPLACE "\r\n" "\n" out "${out}")
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
execute_process(COMMAND ${FMT_COMMAND} --check ${TMP}/readme.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(REPLACE "\r\n" "\n" out "${out}")
if(NOT code EQUAL 1 OR NOT out STREQUAL "${TMP}/readme.relic\n")
  message(FATAL_ERROR "fmt --check dirty: exit ${code}\n${out}${err}")
endif()
execute_process(COMMAND ${FMT_COMMAND} --write ${TMP}/readme.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt --write: exit ${code}\n${out}${err}")
endif()
file(READ ${TMP}/readme.relic wrote)
string(REPLACE "\r\n" "\n" wrote "${wrote}")
if(NOT wrote STREQUAL neat)
  message(FATAL_ERROR "fmt --write did not write the canonical text\n${wrote}")
endif()
execute_process(COMMAND ${FMT_COMMAND} --check ${TMP}/readme.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt --check clean: exit ${code}\n${out}${err}")
endif()

# --write normalizes CRLF source to canonical LF output.
file(WRITE ${TMP}/crlf.in "print( 1 )\n")
configure_file(${TMP}/crlf.in ${TMP}/crlf.relic @ONLY NEWLINE_STYLE CRLF)
file(READ ${TMP}/crlf.relic crlf_input HEX)
if(NOT crlf_input STREQUAL "7072696e7428203120290d0a")
  message(FATAL_ERROR "fmt CRLF fixture does not contain exactly print( 1 ) followed by CRLF")
endif()
execute_process(COMMAND ${FMT_COMMAND} --write ${TMP}/crlf.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
file(READ ${TMP}/crlf.relic crlf_wrote)
if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "" OR NOT crlf_wrote STREQUAL "print(1)\n")
  message(FATAL_ERROR "fmt --write CRLF: exit ${code}\n${out}${err}--- file\n${crlf_wrote}")
endif()

# Formatting adds the canonical final newline when the input has none.
file(WRITE ${TMP}/no_final_newline.relic "print( 1 )")
execute_process(COMMAND ${FMT_COMMAND} ${TMP}/no_final_newline.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "print(1)\n" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt missing final newline: exit ${code}\n${out}${err}")
endif()

# Empty files stay empty, and comment-only files trim trailing blank lines.
file(WRITE ${TMP}/empty.relic "")
execute_process(COMMAND ${FMT_COMMAND} ${TMP}/empty.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt empty file: exit ${code}\n${out}${err}")
endif()
file(WRITE ${TMP}/comment_only.relic "# only\n\n")
execute_process(COMMAND ${FMT_COMMAND} ${TMP}/comment_only.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "# only\n" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt comment-only file: exit ${code}\n${out}${err}")
endif()

# A trailing comment on a multiline call stays on the closing line.
file(WRITE ${TMP}/after_close_comment.relic "print(\n  1\n) # tail\n")
execute_process(COMMAND ${FMT_COMMAND} ${TMP}/after_close_comment.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "print(\n    1\n)  # tail\n" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt comment after multiline close: exit ${code}\n${out}${err}")
endif()

# With several inputs, --check prints only the paths that would change.
file(WRITE ${TMP}/dirty.relic "${messy}")
# Copy the LF golden verbatim; file(WRITE) would introduce CRLF on Windows.
configure_file(${SRC}/tests/fmt/out/readme_example.fmt ${TMP}/clean.relic COPYONLY)
execute_process(COMMAND ${FMT_COMMAND} --check ${TMP}/dirty.relic ${TMP}/clean.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(REPLACE "\r\n" "\n" out "${out}")
if(NOT code EQUAL 1 OR NOT out STREQUAL "${TMP}/dirty.relic\n" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt --check mixed files: exit ${code}\n${out}${err}")
endif()
file(WRITE ${TMP}/dirty_a.relic "${messy}")
file(WRITE ${TMP}/dirty_b.relic "${messy}")
execute_process(COMMAND ${FMT_COMMAND} --check ${TMP}/dirty_b.relic ${TMP}/dirty_a.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(REPLACE "\r\n" "\n" out "${out}")
if(NOT code EQUAL 1 OR NOT out STREQUAL "${TMP}/dirty_b.relic\n${TMP}/dirty_a.relic\n" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt --check path order: exit ${code}\n${out}${err}")
endif()
execute_process(COMMAND ${FMT_COMMAND} --check ${TMP}/missing.relic ${TMP}/dirty_a.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 2 OR NOT out STREQUAL "${TMP}/dirty_a.relic\n" OR
   NOT err STREQUAL "relic: cannot open ${TMP}/missing.relic\n")
  message(FATAL_ERROR "fmt --check missing and dirty: exit ${code}\n${out}${err}")
endif()
set(spaced_file "${TMP}/file with spaces.relic")
file(WRITE "${spaced_file}" "${messy}")
execute_process(COMMAND ${FMT_COMMAND} --check "${spaced_file}"
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 1 OR NOT out STREQUAL "${spaced_file}\n" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt --check path with spaces: exit ${code}\n${out}${err}")
endif()
file(WRITE ${TMP}/write_dirty.relic "${messy}")
configure_file(${SRC}/tests/fmt/out/readme_example.fmt ${TMP}/write_clean.relic COPYONLY)
execute_process(COMMAND ${FMT_COMMAND} --write ${TMP}/write_dirty.relic ${TMP}/write_clean.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
file(READ ${TMP}/write_dirty.relic wrote_dirty)
file(READ ${TMP}/write_clean.relic wrote_clean)
if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "" OR
   NOT wrote_dirty STREQUAL neat OR NOT wrote_clean STREQUAL neat)
  message(FATAL_ERROR "fmt --write multiple files: exit ${code}\n--- stdout\n${out}--- stderr\n${err}--- dirty\n${wrote_dirty}--- clean\n${wrote_clean}")
endif()
configure_file(${TMP}/bad.relic ${TMP}/write_bad.relic COPYONLY)
file(WRITE ${TMP}/write_after_bad.relic "${messy}")
execute_process(COMMAND ${FMT_COMMAND} --write ${TMP}/write_bad.relic ${TMP}/write_after_bad.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(REPLACE "\r\n" "\n" err "${err}")
file(READ ${TMP}/write_bad.relic kept_bad)
file(READ ${TMP}/write_after_bad.relic wrote_after_bad)
if(NOT code EQUAL 1 OR NOT out STREQUAL "" OR NOT err MATCHES "expected an expression, got end of line" OR
   NOT kept_bad STREQUAL "let x =\n" OR NOT wrote_after_bad STREQUAL neat)
  message(FATAL_ERROR "fmt --write after syntax error: exit ${code}\n--- stdout\n${out}--- stderr\n${err}--- bad\n${kept_bad}--- later\n${wrote_after_bad}")
endif()

# Directory inputs are errors, and later valid files still reach stdout.
execute_process(COMMAND ${FMT_COMMAND} ${TMP} ${SRC}/tests/fmt/in/strings.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
file(READ ${SRC}/tests/fmt/out/strings.fmt strings_want)
if(NOT code EQUAL 2 OR NOT out STREQUAL strings_want OR
   NOT err STREQUAL "relic: cannot open ${TMP}\n")
  message(FATAL_ERROR "fmt directory and valid file: exit ${code}\n${out}${err}")
endif()

# Stdout concatenates files in argument order, with no extra separators.
execute_process(COMMAND ${FMT_COMMAND} ${SRC}/tests/fmt/in/strings.relic ${SRC}/tests/fmt/in/readme_example.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT out STREQUAL "${strings_want}${neat}" OR NOT err STREQUAL "")
  message(FATAL_ERROR "fmt stdout multiple files: exit ${code}\n${out}${err}")
endif()

# Lexer and indentation errors must be rejected before any write.
foreach(kind tab indent escape)
  if(kind STREQUAL "tab")
    set(bad "if true\n\tprint(1)\n")
  elseif(kind STREQUAL "indent")
    set(bad "if true\n  print(1)\n")
  else()
    set(bad "print(\"\\q\")\n")
  endif()
  file(WRITE ${TMP}/${kind}.relic "${bad}")
  execute_process(COMMAND ${FMT_COMMAND} --write ${TMP}/${kind}.relic
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  file(READ ${TMP}/${kind}.relic kept)
  if(NOT code EQUAL 1 OR NOT out STREQUAL "" OR NOT err MATCHES ": error: " OR NOT kept STREQUAL bad)
    message(FATAL_ERROR "fmt rejected ${kind}: exit ${code}\n${out}${err}--- file\n${kept}")
  endif()
endforeach()

# Rewriting an already canonical file must leave its timestamp alone.
file(TIMESTAMP ${TMP}/clean.relic before "%s")
execute_process(COMMAND ${CMAKE_COMMAND} -E sleep 1.1)
execute_process(COMMAND ${FMT_COMMAND} --write ${TMP}/clean.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
file(TIMESTAMP ${TMP}/clean.relic after "%s")
if(NOT code EQUAL 0 OR NOT out STREQUAL "" OR NOT err STREQUAL "" OR NOT before STREQUAL after)
  message(FATAL_ERROR "fmt touched canonical input: exit ${code}\n${out}${err}")
endif()

foreach(mode --check --write)
  execute_process(COMMAND ${FMT_COMMAND} ${mode}
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT code EQUAL 2 OR NOT out STREQUAL "" OR NOT err MATCHES "^usage: relic( |-)")
    message(FATAL_ERROR "fmt ${mode} without files: exit ${code}\n${out}${err}")
  endif()
endforeach()
execute_process(COMMAND ${FMT_COMMAND} --check --write ${SRC}/tests/fmt/in/comments.relic
  RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 2 OR NOT out STREQUAL "" OR NOT err MATCHES "^usage: relic( |-)")
  message(FATAL_ERROR "fmt --check --write: exit ${code}\n${out}${err}")
endif()

# An open failure wins over a parse error in either argument order.
foreach(order bad_first missing_first)
  if(order STREQUAL "bad_first")
    set(inputs ${TMP}/bad.relic ${TMP}/missing.relic)
  else()
    set(inputs ${TMP}/missing.relic ${TMP}/bad.relic)
  endif()
  execute_process(COMMAND ${FMT_COMMAND} ${inputs} ${TMP}/clean.relic
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT code EQUAL 2 OR NOT out STREQUAL neat OR
     NOT err MATCHES "expected an expression, got end of line" OR NOT err MATCHES "relic: cannot open")
    message(FATAL_ERROR "fmt ${order} error priority: exit ${code}\n${out}${err}")
  endif()
endforeach()
message("fmt CLI: ${golden_n} goldens, idempotence, stdout, check, write and rejected inputs passed")
