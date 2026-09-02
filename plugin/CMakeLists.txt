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

# --- 2.0: one version + build stamp for BOTH bundles (skew between them is impossible by
#     construction; the M0 exit check greps the identical stamp from both binaries) ---
set(MINC_VERSION "2.0.0-beta.1")
set(MINC_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
file(MAKE_DIRECTORY "${MINC_GEN_DIR}")
add_custom_target(buildstamp ALL
  COMMAND ${CMAKE_COMMAND} -DOUT=${MINC_GEN_DIR}/MincBuildStamp.h -DVERSION=${MINC_VERSION}
          -DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/.. -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildStamp.cmake"
  BYPRODUCTS "${MINC_GEN_DIR}/MincBuildStamp.h"
  COMMENT "build stamp")

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
# OpenColorIOConfig.cmake find_dependency()s the ext packages it was built against — the
# vendored dist must outrank system package managers on EVERY platform (without this, mac
# resolved Imath/pystring/yaml-cpp/minizip to homebrew DYLIBS: plugin failed to load on
# machines without homebrew).
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/build/ext/dist")
set(ZLIB_USE_STATIC_LIBS ON)   # ext/dist ships static AND shared zlib; the shared one links as
                               # @rpath/libz.1.dylib which resolves NOWHERE (no rpath) — force .a
find_package(OpenColorIO REQUIRED CONFIG)

# --- shared core: compiled INTO each bundle (not a prebuilt lib) so per-target compile defs
#     like MINC_LOG_BASENAME apply; statics are per-binary by design (see core/MincCore.h) ---
set(MINC_CORE_SOURCES
  src/core/MincJson.cpp
  src/core/MincMenus.cpp
  src/core/MincSignal.cpp
  src/core/MincLog.cpp
  src/core/MincUtf16.cpp
  src/core/MincAuthority.cpp
  src/core/MincMint.cpp
  src/core/MincSyncWalk.cpp
  src/core/MincRifx.cpp
  "${AE_SDK_EXAMPLES}/Util/AEGP_SuiteHandler.cpp"
  "${AE_SDK_EXAMPLES}/Util/MissingSuiteError.cpp"
)
set(MINC_COMMON_INCLUDES
  "${AE_SDK_HEADERS}"
  "${AE_SDK_HEADERS}/SP"
  "${AE_SDK_EXAMPLES}/Util"
  src/core
  "${MINC_GEN_DIR}"
)

# ============================== bundle 1: the effect ==============================
add_library(minColorCST MODULE
  src/MinColorCST.cpp
  src/Arb.cpp
  src/Registry.cpp
  src/EffectAuthority.cpp
  src/Passport.cpp
  src/OcioEngine.cpp
  src/SmartRender.cpp
  src/Ui.cpp
  src/EffectEdit.cpp
  ${MINC_CORE_SOURCES}
  "${AE_SDK_EXAMPLES}/Util/AEFX_SuiteHelper.c"
  "${AE_SDK_EXAMPLES}/Util/Smart_Utils.cpp"
  "${AE_SDK_EXAMPLES}/Util/AEGP_Utils.cpp"
)
add_dependencies(minColorCST buildstamp)
target_include_directories(minColorCST PRIVATE ${MINC_COMMON_INCLUDES} src)
target_compile_definitions(minColorCST PRIVATE MINC_LOG_BASENAME="minColorCST_authority")
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
  # --- shared mac bundle mechanics: Rez the PiPL, copy the rsrc in, THEN codesign (copying
  #     after the linker sealed the bundle shipped invalid signatures — see git history) ---
  find_program(REZ_TOOL Rez)
  set(MINC_CODESIGN_IDENTITY "-" CACHE STRING "codesign identity for the plugin bundles (- = ad-hoc)")
  if(MINC_CODESIGN_IDENTITY STREQUAL "-")
    set(MINC_CODESIGN_FLAGS --force --deep --sign -)
  else()
    set(MINC_CODESIGN_FLAGS --force --deep --options runtime --timestamp --sign "${MINC_CODESIGN_IDENTITY}")
  endif()
  function(minc_mac_bundle tgt pipl_r plist entry_symbols)
    set_target_properties(${tgt} PROPERTIES
      BUNDLE TRUE
      BUNDLE_EXTENSION "plugin"
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/${plist}"
    )
    # export ONLY the entry points (the static OCIO otherwise leaks thousands of symbols).
    # entry_symbols is a ;-list: every PiPL CodeMac* string needs its exported symbol here
    # or -dead_strip removes it and AE can't bind the effect.
    foreach(_sym IN LISTS entry_symbols)
      target_link_options(${tgt} PRIVATE "-Wl,-exported_symbol,_${_sym}")
    endforeach()
    target_link_options(${tgt} PRIVATE "-Wl,-dead_strip")
    set(_rsrc "${CMAKE_CURRENT_BINARY_DIR}/${tgt}.rsrc")
    add_custom_command(OUTPUT "${_rsrc}"
      COMMAND ${REZ_TOOL} "${CMAKE_CURRENT_SOURCE_DIR}/${pipl_r}"
              -o "${_rsrc}" -useDF
              -i "${AE_SDK_HEADERS}" -i "${AE_SDK_EXAMPLES}/Resources" -i "${AE_SDK_EXAMPLES}/Util"
              -i "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
      DEPENDS "${pipl_r}" "${CMAKE_CURRENT_SOURCE_DIR}/src/core/MincIds.h"
      COMMENT "Rez PiPL (${tgt})")
    add_custom_target(${tgt}_pipl DEPENDS "${_rsrc}")
    add_dependencies(${tgt} ${tgt}_pipl)
    # PiPL-only edits must refresh the bundle: POST_BUILD (rsrc copy + codesign) only runs on
    # relink, so make the rsrc a link dependency (found the hard way in M3 Probe P — a 4-PiPL
    # build shipped a bundle still carrying the old 5-PiPL rsrc).
    set_property(TARGET ${tgt} APPEND PROPERTY LINK_DEPENDS "${_rsrc}")
    add_custom_command(TARGET ${tgt} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/Resources"
      COMMAND ${CMAKE_COMMAND} -E copy "${_rsrc}" "$<TARGET_BUNDLE_DIR:${tgt}>/Contents/Resources/${tgt}.rsrc"
      COMMAND codesign ${MINC_CODESIGN_FLAGS} "$<TARGET_BUNDLE_DIR:${tgt}>"
      COMMAND codesign --verify --deep --strict "$<TARGET_BUNDLE_DIR:${tgt}>"
      COMMENT "PiPL into bundle, then codesign (${tgt}, ${MINC_CODESIGN_IDENTITY})" VERBATIM)
  endfunction()

  # badge menu (M2 step 5): native NSMenu popup + AppKit for the effect target
  enable_language(OBJCXX)
  target_sources(minColorCST PRIVATE src/UiMenuMac.mm)
  set_source_files_properties(src/UiMenuMac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_link_libraries(minColorCST PRIVATE "-framework AppKit")

  # no EffectMain: legacy "MINC CST" registration retired on mac (M3 step 8)
  minc_mac_bundle(minColorCST src/MinColorCST_PiPL.r src/Info.plist.in
                  "EffectMainXform;EffectMainView;EffectMainRender;EffectMainLook")

  # ============================== bundle 2: the ceremonies AEGP ==============================
  # AEgx package type, app Plug-ins folder, loads at AE launch. No OCIO — the admin-installed
  # bundle stays tiny; all colour math stays in the effect.
  set(MINC_CEREMONY_SOURCES
    src/ceremony/MincSettings.cpp
    src/ceremony/MincEffectOps.cpp
    src/ceremony/MincPresets.cpp
    src/ceremony/MincDoctor.cpp
    src/ceremony/MincSuggest.cpp
    src/ceremony/MincInterpret.cpp
    src/ceremony/MincCeremonyProject.cpp
    src/ceremony/MincStrip.cpp
    src/ceremony/MincArchive.cpp
    src/ceremony/MincTranslate.cpp
    src/ceremony/MincMenusWrite.cpp
    src/ceremony/MincArgs.cpp
    src/ceremony/MincUtility.cpp
    src/ceremony/MincPicker.mm
  )
  add_library(minColorAEGP MODULE
    src/aegp/AegpMain.cpp
    ${MINC_CEREMONY_SOURCES}
    ${MINC_CORE_SOURCES}
  )
  add_dependencies(minColorAEGP buildstamp)
  target_include_directories(minColorAEGP PRIVATE ${MINC_COMMON_INCLUDES})
  target_compile_definitions(minColorAEGP PRIVATE MINC_LOG_BASENAME="minColorAEGP")
  set_source_files_properties(src/ceremony/MincPicker.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_link_libraries(minColorAEGP PRIVATE "-framework AppKit")
  set_target_properties(minColorAEGP PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    OUTPUT_NAME "minColorAEGP"
  )
  minc_mac_bundle(minColorAEGP src/aegp/MinColorAEGP_PiPL.r src/aegp/InfoAEGP.plist.in MincAegpEntry)
elseif(WIN32)
  # a .aex is a plain DLL; EffectMain is the only __declspec(dllexport) (static OCIO exports nothing)
  # (the AEGP companion is mac-only in M0; Windows parity is milestone M4)
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
            -DINC2=${CMAKE_CURRENT_SOURCE_DIR}/src/core
            -DIN_R=${CMAKE_CURRENT_SOURCE_DIR}/src/MinColorCST_PiPL.r
            -DOUT_RC=${PIPL_RC}
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildPiPL.cmake"
    DEPENDS src/MinColorCST_PiPL.r src/core/MincIds.h cmake/BuildPiPL.cmake
    COMMENT "PiPL (cl /EP -> PiPLtool -> rc)"
    VERBATIM)
  target_sources(minColorCST PRIVATE "${PIPL_RC}")

  # --- version.txt beside the .aex: the panel's Windows contract for pluginBundleVersion() ---
  message(STATUS "plugin version (version.txt): ${MINC_VERSION}")
  add_custom_command(TARGET minColorCST POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "${MINC_VERSION}" > "$<TARGET_FILE_DIR:minColorCST>/version.txt"
    COMMENT "version.txt = ${MINC_VERSION}" VERBATIM)
endif()
