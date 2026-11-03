# Verification — 2026-11-03

## Final structure and source integrity

```text
engine/animation/
  core/{include,src,test}       Animation / AnimationTest
  assets/{include,src,module}  Animation.GraphAssets.Static / Animation.GraphAssets
  adaptor/                    Animation.World.Static / Animation.World
  builder/                    Animation.Builder.Static / Animation.Builder
  editor/                     Animation.Editor.Static / Animation.Editor
```

The original algorithm include/src/test trees and Graph definition/evaluation now share core. Animation.Graph.Static and Animation.GraphTest are removed; Graph tests run in AnimationTest. Asset serialization/registration stays outside the algorithm library. SKY_BUILD_ANIMATION controls assets/World/tool integration; pure Graph remains available with Animation when integration is disabled.

Compared all relocated non-CMake files against their original HEAD paths: **96 are byte-identical**. The only changed relocated C++ file is AnimationTest.cpp: its duplicate gtest main was removed in favor of the existing shared core/test/main.cpp, also reused by ModuleTest. No algorithm, asset schema, module name, metadata owner or public animation/... include changed. No old root include/src/test directory or plugins/animation directory remains.

## Full build and native tests

Existing cmake-build-debug cache retained: Xcode generator, 3RD_PATH=/Users/lizhen/Program/SkyEngine/build_3rd/MacOS-x86. Configure and the automatic production-source ownership audit passed. Xcode cache access used the previously required tool-approved escalation.

```sh
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --config Debug --parallel 4 --target \
  Animation Animation.GraphAssets Animation.World Animation.Builder Animation.Editor \
  AnimationTest Animation.WorldTest Animation.BuilderTest Animation.EditorTest \
  Animation.ModuleTest FrameworkTest AssetTool
ctest --test-dir cmake-build-debug -C Debug --output-on-failure \
  -R '^(AnimationTest|Animation\.(WorldTest|BuilderTest|EditorTest|ModuleTest)|FrameworkTest)$'
```

Build passed, including the real implementation static libraries and module entries. All six native test programs passed; skips remain skips, not passed cases.

| Program | Passed | Skipped |
| --- | ---: | ---: |
| AnimationTest (including Graph) | 78 | 0 |
| Animation.WorldTest | 10 | 0 |
| Animation.BuilderTest | 4 | 0 |
| Animation.EditorTest | 8 | 0 |
| Animation.ModuleTest | 2 | 0 |
| FrameworkTest | 281 | 22 |

Logs: /private/tmp/animation-core-configure.log, /private/tmp/animation-core-build.log, /private/tmp/animation-core-native-tests.log and cmake-build-debug/Testing/Temporary/LastTest.log. ModuleTest loads the real metadata/runtime/editor modules and checks teardown/restart.

## Integration disabled and static configuration

- Reconfigured the isolated full-repository Ninja build at /private/tmp/sky-animation-disabled-config with SKY_BUILD_ANIMATION=OFF and SKY_ANIMATION_ACL=OFF. Built AnimationTest and ran it: **75 passed, 3 ACL tests skipped**, including all 14 Graph tests. Animation links only Core in this configuration; no GraphAssets/World/Builder/Editor integration targets are generated.
- Reconfigured the isolated full-repository static/editor/tests build at /private/tmp/sky-animation-static-config. The four module entries remain static targets, pure Graph belongs to Animation, and no Animation.Graph.Static or Animation.GraphTest target is generated. This static configuration was not separately compiled/executed.
- Both isolated builds retain their own output paths, separate from the Xcode products.

Logs: /private/tmp/animation-core-disabled-{configure,build,tests}.log and /private/tmp/animation-core-static-configure.log.

## Regression checks

- python3 python/tests/test_animation_configuration.py: **2 tests passed**, covering six target configurations (shipping, tools, tests-only, editor/tests, static editor, disabled integration), algorithm/asset dependency boundaries, sole Graph compilation owner, removed targets, module factory definitions, default ON and explicit OFF surviving cache/plugin-manifest changes.
- python3 python/tests/test_cmake_source_ownership.py: **25 passed, 1 skipped** (26 cases).
- python3 -m unittest python.tests.test_animation_package python.tests.test_create_package_smoke python.tests.test_native_package_artifacts: **38 tests passed** against the rebuilt AssetTool. Product audit error messages are expected negative cases.
- openspec validate refactor-animation-engine-ownership --strict: passed.
- openspec validate add-skeletal-animation-workflow --strict: passed.
- openspec validate --specs --strict: all **32** domain specs passed.
- git diff --check: passed.

Regression logs: /private/tmp/animation-core-configuration-tests.log, /private/tmp/animation-core-ownership-tests.log, /private/tmp/animation-core-package-tests.log, /private/tmp/animation-core-specs.log.

## Acceptance boundary

This is macOS validation (Xcode Debug plus the isolated Ninja algorithm build). VS2022/Win32 and mobile execution have not been run for this migration. The skeletal animation workflow's remaining functionality/GPU/platform acceptance tasks remain open. At the 2026-11-03 review handoff, no commit or OpenSpec archive had been performed.

## Archive — 2026-11-04

The user explicitly approved archiving this migration, rebasing onto origin/dev_refactor_rhi and committing the implementation and OpenSpec artifacts together. All eight tasks and four artifact groups are complete. The five requirement blocks across the assets/build/modules deltas already match the main specs exactly, so the archive preserves them without replaying deltas. The change is archived at openspec/changes/archive/2026-11-04-refactor-animation-engine-ownership; the original skeletal workflow remains active for its outstanding functional and platform acceptance.
