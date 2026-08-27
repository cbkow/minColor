# minColor plugin — Windows build quick-start

Prereqs: Visual Studio 2022 (C++ workload), CMake 3.24+, git. Use an
"x64 Native Tools Command Prompt for VS".

1. Unzip the Windows AE SDK (private/sdk/AfterEffectsSDK_25.6_61_win — it's a
   zstd tarball like the mac one; 7-Zip-Zstandard is bundled beside it) so that
   `plugin/sdk/<anything>/Examples/Headers/AE_Effect.h` exists.
2. Build static OCIO:  `plugin\external\build.bat`   (produces external\install)
3. Configure + build:  `cmake -S plugin -B plugin\build`  then
                       `cmake --build plugin\build --config Release`
4. Install: copy `plugin\build\Release\minColorCST.aex` to
   `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\`
   and the configs from `config\dist\` (plus luts/filmic/icc) into a `configs`
   folder beside it.
5. Verify: restart AE → Effect ▸ minColor ▸ minColor CST; run the panel's
   interpret pass on a scratch project; log lands in %TEMP%\minColorCST_authority.log.

Known-unknowns to expect (M0-style whack-a-mole): PiPL preprocess flags, OCIO
ext-package link order (add external\build\ext\dist\lib\*.lib to the target if
unresolved externals appear — mirrors the mac fix), MSVC runtime (/MD) matching
between OCIO and the plugin, and the DRAWBOT/AEGP suite headers pulling in
Windows.h ordering issues (NOMINMAX is already defined).
