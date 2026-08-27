# minColor plugin — Windows build

STATUS: built and proven on the Union Windows machine (2026-08-27) — this doc
describes the pipeline as it actually shipped.

Prereqs: Visual Studio 2022 (C++ workload), CMake 3.24+, git.
Use an "x64 Native Tools Command Prompt for VS".

1. SDK: unzip the Windows AE SDK so plugin/sdk/<anything>/Examples/Headers/AE_Effect.h
   exists (the SDK archive does not travel with the repo — copy it from
   private/sdk/ on the mac, or re-download).
2. Static OCIO 2.5.2:  plugin\external\build.bat   -> external\install
   (built /MD — CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL matches AE and the plugin)
3. Plugin:  cmake -S plugin -B plugin\build
            cmake --build plugin\build --config Release      -> minColorCST.aex
   PiPL: src/MinColorCST_PiPL.r -> cl /EP -> PiPLtool -> cl /EP /D MSWindows -> .rc,
   run via cmake/BuildPiPL.cmake in script mode (stdout redirection works under
   both VS and Ninja generators). OCIO ext deps resolve via CMAKE_PREFIX_PATH to
   external/build/ext/dist (OpenColorIOConfig find_dependency()s them) and the
   *.lib glob links the static ext packages.
4. AE-free engine check:  plugin\tools\build-probe.bat  (MSVC build of
   tools/probe-engine.cpp; verify against PyOpenColorIO as on mac).
5. Install: copy plugin\build\Release\minColorCST.aex to
   C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\
   plus config\dist contents as a "configs" folder beside it.
6. Verify: restart AE -> Effect > minColor > minColor CST; panel interpret pass;
   log lands in %TEMP%\minColorCST_authority.log.

Portability rules (both platforms build from the SAME sources): platform code
goes behind #ifdef AE_OS_WIN (defined by the Windows CMake branch alongside
MSWindows); never fork shared logic.
