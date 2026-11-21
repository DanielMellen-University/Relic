# relic fmt: golden files, idempotence, and every .relic file in the tree.
#   cmake -DRELIC=path/to/relic -DSRC=source/dir -DTMP=scratch/dir -P tools/check_fmt.cmake
set(FMT_COMMAND ${RELIC} fmt)
include(${SRC}/tools/check_fmt_cli.cmake)

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

set(FMT_FAULTS WRITE FLUSH RENAME CLOSE)
include(${SRC}/tools/check_fmt_write.cmake)
