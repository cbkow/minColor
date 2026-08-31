# Windows PiPL: MinColorCST_PiPL.r -> (cl /EP) -> .rr -> PiPLtool -> .rrc -> (cl /EP /D MSWindows) -> .rc
# Mirrors the "Compiling the PiPL" custom build step in the SDK's example .vcxproj files, run in
# CMake script mode so stdout redirection works identically under the VS and Ninja generators.
#   cmake -DCL=<cl.exe> -DPIPLTOOL=<PiPLtool.exe> -DINC=<SDK Headers> -DIN_R=<.r> -DOUT_RC=<.rc> -DWORK=<dir> -P BuildPiPL.cmake
foreach(v CL PIPLTOOL INC IN_R OUT_RC WORK)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "BuildPiPL.cmake: -D${v}= is required")
  endif()
endforeach()
get_filename_component(_name "${IN_R}" NAME_WE)
set(_rr  "${WORK}/${_name}.rr")
set(_rrc "${WORK}/${_name}.rrc")

# /TC: cl refuses unknown extensions (.r/.rrc) otherwise; /EP: preprocess to stdout, no #line
# INC2: optional second include dir (src/core for MincIds.h)
set(_inc2 "")
if(DEFINED INC2)
  set(_inc2 /I "${INC2}")
endif()
execute_process(COMMAND "${CL}" /nologo /TC /EP /I "${INC}" ${_inc2} "${IN_R}"
                OUTPUT_FILE "${_rr}" ERROR_VARIABLE _err RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "PiPL: cl /EP ${IN_R} failed (${_rc}):\n${_err}")
endif()
execute_process(COMMAND "${PIPLTOOL}" "${_rr}" "${_rrc}"
                OUTPUT_VARIABLE _out ERROR_VARIABLE _err RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "PiPL: PiPLtool failed (${_rc}):\n${_out}\n${_err}")
endif()
execute_process(COMMAND "${CL}" /nologo /TC /EP /D MSWindows "${_rrc}"
                OUTPUT_FILE "${OUT_RC}" ERROR_VARIABLE _err RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "PiPL: cl /EP ${_rrc} failed (${_rc}):\n${_err}")
endif()
