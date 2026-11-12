# relicc's command line: exit codes 0 written, 1 a lex or parse error, 3 bad usage
# or a file that cannot be opened (the message names the file, not a std/ call).
#   cmake -DRELICC=path/to/relicc -DCC=path/to/cc -DSRC=source/dir -DTMP=scratch/dir -P tools/check_relicc_cli.cmake
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

# --stub emits a standalone C program for a syntax error, suitable for a build
# that compiles every input even when relicc refuses one.
file(WRITE ${TMP}/stub_bad.relic "let x as integer = 1\n")
execute_process(COMMAND ${RELICC} stub_bad.relic -o stub_bad.c --stub WORKING_DIRECTORY ${TMP}
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
if(NOT got STREQUAL "0" OR NOT out STREQUAL "" OR NOT err STREQUAL "" OR NOT EXISTS ${TMP}/stub_bad.c)
  message(FATAL_ERROR "--stub: exit ${got}\n${out}${err}")
endif()
set(stub_project "${TMP}/stub_project")
file(MAKE_DIRECTORY "${stub_project}")
file(WRITE "${stub_project}/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.16)\nproject(stub C)\nadd_executable(stub \"${TMP}/stub_bad.c\")\nfile(GENERATE OUTPUT \"${stub_project}/stub_path_$<CONFIG>.txt\" CONTENT \"$<TARGET_FILE:stub>\")\n")
set(configure_args -S "${stub_project}" -B "${stub_project}/build"
  -DCMAKE_C_COMPILER=${CC} -DCMAKE_BUILD_TYPE=${CONFIG})
if(GENERATOR)
  list(APPEND configure_args -G "${GENERATOR}")
endif()
if(PLATFORM)
  list(APPEND configure_args -A "${PLATFORM}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} ${configure_args}
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
if(NOT got STREQUAL "0")
  message(FATAL_ERROR "--stub configure failed: ${out}${err}")
endif()
set(build_args --build "${stub_project}/build" -j4)
if(CONFIG)
  list(APPEND build_args --config "${CONFIG}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} ${build_args}
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
if(NOT got STREQUAL "0")
  message(FATAL_ERROR "--stub did not compile: ${out}${err}")
endif()
file(READ "${stub_project}/stub_path_${CONFIG}.txt" stub_exe)
string(STRIP "${stub_exe}" stub_exe)
execute_process(COMMAND "${stub_exe}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
set(stub_error "stub_bad.relic:1:10: error: unknown type 'integer'; use any, none, bool, int, float, number, string, list, map, function or module\n    let x as integer = 1\n             ^^^^^^^\nhint: did you mean 'int'?\n")
if(NOT got STREQUAL "1" OR NOT out STREQUAL "" OR NOT err STREQUAL stub_error)
  message(FATAL_ERROR "compiled --stub: exit ${got}\n--- stderr\n${err}--- expected\n${stub_error}")
endif()

# Compile-error diagnostics in tests/diag_cases: relicc's stderr matches the
# .err file byte for byte (the same text the host prints).
file(GLOB diag_cases ${SRC}/tests/diag_cases/*.relic)
foreach(f ${diag_cases})
  get_filename_component(dir ${f} DIRECTORY)
  get_filename_component(name ${f} NAME)
  get_filename_component(stem ${f} NAME_WE)
  file(READ ${dir}/${stem}.err expected)
  string(REPLACE "\r\n" "\n" expected "${expected}")
  execute_process(COMMAND ${RELICC} ${name} -o ${TMP}/diag_${stem}.c WORKING_DIRECTORY ${dir}
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE got)
  string(REPLACE "\r\n" "\n" err "${err}")
  if(NOT got STREQUAL "1" OR NOT out STREQUAL "" OR NOT err STREQUAL expected)
    message(FATAL_ERROR "diag ${stem}: exit ${got}\n--- stderr\n${err}--- expected\n${expected}")
  endif()
endforeach()
