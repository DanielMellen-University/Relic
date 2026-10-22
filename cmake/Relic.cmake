# relic_add_program(name file [GRAPHICS] [DEPENDS extra_files...])
#
# Builds the Relic program `file` into an executable target `name`: relicc
# compiles it, with every module it uses, to one C file in the build folder,
# which is built with the C runtime (runtime/c) and its strict flags.
#
# Works inside the Relic tree and in any CMake project that adds Relic with
# add_subdirectory (for example a relic/ submodule):
#
#   add_subdirectory(relic)
#   relic_add_program(hello src/hello.relic)
#
# Rebuilds: the C is written again when relicc changes, when `file` changes,
# or when any .relic file under the folder of `file` (subfolders included)
# or in Relic's std/ changes. A module outside that folder (`use ../x`) is not
# seen on its own; list it under DEPENDS. Files in the build folder are left
# out. relicc follows `use` itself, so this only decides when it runs again.
#
# GRAPHICS links the program with relic_rt_gfx, the runtime with the SDL2
# graphics module, so std/graphics opens real windows. That library exists
# when Relic is configured with -DRELIC_C_SDL=ON. Without it the program
# links relic_rt as usual: std/graphics loads and available() is false.
# Programs without GRAPHICS never link SDL2.
#
# The program finds std/ at the path of the Relic tree it was built from
# ($RELIC_STD overrides that at run time).
get_filename_component(RELIC_ROOT ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
set(RELIC_ROOT ${RELIC_ROOT} CACHE INTERNAL "Relic source tree")

function(relic_add_program name file)
  cmake_parse_arguments(ARG "GRAPHICS" "" "DEPENDS" ${ARGN})
  if(NOT TARGET relicc)
    message(FATAL_ERROR "relic_add_program: no relicc target; add Relic with add_subdirectory first")
  endif()
  get_filename_component(src ${file} ABSOLUTE)
  if(NOT EXISTS ${src})
    message(FATAL_ERROR "relic_add_program(${name}): ${src} not found")
  endif()
  get_filename_component(dir ${src} DIRECTORY)
  file(GLOB_RECURSE near CONFIGURE_DEPENDS ${dir}/*.relic)
  file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}" bin)
  list(FILTER near EXCLUDE REGEX "^${bin}/")
  file(GLOB std CONFIGURE_DEPENDS ${RELIC_ROOT}/std/*.relic)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/relic_programs)
  set(c ${CMAKE_CURRENT_BINARY_DIR}/relic_programs/${name}.c)
  add_custom_command(OUTPUT ${c}
    COMMAND relicc ${src} -o ${c}
    DEPENDS relicc ${src} ${near} ${std} ${ARG_DEPENDS}
    COMMENT "relicc ${file}"
    VERBATIM)
  add_executable(${name} ${c})
  if(ARG_GRAPHICS AND TARGET relic_rt_gfx)
    target_link_libraries(${name} PRIVATE relic_rt_gfx)
  else()
    if(ARG_GRAPHICS)
      message(STATUS "relic_add_program(${name}): no SDL2 graphics (configure with -DRELIC_C_SDL=ON); using the stub")
    endif()
    target_link_libraries(${name} PRIVATE relic_rt)
  endif()
  relic_c_target(${name})
endfunction()
