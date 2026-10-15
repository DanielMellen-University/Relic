cmake_minimum_required(VERSION 3.16)
project(relic VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

configure_file(src/version.hpp.in ${CMAKE_BINARY_DIR}/generated/version.hpp)

add_library(relic_core STATIC
  src/core.cpp
  src/diag/diag.cpp
  src/lexer/lexer.cpp
  src/ast/ast.cpp
  src/parser/parser.cpp
  src/interp/value.cpp
  src/interp/gc.cpp
  src/interp/interp.cpp
  src/builtins/builtins.cpp
  src/vm/vm.cpp
  src/builtins/graphics.cpp
)
target_include_directories(relic_core PUBLIC src ${CMAKE_BINARY_DIR}/generated)
# Graphics (M11): SDL2 is optional. Off by default so CI builds without it.
option(RELIC_SDL "Build the SDL2 graphics built-ins (std/graphics)" OFF)
if(RELIC_SDL)
  find_package(SDL2 REQUIRED)
  target_compile_definitions(relic_core PRIVATE RELIC_HAVE_SDL)
  if(TARGET SDL2::SDL2)
    target_link_libraries(relic_core PUBLIC SDL2::SDL2)
  else()
    target_include_directories(relic_core PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(relic_core PUBLIC ${SDL2_LIBRARIES})
  endif()
endif()
target_compile_definitions(relic_core PRIVATE RELIC_STD_DIR="${CMAKE_SOURCE_DIR}/std/")

add_executable(relic src/main.cpp)
target_link_libraries(relic PRIVATE relic_core)
if(MSVC)
  # The tree-walker recurses once per Relic call; the 1 MB Windows default is
  # too small for the 800-call limit. Match the 8 MB Linux/macOS default.
  target_link_options(relic PRIVATE /STACK:8388608)
endif()

enable_testing()

add_executable(relic_tests tests/unit/main.cpp)
target_link_libraries(relic_tests PRIVATE relic_core)
add_test(NAME unit COMMAND relic_tests)

file(GLOB GOLDEN_TESTS ${CMAKE_SOURCE_DIR}/tests/golden/*.relic)
foreach(t ${GOLDEN_TESTS})
  get_filename_component(name ${t} NAME_WE)
  add_test(NAME golden_${name}
    COMMAND ${CMAKE_COMMAND}
      -DRELIC=$<TARGET_FILE:relic>
      -DTEST=${t}
      -P ${CMAKE_SOURCE_DIR}/tools/run_golden.cmake)
endforeach()

if(RELIC_SDL)
  # Headless SDL smoke test: input, text and image calls run without a display.
  add_test(NAME gfx_smoke COMMAND relic gfx_smoke.relic WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/sdl)
  set_tests_properties(gfx_smoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "4 3\nnone\nbool bool\nint false\ndone")
endif()

add_test(NAME selfhost_lexer
  COMMAND ${CMAKE_COMMAND} -DRELIC=$<TARGET_FILE:relic> -DSRC=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/tools/compare_lexers.cmake)

add_test(NAME selfhost_parser
  COMMAND ${CMAKE_COMMAND} -DRELIC=$<TARGET_FILE:relic> -DSRC=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/tools/compare_parsers.cmake)

add_test(NAME selfhost_interp
  COMMAND ${CMAKE_COMMAND} -DRELIC=$<TARGET_FILE:relic> -DSRC=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/tools/compare_interp.cmake)

add_test(NAME vm_matches_tree_walker
  COMMAND ${CMAKE_COMMAND} -DRELIC=$<TARGET_FILE:relic> -DSRC=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/tools/compare_vm.cmake)
