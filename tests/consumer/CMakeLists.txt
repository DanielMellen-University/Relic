# A project outside the Relic tree that uses it the way a submodule would:
#   cmake -S tests/consumer -B build-consumer && cmake --build build-consumer
# CI builds this to check relic_add_program from a consuming project.
cmake_minimum_required(VERSION 3.16)
project(relic_consumer LANGUAGES C)
get_filename_component(relic_dir ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
add_subdirectory(${relic_dir} relic)
relic_add_program(hello hello.relic)
enable_testing()
add_test(NAME hello COMMAND hello)
set_tests_properties(hello PROPERTIES PASS_REGULAR_EXPRESSION "^hello from a consuming project\nHELLO\n$")
