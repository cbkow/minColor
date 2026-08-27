cmake_minimum_required(VERSION 3.24)
project(minColorCST LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64")   # OCIO ext deps built arm64-only; universal = distribution task (see plan risks)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")
if(WIN32)
  enable_language(RC)                                    # the PiPL is a compiled .rc on Windows
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")     # /MD in every config: matches the static OCIO and AE itself
endif()

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

# --- vendored static OCIO (plugin/external/build.sh on macOS, build.bat on Windows) ---
set(OCIO_INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/external/install")
if(NOT EXISTS "${OCIO_INSTALL}/lib")
  message(FATAL_ERROR "OCIO not built — run plugin/external/build.sh (macOS) or build.bat (Windows) first")
endif()
list(PREPEND CMAKE_PREFIX_PATH "${OCIO_INSTALL}")
if(WIN32)
  # OpenColorIOConfig.cmake find_dependency()s the ext packages it was built against
  list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/build/ext/dist")
endif()
find_package(OpenColorIO REQUIRED CONFIG)

add_library(minColorCST MODULE
  src/MinColorCST.cpp
  src/Arb.cpp
  src/Authority.cpp
  src/Passport.cpp
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
if(WIN32)
  file(GLOB OCIO_EXT_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/external/build/ext/dist/lib/*.lib")
else()
  file(GLOB OCIO_EXT_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/external/build/ext/dist/lib/*.a")
endif()
target_link_libraries(minColorCST PRIVATE OpenColorIO::OpenColorIO ${OCIO_EXT_LIBS})
set_target_properties(minColorCST PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  OUTPUT_NAME "minColorCST"
)

if(APPLE)
  set_target_properties(minColorCST PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION "plugin"
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
elseif(WIN32)
  # a .aex is a plain DLL; EffectMain is the only __declspec(dllexport) (static OCIO exports nothing)
  set_target_properties(minColorCST PROPERTIES PREFIX "" SUFFIX ".aex")
  target_include_directories(minColorCST PRIVATE "${AE_SDK_HEADERS}/Win" "${AE_SDK_EXAMPLES}/Resources")
  target_compile_definitions(minColorCST PRIVATE MSWindows WIN32 _WINDOWS _CRT_SECURE_NO_WARNINGS)

  # --- PiPL: .r -> .rc via the SDK's cl/PiPLtool pipeline (cmake/BuildPiPL.cmake), compiled into the DLL ---
  set(PIPL_RC "${CMAKE_CURRENT_BINARY_DIR}/minColorCST_PiPL.rc")
  add_custom_command(OUTPUT "${PIPL_RC}"
    COMMAND ${CMAKE_COMMAND}
            -DCL=${CMAKE_CXX_COMPILER}
            -DPIPLTOOL=${AE_SDK_EXAMPLES}/Resources/PiPLtool.exe
            -DINC=${AE_SDK_HEADERS}
            -DIN_R=${CMAKE_CURRENT_SOURCE_DIR}/src/MinColorCST_PiPL.r
            -DOUT_RC=${PIPL_RC}
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildPiPL.cmake"
    DEPENDS src/MinColorCST_PiPL.r cmake/BuildPiPL.cmake
    COMMENT "PiPL (cl /EP -> PiPLtool -> rc)"
    VERBATIM)
  target_sources(minColorCST PRIVATE "${PIPL_RC}")

  # --- version.txt beside the .aex: the panel's Windows contract for pluginBundleVersion()
  #     (macOS reads CFBundleVersion from the bundle plist; Info.plist.in is the single source) ---
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist.in" _plist)
  string(REGEX MATCH "CFBundleVersion</key>[ 	]*<string>([^<]+)</string>" _m "${_plist}")
  set(MINC_PLUGIN_VERSION "${CMAKE_MATCH_1}")
  if(NOT MINC_PLUGIN_VERSION)
    message(FATAL_ERROR "CFBundleVersion not found in src/Info.plist.in")
  endif()
  message(STATUS "plugin version (version.txt): ${MINC_PLUGIN_VERSION}")
  add_custom_command(TARGET minColorCST POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "${MINC_PLUGIN_VERSION}" > "$<TARGET_FILE_DIR:minColorCST>/version.txt"
    COMMENT "version.txt = ${MINC_PLUGIN_VERSION}" VERBATIM)
endif()
