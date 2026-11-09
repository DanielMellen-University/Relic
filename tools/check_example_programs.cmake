# Runs the runnable examples and checks each against its own expected stdout.
#   cmake -DRELIC=path/to/relic -DSRC=source/dir -DTMP=dir -P tools/check_example_programs.cmake
#
# A program is any examples/learn/*.relic, any examples/ref/*/*.relic, and
# examples/word_count.relic, each with a sibling .out. Optional .in is stdin.
# Each one runs in its own empty folder, so a files demo can write without
# touching the tree. A learn program whose compiled twin is tests/cgen/learn_*.out
# must carry the same bytes there: the cgen case and this one stay in step.
# Helper modules live in a subfolder and have no .out, so they are not run.
file(MAKE_DIRECTORY ${TMP})

function(run_one label src)
  get_filename_component(dir ${src} DIRECTORY)
  get_filename_component(name ${src} NAME_WE)
  set(out_file ${dir}/${name}.out)
  if(NOT EXISTS ${out_file})
    message(FATAL_ERROR "${label}: missing ${name}.out")
  endif()
  file(READ ${out_file} want)
  string(REPLACE "\r\n" "\n" want "${want}")
  set(stdin_file ${dir}/${name}.in)
  if(NOT EXISTS ${stdin_file})
    set(stdin_file ${SRC}/tools/empty.in)
  endif()
  set(work ${TMP}/ex/${label})
  file(MAKE_DIRECTORY ${work})
  execute_process(COMMAND ${RELIC} ${src}
    INPUT_FILE ${stdin_file}
    WORKING_DIRECTORY ${work}
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE code)
  string(REPLACE "\r\n" "\n" out "${out}")
  string(REPLACE "\r\n" "\n" err "${err}")
  if(NOT code EQUAL 0 OR NOT out STREQUAL want OR NOT err STREQUAL "")
    message(FATAL_ERROR "${label}: exit ${code}\n--- stdout want\n${want}--- stdout got\n${out}--- stderr\n${err}")
  endif()
endfunction()

set(ran 0)
file(GLOB learn ${SRC}/examples/learn/*.relic)
foreach(src ${learn})
  get_filename_component(name ${src} NAME_WE)
  run_one("learn_${name}" ${src})
  math(EXPR ran "${ran} + 1")
endforeach()

file(GLOB_RECURSE ref ${SRC}/examples/ref/*.relic)
foreach(src ${ref})
  file(RELATIVE_PATH rel ${SRC}/examples/ref ${src})
  string(REPLACE "/" "_" label ${rel})
  string(REPLACE ".relic" "" label ${label})
  run_one("ref_${label}" ${src})
  math(EXPR ran "${ran} + 1")
endforeach()

if(EXISTS ${SRC}/examples/word_count.out)
  run_one("word_count" ${SRC}/examples/word_count.relic)
  math(EXPR ran "${ran} + 1")
endif()

if(ran LESS 1)
  message(FATAL_ERROR "no example programs with expected output")
endif()

# The compiled learn cases (tests/cgen/learn_*.relic) must expect the same stdout
# the source example prints. Each wrapper is `use ../../examples/<path>`.
file(GLOB wrappers ${SRC}/tests/cgen/learn_*.relic)
set(wrapped 0)
foreach(w ${wrappers})
  file(READ ${w} text)
  if(NOT text MATCHES "use \\.\\./\\.\\./examples/([A-Za-z0-9_/]+)")
    message(FATAL_ERROR "${w} should use an examples/ program")
  endif()
  set(rel ${CMAKE_MATCH_1})
  get_filename_component(wdir ${w} DIRECTORY)
  get_filename_component(wname ${w} NAME_WE)
  file(READ ${SRC}/examples/${rel}.out source_out)
  file(READ ${wdir}/${wname}.out wrapped_out)
  string(REPLACE "\r\n" "\n" source_out "${source_out}")
  string(REPLACE "\r\n" "\n" wrapped_out "${wrapped_out}")
  if(NOT source_out STREQUAL wrapped_out)
    message(FATAL_ERROR "${wname}.out does not match examples/${rel}.out")
  endif()
  math(EXPR wrapped "${wrapped} + 1")
endforeach()
if(wrapped LESS 1)
  message(FATAL_ERROR "no compiled learn examples")
endif()
message("${ran} example programs match their expected output (${wrapped} also compiled)")
