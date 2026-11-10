# Redirect standard input on every platform, including Windows.
# The fixtures are checked in byte-exact (*.in is -text). file(WRITE) uses
# text mode on Windows and would turn each \n into \r\n.
foreach(mode lines cr empty)
  execute_process(COMMAND "${EXE}" "${mode}" INPUT_FILE "${IN}/${mode}.in"
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
