# Capture process output in files because CMake strings cannot preserve NUL.
function(compare_capture prefix)
  execute_process(COMMAND ${ARGN}
    OUTPUT_FILE "${COMPARE_TMP}/${prefix}.out"
    ERROR_FILE "${COMPARE_TMP}/${prefix}.err"
    RESULT_VARIABLE code)
  set(CAPTURE_${prefix}_CODE "${code}" PARENT_SCOPE)
endfunction()

function(captures_equal left right out_var)
  file(SHA256 "${COMPARE_TMP}/${left}.out" left_out)
  file(SHA256 "${COMPARE_TMP}/${right}.out" right_out)
  file(SHA256 "${COMPARE_TMP}/${left}.err" left_err)
  file(SHA256 "${COMPARE_TMP}/${right}.err" right_err)
  if(left_out STREQUAL right_out AND left_err STREQUAL right_err AND
     CAPTURE_${left}_CODE STREQUAL CAPTURE_${right}_CODE)
    set(${out_var} TRUE PARENT_SCOPE)
  else()
    set(${out_var} FALSE PARENT_SCOPE)
  endif()
endfunction()
