# Generates MincBuildStamp.h (version + git SHA + dirty flag). Both bundles embed the SAME
# stamp per build — the M0 exit check greps it from both binaries. Write-if-changed so an
# unchanged stamp never triggers recompiles.
#   cmake -DOUT=<header> -DVERSION=<ver> -DSRC_DIR=<repo> -P BuildStamp.cmake
foreach(v OUT VERSION SRC_DIR)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "BuildStamp.cmake: -D${v}= is required")
  endif()
endforeach()
execute_process(COMMAND git -C "${SRC_DIR}" rev-parse --short HEAD
                OUTPUT_VARIABLE GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT GIT_SHA)
  set(GIT_SHA "nogit")
endif()
execute_process(COMMAND git -C "${SRC_DIR}" diff --quiet RESULT_VARIABLE _dirty ERROR_QUIET)
if(_dirty)
  set(GIT_SHA "${GIT_SHA}-dirty")
endif()
set(_content "#pragma once
#define MINC_VERSION_STR \"${VERSION}\"
#define MINC_BUILD_STAMP \"${VERSION} ${GIT_SHA}\"
")
if(EXISTS "${OUT}")
  file(READ "${OUT}" _old)
else()
  set(_old "")
endif()
if(NOT _content STREQUAL _old)
  file(WRITE "${OUT}" "${_content}")
endif()
