cmake_minimum_required(VERSION 3.16)
project(relic VERSION 1.0.3 LANGUAGES CXX)

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
  # Optional extras: PNG (and JPG) images via SDL2_image, TrueType text via SDL2_ttf.
  find_package(SDL2_image CONFIG QUIET)
  if(TARGET SDL2_image::SDL2_image)
    target_compile_definitions(relic_core PRIVATE RELIC_HAVE_SDL_IMAGE)
    target_link_libraries(relic_core PUBLIC SDL2_image::SDL2_image)
    message(STATUS "Relic graphics: PNG images on (SDL2_image)")
  endif()
  find_package(SDL2_ttf CONFIG QUIET)
  if(TARGET SDL2_ttf::SDL2_ttf)
    target_compile_definitions(relic_core PRIVATE RELIC_HAVE_SDL_TTF)
    target_link_libraries(relic_core PUBLIC SDL2_ttf::SDL2_ttf)
    message(STATUS "Relic graphics: TrueType fonts on (SDL2_ttf)")
  endif()
endif()
target_compile_definitions(relic_core PRIVATE RELIC_STD_DIR="${CMAKE_SOURCE_DIR}/std/")

add_executable(relic src/main.cpp)
target_link_libraries(relic PRIVATE relic_core)
if(MSVC)
  # The tree-walker recurses once per Relic call; the 1 MB Windows default is
  # too small for the 800-call limit. MSVC frames run larger than GCC and
  # Clang ones, so give it 16 MB (Linux and macOS default to 8 MB).
  target_link_options(relic PRIVATE /STACK:16777216)
endif()

enable_testing()

add_executable(relic_tests tests/unit/main.cpp)
target_link_libraries(relic_tests PRIVATE relic_core)
add_test(NAME unit COMMAND relic_tests)

# The C runtime for compiled Relic (C1). Built next to the C++ host, not instead of it.
option(RELIC_C_RUNTIME "Build the C runtime in runtime/c" ON)
if(RELIC_C_RUNTIME)
  add_subdirectory(runtime/c)
endif()

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
  # Event queue (simulated keys, text, clicks, wheel, resize), extra drawing, clipboard.
  add_test(NAME gfx_events COMMAND relic gfx_events.relic WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/sdl)
  set_tests_properties(gfx_events PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "^\\[80, 60\\]\nkey a false\ntext h[^\n]+\nmouse_down 1 12 34\nmouse_up 1 12 34\nwheel -2\nresize 100 70\na\n30 20\nstring\ndone")

  add_test(NAME gfx_simulate COMMAND relic gfx_simulate.relic WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/sdl)
  set_tests_properties(gfx_simulate PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "^key_down a true true false\nkey_up a true false false\nmove 40 50 0 0\ndown 1 11 22\nup 1 11 22\nquit true\ndone")
  # Mini desktop example: multi-app event loop, headless.
  add_test(NAME mini_desktop COMMAND relic desktop.relic WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/examples/mini_desktop)
  set_tests_properties(mini_desktop PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy"
    PASS_REGULAR_EXPRESSION "hi\n> ls data\nsettings.json\n> cat nope.txt\nerror: files.read..: cannot open nope.txt\nnotes: hello notes saved\nfocus: 0")
  if(TARGET SDL2_image::SDL2_image)
    add_test(NAME gfx_png COMMAND relic gfx_png.relic WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/sdl)
    set_tests_properties(gfx_png PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy"
      PASS_REGULAR_EXPRESSION "^true\n5 4\n")
  endif()
  find_file(RELIC_TEST_FONT NAMES DejaVuSans.ttf
    PATHS /usr/share/fonts/truetype/dejavu /usr/share/fonts/TTF /Library/Fonts C:/Windows/Fonts NO_DEFAULT_PATH)
  if(TARGET SDL2_ttf::SDL2_ttf AND RELIC_TEST_FONT)
    add_test(NAME gfx_ttf COMMAND relic gfx_ttf.relic ${RELIC_TEST_FONT} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/sdl)
    set_tests_properties(gfx_ttf PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy"
      PASS_REGULAR_EXPRESSION "^true\nwide true\ntall true\ndone")
  endif()
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

# Self-host: args reach the program, and run.relic can run itself (two levels).
add_test(NAME selfhost_args
  COMMAND relic ${CMAKE_SOURCE_DIR}/selfhost/run.relic ${CMAKE_SOURCE_DIR}/tests/selfhost/args.relic one --two)
set_tests_properties(selfhost_args PROPERTIES
  PASS_REGULAR_EXPRESSION "^args \\[\"one\", \"--two\"\\] \\[\"one\", \"--two\"\\]")
add_test(NAME selfhost_nested
  COMMAND relic ${CMAKE_SOURCE_DIR}/selfhost/run.relic ${CMAKE_SOURCE_DIR}/selfhost/run.relic ${CMAKE_SOURCE_DIR}/tests/selfhost/args.relic deep)
set_tests_properties(selfhost_nested PROPERTIES
  PASS_REGULAR_EXPRESSION "^args \\[\"deep\"\\] \\[\"deep\"\\]")

add_test(NAME vm_matches_tree_walker
  COMMAND ${CMAKE_COMMAND} -DRELIC=$<TARGET_FILE:relic> -DSRC=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/tools/compare_vm.cmake)
