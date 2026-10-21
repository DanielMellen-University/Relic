# Shared by check_bootstrap.cmake and regen_bootstrap.cmake: the comment at
# the top of bootstrap/cgen.c. The rest of the file is exactly what cgen
# writes for selfhost/cgen.relic with --root set to the source folder.
set(RELIC_BOOTSTRAP_HEADER "/* bootstrap/cgen.c: stage 0 of the Relic compiler. Generated, do not edit.
 *
 * This is selfhost/cgen.relic, with every module it uses, compiled to C by
 * cgen itself. A fresh checkout builds Relic from this file with only a C
 * compiler and runtime/c (see bootstrap/README.md).
 *
 * After changing selfhost/ or std/, regenerate it from a configured build:
 *   cmake -DBUILD=build -P tools/regen_bootstrap.cmake
 * ctest bootstrap_fresh fails while this file is out of date.
 */
")

# Reads a file with line endings made LF.
function(relic_read_lf path var)
  file(READ ${path} text)
  string(REPLACE "\r\n" "\n" text "${text}")
  set(${var} "${text}" PARENT_SCOPE)
endfunction()
