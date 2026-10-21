cmake_minimum_required(VERSION 3.16)
project(relic VERSION 1.0.7 LANGUAGES C)

# C5: the default build needs only a C compiler. bootstrap/cgen.c (cgen
# compiled to C by itself) is built with runtime/c into relicc_stage0, which
# compiles selfhost/cgen.relic from this checkout into relicc (stage 1);
# relicc compiles programs to C. The C++ host (relic: tree-walker, VM, REPL,
# graphics) is optional and is used to cross-check the compiled side.
option(RELIC_BUILD_HOST "Also build the C++ host (relic) and its tests" OFF)
option(RELIC_SDL "Build the SDL2 graphics built-ins (std/graphics); needs RELIC_BUILD_HOST" OFF)
option(RELIC_C_RUNTIME "Build the C runtime in runtime/c and the C compiler on it" ON)
option(RELIC_CGEN_TESTS "Compile golden programs to C and run them" ON)
if(RELIC_SDL AND NOT RELIC_BUILD_HOST)
  message(FATAL_ERROR "RELIC_SDL needs -DRELIC_BUILD_HOST=ON")
endif()
if(NOT RELIC_C_RUNTIME AND NOT RELIC_BUILD_HOST)
  message(FATAL_ERROR "nothing to build: turn on RELIC_C_RUNTIME or RELIC_BUILD_HOST")
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

