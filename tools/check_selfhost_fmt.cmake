# The same CLI and goldens on a compiled, interpreted or nested formatter.
if(TOOL)
  set(FMT_COMMAND ${TOOL})
elseif(NESTED)
  set(FMT_COMMAND ${RELIC} ${SRC}/selfhost/run.relic ${SRC}/selfhost/fmt.relic)
else()
  set(FMT_COMMAND ${RELIC} ${SRC}/selfhost/fmt.relic)
endif()
include(${SRC}/tools/check_fmt_cli.cmake)
# files.write checks the buffered write and fclose. The host also explicitly
# calls fflush, covered by the existing fmt test's FLUSH case.
set(FMT_FAULTS WRITE RENAME CLOSE)
include(${SRC}/tools/check_fmt_write.cmake)
