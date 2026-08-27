cmake_minimum_required(VERSION 3.24)
project(minColorCST LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64")   # OCIO ext deps built arm64-only; universal = distribution task (see plan risks)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")

# --- After Effects SDK (drop the unzipped SDK into plugin/sdk/) ---
set(AE_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/sdk" CACHE PATH "After Effects SDK root")
file(GLOB _ae_sdk_candidates "${AE_SDK_ROOT}" "${AE_SDK_ROOT}/*")
set(AE_SDK_HEADERS "")
foreach(c ${_ae_sdk_candidates})
  if(EXISTS "${c}/Examples/Headers/AE_Effect.h")
    set(AE_SDK_HEADERS "${c}/Examples/Headers")
    set(AE_SDK_EXAMPLES "${c}/Examples")
  elseif(EXISTS "${c}/Headers/AE_Effect.h")
    set(AE_SDK_HEADERS "${c}/Headers")
    get_filename_component(AE_SDK_EXAMPLES "${c}" DIRECTORY)
  endif()
endforeach()
if(NOT AE_SDK_HEADERS)
  message(FATAL_ERROR "After Effects SDK not found under ${AE_SDK_ROOT} — unzip the SDK there (needs Examples/Headers/AE_Effect.h)")
endif()
message(STATUS "AE SDK headers: ${AE_SDK_HEADERS}")

# --- vendored static OCIO (plugin/external/build.sh) ---
set(OCIO_INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/external/install")
if(NOT EXISTS "${OCIO_INSTALL}/lib")
  message(FATAL_ERROR "OCIO not built — run plugin/external/build.sh first")
endif()
list(PREPEND CMAKE_PREFIX_PATH "${OCIO_INSTALL}")
find_package(OpenColorIO REQUIRED CONFIG)

add_library(minColorCST MODULE
  src/MinColorCST.cpp
  src/Arb.cpp
  src/Authority.cpp
  src/OcioEngine.cpp
  src/SmartRender.cpp
  src/Ui.cpp
  "${AE_SDK_EXAMPLES}/Util/AEFX_SuiteHelper.c"
  "${AE_SDK_EXAMPLES}/Util/AEGP_SuiteHandler.cpp"
  "${AE_SDK_EXAMPLES}/Util/MissingSuiteError.cpp"
  "${AE_SDK_EXAMPLES}/Util/Smart_Utils.cpp"
  "${AE_SDK_EXAMPLES}/Util/AEGP_Utils.cpp"
)
target_include_directories(minColorCST PRIVATE
  "${AE_SDK_HEADERS}"
  "${AE_SDK_HEADERS}/SP"
  "${AE_SDK_EXAMPLES}/Util"
  src
)
file(GLOB OCIO_EXT_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/external/build/ext/dist/lib/*.a")
target_link_libraries(minColorCST PRIVATE OpenColorIO::OpenColorIO ${OCIO_EXT_LIBS})
set_target_properties(minColorCST PROPERTIES
  BUNDLE TRUE
  BUNDLE_EXTENSION "plugin"
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  OUTPUT_NAME "minColorCST"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist.in"
)
# export ONLY the effect entry point (the static OCIO otherwise leaks thousands of symbols)
target_link_options(minColorCST PRIVATE "-Wl,-exported_symbol,_EffectMain" "-Wl,-dead_strip")

# --- PiPL: .r -> .rsrc via Rez (fallback: commit the generated rsrc) ---
find_program(REZ_TOOL Rez)
set(PIPL_RSRC "${CMAKE_CURRENT_BINARY_DIR}/minColorCST.rsrc")
add_custom_command(OUTPUT "${PIPL_RSRC}"
  COMMAND ${REZ_TOOL} "${CMAKE_CURRENT_SOURCE_DIR}/src/MinColorCST_PiPL.r"
          -o "${PIPL_RSRC}" -useDF
          -i "${AE_SDK_HEADERS}" -i "${AE_SDK_EXAMPLES}/Resources" -i "${AE_SDK_EXAMPLES}/Util"
  DEPENDS src/MinColorCST_PiPL.r
  COMMENT "Rez PiPL")
add_custom_target(pipl DEPENDS "${PIPL_RSRC}")
add_dependencies(minColorCST pipl)
add_custom_command(TARGET minColorCST POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:minColorCST>/Contents/Resources"
  COMMAND ${CMAKE_COMMAND} -E copy "${PIPL_RSRC}" "$<TARGET_BUNDLE_DIR:minColorCST>/Contents/Resources/minColorCST.rsrc")