file(GLOB GOLDEN_TESTS ${CMAKE_SOURCE_DIR}/tests/golden/*.relic)

if(RELIC_BUILD_HOST)
  enable_language(CXX)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)

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

  add_executable(relic_tests tests/unit/main.cpp)
  target_link_libraries(relic_tests PRIVATE relic_core)
  add_test(NAME unit COMMAND relic_tests)

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
endif() # RELIC_BUILD_HOST

# ---- C: runtime, bootstrap compiler, compiled goldens ----
if(RELIC_C_RUNTIME)
  add_subdirectory(runtime/c)
  set(RELIC_BOOT ${CMAKE_BINARY_DIR}/bootstrap)
  file(MAKE_DIRECTORY ${RELIC_BOOT} ${CMAKE_BINARY_DIR}/cgen)
  file(GLOB RELIC_CGEN_DEPS ${CMAKE_SOURCE_DIR}/selfhost/*.relic ${CMAKE_SOURCE_DIR}/std/*.relic)

  # Stage 0: the committed C. Stage 1: cgen from this checkout, compiled by
  # stage 0. relicc is the compiler a build uses: relicc FILE -o OUT.c
  add_executable(relicc_stage0 ${CMAKE_SOURCE_DIR}/bootstrap/cgen.c)
  target_link_libraries(relicc_stage0 PRIVATE relic_rt)
  relic_c_target(relicc_stage0)
  add_custom_command(OUTPUT ${RELIC_BOOT}/stage1.c
    COMMAND relicc_stage0 ${CMAKE_SOURCE_DIR}/selfhost/cgen.relic --root ${CMAKE_SOURCE_DIR} -o ${RELIC_BOOT}/stage1.c
    DEPENDS relicc_stage0 ${RELIC_CGEN_DEPS}
    COMMENT "stage 1: relicc_stage0 selfhost/cgen.relic"
    VERBATIM)
  add_executable(relicc ${RELIC_BOOT}/stage1.c)
  target_link_libraries(relicc PRIVATE relic_rt)
  relic_c_target(relicc)

  # bootstrap/cgen.c must be what cgen writes from the current sources.
  add_test(NAME bootstrap_fresh
    COMMAND ${CMAKE_COMMAND} -DTOOL=$<TARGET_FILE:relicc_stage0> -DSRC=${CMAKE_SOURCE_DIR}
      -DOUT=${RELIC_BOOT}/fresh.c -P ${CMAKE_SOURCE_DIR}/tools/check_bootstrap.cmake)
  # Stage 2: relicc compiling cgen writes the C it was built from.
  add_test(NAME bootstrap_fixed_point
    COMMAND ${CMAKE_COMMAND} -DSTAGE1=$<TARGET_FILE:relicc> -DSRC=${CMAKE_SOURCE_DIR} -DROOT=${CMAKE_SOURCE_DIR}
      -DSTAGE0_C=${RELIC_BOOT}/stage1.c -DOUT=${RELIC_BOOT}/stage2.c
      -P ${CMAKE_SOURCE_DIR}/tools/cgen_fixed_point.cmake)
  if(RELIC_BUILD_HOST)
    # Cross-check: cgen run on the host writes the committed C too.
    add_test(NAME bootstrap_matches_host
      COMMAND ${CMAKE_COMMAND} -DTOOL=$<TARGET_FILE:relic> -DSCRIPT=${CMAKE_SOURCE_DIR}/selfhost/cgen.relic
        -DSRC=${CMAKE_SOURCE_DIR} -DOUT=${RELIC_BOOT}/host.c -P ${CMAKE_SOURCE_DIR}/tools/check_bootstrap.cmake)
  endif()
endif()

# Golden programs compiled to C, built against runtime/c with the same strict
# flags, run and compared with the same .out/.err files. REPL tests and tests
# with .args (CLI flags) are left out. Without the host, relicc compiles
# them; with the host, cgen.relic runs on the host (as before C5) and
# cgen_stage1_matches_host checks relicc writes the same C for each.
# relic_cgen_test(name file) works for any program with a golden.
if(RELIC_C_RUNTIME AND RELIC_CGEN_TESTS)
  # relic_cgen_exe(name src): src compiled to C and built as cgen_<name>.
  function(relic_cgen_exe name src)
    set(c ${CMAKE_BINARY_DIR}/cgen/${name}.c)
    if(RELIC_BUILD_HOST)
      add_custom_command(OUTPUT ${c}
        COMMAND relic ${CMAKE_SOURCE_DIR}/selfhost/cgen.relic ${src} -o ${c} --stub
        DEPENDS relic ${src} ${RELIC_CGEN_DEPS}
        COMMENT "cgen ${name}.relic"
        VERBATIM)
    else()
      add_custom_command(OUTPUT ${c}
        COMMAND relicc ${src} -o ${c} --stub
        DEPENDS relicc ${src} ${RELIC_CGEN_DEPS}
        COMMENT "relicc ${name}.relic"
        VERBATIM)
    endif()
    add_executable(cgen_${name} ${c})
    target_link_libraries(cgen_${name} PRIVATE relic_rt)
    relic_c_target(cgen_${name})
  endfunction()
  function(relic_cgen_test name src)
    relic_cgen_exe(${name} ${src})
    add_test(NAME cgen_${name}
      COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:cgen_${name}> -DTEST=${src}
        -P ${CMAKE_SOURCE_DIR}/tools/run_cgen.cmake)
    set_tests_properties(cgen_${name} PROPERTIES SKIP_REGULAR_EXPRESSION "cgen-skip: ")
  endfunction()
  foreach(t ${GOLDEN_TESTS})
    get_filename_component(name ${t} NAME_WE)
    get_filename_component(dir ${t} DIRECTORY)
    if(NOT name MATCHES "^repl_" AND NOT EXISTS ${dir}/${name}.args)
      relic_cgen_test(${name} ${t})
    endif()
  endforeach()
  # Cases for the compiled side only (expected output from relic --tree).
  file(GLOB CGEN_ONLY_TESTS ${CMAKE_SOURCE_DIR}/tests/cgen/*.relic)
  foreach(t ${CGEN_ONLY_TESTS})
    get_filename_component(name ${t} NAME_WE)
    relic_cgen_test(only_${name} ${t})
  endforeach()
  if(RELIC_BUILD_HOST)
    add_test(NAME cgen_stage1_matches_host
      COMMAND ${CMAKE_COMMAND} -DTOOL=$<TARGET_FILE:relicc> -DSRC=${CMAKE_SOURCE_DIR}
        -DHOST_C=${CMAKE_BINARY_DIR}/cgen -DOUT=${RELIC_BOOT}/goldens
        -P ${CMAKE_SOURCE_DIR}/tools/compare_stage1_host.cmake)
    # The Relic-written lexer, parser and code generator compiled to C by
    # cgen on the host. The compiled lexer and parser must match --tokens
    # and --ast on every file; the compiled cgen must write exactly the C
    # that cgen on the host wrote for it (a fixed point).
    relic_cgen_exe(selfhost_tokens ${CMAKE_SOURCE_DIR}/selfhost/tokens.relic)
    relic_cgen_exe(selfhost_ast ${CMAKE_SOURCE_DIR}/selfhost/ast.relic)
    relic_cgen_exe(selfhost_cgen ${CMAKE_SOURCE_DIR}/selfhost/cgen.relic)
    add_test(NAME cgen_selfhost_lexer
      COMMAND ${CMAKE_COMMAND} -DRELIC=$<TARGET_FILE:relic> -DSRC=${CMAKE_SOURCE_DIR}
        -DTOOL=$<TARGET_FILE:cgen_selfhost_tokens> -P ${CMAKE_SOURCE_DIR}/tools/compare_lexers.cmake)
    add_test(NAME cgen_selfhost_parser
      COMMAND ${CMAKE_COMMAND} -DRELIC=$<TARGET_FILE:relic> -DSRC=${CMAKE_SOURCE_DIR}
        -DTOOL=$<TARGET_FILE:cgen_selfhost_ast> -P ${CMAKE_SOURCE_DIR}/tools/compare_parsers.cmake)
    add_test(NAME cgen_fixed_point
      COMMAND ${CMAKE_COMMAND} -DSTAGE1=$<TARGET_FILE:cgen_selfhost_cgen> -DSRC=${CMAKE_SOURCE_DIR}
        -DSTAGE0_C=${CMAKE_BINARY_DIR}/cgen/selfhost_cgen.c -DOUT=${CMAKE_BINARY_DIR}/cgen/selfhost_cgen.stage2.c
        -P ${CMAKE_SOURCE_DIR}/tools/cgen_fixed_point.cmake)
  endif()
endif()
