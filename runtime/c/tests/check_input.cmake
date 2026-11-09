# execute_process supplies a pipe on every platform, including Windows.
file(MAKE_DIRECTORY "${TMP}")
string(REPEAT "x" 300 long_line)
file(WRITE "${TMP}/lines.in" "\n\r\n${long_line}\r\nlast\r")
file(WRITE "${TMP}/empty.in" "")
foreach(mode lines empty)
  execute_process(COMMAND "${EXE}" "${mode}" INPUT_FILE "${TMP}/${mode}.in"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE status)
  string(REPLACE "\r\n" "\n" out "${out}")
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "input mode ${mode} failed (${status}):\n${out}\n${err}")
  endif()
  # A redirected read must not print the terminal prompt.
  if(NOT out MATCHES "^[0-9]+ input checks passed\n$" OR NOT err STREQUAL "")
    message(FATAL_ERROR "unexpected input output: [${out}] [${err}]")
  endif()
  message(STATUS "${mode}: ${out}")
endforeach()
file(REMOVE_RECURSE "${TMP}")
