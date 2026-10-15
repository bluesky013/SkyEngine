# Native template package smoke test

Date: 2026-10-15. This follow-up uses one real generated `PlatformSmoke` game for Android, iOS and macOS. It validates the public `python/package_project.py package` path. It does not substitute a Framework-only executable for the game: `GameApplication` loads `Aurora.Render` and the generated game module.

For the consolidated causes/fixes/coverage across all phases, see the
[cross-platform issue ledger](validation.md#修复与补齐问题总表). Earlier preflight JSON/logs and the superseded
141 MB / ten-ELF APK describe intermediate checkpoints; the final artifact results below take precedence.

## Reproduction

Create the fixture without building or downloading anything:

```sh
python3 python/tests/create_package_smoke.py --dir /tmp/skyengine-package-smoke
```

The script prints the three public package commands. It disables optional feature plugins, requests only the game module in `modules_game.json`, adds a package marker config, selects static iOS game modules, and limits native compile/link jobs to two. Android uses one shared Ninja job pool because AGP invokes Ninja directly; `CMAKE_BUILD_PARALLEL_LEVEL` alone does not cap that invocation.

Bootstrap third-party outputs with `python/third_party.py` before packaging. For each command, set `SKY_3RD_PATH` to the corresponding `build_3rd/Android`, `build_3rd/IOS` or `build_3rd/MacOS-x86` directory. Apple builds require Xcode. Android requires the SDK, NDK and a Gradle-compatible JDK; this host uses CLion's JBR 21 because Android Studio's JBR 25 is incompatible with Gradle 8.10.2.

```sh
export JAVA_HOME='/Applications/CLion.app/Contents/jbr/Contents/Home'
export ANDROID_HOME="$HOME/Library/Android/sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export GRADLE_OPTS='-Dorg.gradle.workers.max=1 -Dorg.gradle.daemon=false -Dorg.gradle.console=plain'
SKY_3RD_PATH="$PWD/build_3rd/Android" python3 python/package_project.py package \
  -p /tmp/skyengine-package-smoke/PlatformSmoke --preset PlatformSmoke-android
SKY_3RD_PATH="$PWD/build_3rd/IOS" python3 python/package_project.py package \
  -p /tmp/skyengine-package-smoke/PlatformSmoke --preset PlatformSmoke-ios
SKY_3RD_PATH="$PWD/build_3rd/MacOS-x86" python3 python/package_project.py package \
  -p /tmp/skyengine-package-smoke/PlatformSmoke --preset PlatformSmoke-macos
```

All three presets are unsigned Apple exports / Android debug exports. No Apple signing identity or provisioning profile is installed on this host. An unsigned IPA cannot be installed directly on an ordinary physical iOS device. This test does not claim signed archive/export, App Store acceptance or macOS notarization.

## Build and package boundary fixes

- Preserve explicit plugin cache overrides on the first CMake configure. Previously the initially empty manifest hash caused every `OFF` preset value to be force-reset to the plugin defaults.
- Forward the engine root and common native preset options to Gradle with single-quoted Groovy arguments, preserving spaces, quotes, backslashes, dollar signs and UTF-8 paths.
- Use the same tagged static/dynamic registration contract for the generated game module as engine runtime modules. Default iOS export presets to static module packaging.
- Enable Objective-C++ in generated Apple projects and use relative install names and runtime search paths.
- Use a real AppKit bundle manifest on macOS and provide `--frames N` for bounded startup/shutdown validation. Apple API defaults select Metal while preserving explicit backend overrides.
- Embed the iOS dynamic Metal backend as a framework; the Core loader resolves it from the executable's `Frameworks` directory. macOS resolves bundled modules beside the executable, independently of the current working directory.
- Exclude offline Content/Navigation builders from mobile runtime package graphs.
- Collect configs and products inside the actual runtime bundle root and create `Payload/<name>.app` IPA / app ZIP archives with executable permissions and symbolic links preserved. Rebuild Apple app outputs fresh while reusing compiled objects, and restage configs/products so removed files do not remain in later packages.
- Stop packaging when an available AssetTool fails. A missing tool still permits explicitly pre-cooked inputs under the existing workflow.
- Repair AssetTool's bootstrap: keep owning filesystem pointers while handing them to managers, load merged builder configs after module registration, and count actual batch completion results. A rejected product now returns a nonzero exit code instead of reporting success.
- Import Slang using the real compiler SONAME and stage matching-config runtime companions on both platforms. Android uses an AGP generated-jniLibs task driven by CMake metadata; the redundant byte-identical compiler compatibility alias is omitted.

## Executed checks

- `python3 -m unittest discover -s python/tests -p 'test_*.py'`: 27 passed at the combined checkpoint, including actual production DynamicModule loading from an unrelated working directory, real CMake imported-runtime models and Android runtime manifest generation. The iOS path-policy probe uses a host-runnable Mach-O; it is not an iOS runtime execution result.
- Fixture creation through `create_package_smoke.py` succeeded separately under `/tmp/skyengine-package-fixture-check`.
- Actual Android Gradle help, runtime dependency resolution, official GameActivity 1.2.2 AAR/Prefab import and CMake configure succeeded. SDK 34, NDK 28.1.13356709, Gradle 8.10.2, AGP 8.8.2, arm64-v8a, minimum API 29. The generated cache confirms optional plugins are off and the shared Ninja pool has depth two.
- Actual iOS Xcode configure succeeded using the iOS 18.5 device SDK, arm64, static modules and signing disabled. The generated graph includes the game module's tagged factory and an Embed Frameworks phase.
- Actual macOS public package command generated `PlatformSmoke.app` and `PlatformSmoke-macos.zip` using the macOS 15.5 SDK and x86_64.

Logs and temporary build outputs are under `/tmp/skyengine-package-smoke`; final deliverables are collected under its `artifacts` directory.

## Final artifact acceptance

- AssetTool positive/negative integration cases pass using the rebuilt production executable. The valid PNG generates a real product with project settings overriding engine settings and engine config fallback intact. Invalid PNG data yields `0 product(s), 1 failed` and exit 1. Runner: `python3 python/tests/run_asset_tool_package_tests.py --work-dir /tmp/skyengine-package-smoke/logs/asset-tool-integration`.
- All three final public package runs successfully cook the template's 11 products with zero failures. These outputs include meshes, images and materials; bundle files are shipped under their actual runtime paths.
- iOS: `artifacts/PlatformSmoke-unsigned.ipa`, 6,892,414 bytes. ZIP CRC, unique entries, executable permissions, `Payload/PlatformSmoke.app` layout, bundle ID, single-scene delegate, configs and 11 product binaries pass. The app and Metal framework are arm64 iOS Mach-O, SDK/minimum OS 18.5; framework install name and app RPATH resolve inside the bundle, all five static factories and the backend factory are present, and no standalone third-party dylib is shipped. SHA-256: `44d0f2a6e8a936444e6baaf55eaaa3539ae109c20e84d886e74545102e0ddb38`.
- macOS: `artifacts/PlatformSmoke.app` and `artifacts/PlatformSmoke-macos.zip`, ZIP 18,321,265 bytes. All 15 Mach-O paths have complete declared dependencies, no absolute external dependency and no absolute non-system RPATH. ZIP CRC, byte equality, executable permissions and three symlinks pass; 11 cooked products and config overrides are present, with no offline builder dylibs. Both relocated runs (default and explicit Metal) from `/private/tmp` initialize the Intel Iris Plus 655 and generated game module, load modules from inside the app, run three frames and exit 0 without reported viewport/load failure. SDK/minimum macOS 15.5, x86_64 only. ZIP SHA-256: `73db327353c3ad00fceb159a748660dbf2e70450a3be885c242442accbbfc96b`.
- Android: `artifacts/PlatformSmoke-debug.apk`, 102,192,733 bytes. Compiled manifest (application ID, Activity, native entry, minimum API 29 / target 33), debug signing and ZIP alignment pass. All 22 config/product asset files match staging byte-for-byte. All nine AArch64 ELF dependency closures and launcher/JNI symbols pass. The real Slang compiler SONAME and GLSL runtime companion are included; the byte-identical 39,361,056-byte compatibility alias is absent. SHA-256: `eab2017a2989d041adab5208c73143d2278816c2d8050a6e4515dcc63ab45bce`.

Machine reports: `logs/android-artifact-validation.json`, `logs/ios-artifact-audit.json`, `logs/ios-ipa-final.json`, `logs/macos-artifact-validation.json` and `logs/macos-runtime-summary.json`. The macOS ZIP was inspected after fresh configure/build/collection, rather than repaired manually.

Final checks: 27 Python regressions pass after the Android alias removal; the two real AssetTool process cases pass; the final relocated macOS default/Metal runs pass. Clang-format dry-run checks on the packaging follow-up C++/Objective-C++ changes, `git diff --check` and strict OpenSpec validation pass. The change remains active and uncommitted; no OpenSpec archive was performed.

## Environment limits and preflight observations

- iOS 27 SDK/runtime is absent; the earlier scene migration and this package build do not establish iOS 27 execution.
- Third-party metadata check reports Slang stale on macOS and Android. The exact hash difference is only the newly pinned `SLANG_ENABLE_SPLIT_DEBUG_INFO=OFF` option: removing that option reproduces the existing metadata hash. Both installed packages are Slang 2026.9.2. An optional refresh triggered a broad compiler rebuild and was stopped before replacing installed runtimes; this test uses the existing installed packages and does not claim a clean metadata check for these two platforms. The iOS metadata check passes, with the known `ios-cmake` tool-only output-directory warning.
- The initial cook exposed missing builder settings and false-success exits; these were fixed and both failure cases and final 11-product cooks were rerun successfully. Visual rendering, arbitrary project content conversion, release package size optimization and the pre-existing concurrent asset-publication race remain outside this smoke acceptance.
- Android has no connected device and no installed emulator, so APK install/Activity lifecycle execution is not tested. iOS is unsigned and no ordinary physical-device installation is claimed. The macOS runs establish bounded startup/shutdown, not visual rendering quality.
