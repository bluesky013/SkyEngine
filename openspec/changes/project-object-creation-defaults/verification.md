# Verification after review remediation

Date: 2026-11-15. Existing macOS x86_64 Xcode Debug cache and third-party root were preserved. No commit or archive was performed.

## Build and automated checks

Actual production libraries/modules, SandboxModule and SandboxEditor built successfully. Logs: `cmake-build-debug/review-cleanup-build.log`, `review-cleanup-editor-build.log` and `review-cleanup-final-build.log`.

| Test target | Passed |
| --- | ---: |
| FrameworkTest | 317 |
| Aurora.AdaptorTest | 105 |
| Aurora.CookTest | 59 |
| EditorCoreTest | 206 |
| EditorShellTest | 168 |
| EditorWidgetsTest | 62 |
| EditorRenderTest | 48 |
| SandboxProjectTest | 16 |
| UICoreTest | 194 |
| Total | 1175 |

FrameworkTest initially passed 294 and skipped 23 Cocoa tests because the sandbox could not access WindowServer. All 23 were rerun in the logged-in desktop session and passed. All final test processes exited with code 0. Logs: `cmake-build-debug/review-cleanup-<target>.log` and `review-cleanup-CocoaPlatformTest.log`.

Source ownership: 25 passed, one platform-specific skip. Both OpenSpec changes passed strict validation. Scoped formatting, ASCII and `git diff --check` passed.

## Regression evidence

- Engine remove/move calls fail without changing the catalog or source location. Injected manifest failures preserve source/generated identities and roll back a renamed source.
- Refreshing a scene asset source across 1030 identities no longer exhausts persistent watches. At the 1024 live-asset limit existing pending resources remain pollable and new requests report OUT_OF_MEMORY.
- Tooltip hit testing respects a foreground sibling even when it has no tooltip. Popup capture still queries outside normal bounds. Paint validates only the retained owner/root/binding/geometry and preserves pending delay.
- Asset label painting queries one catalog item; cheap tooltip queries do not query the catalog, while deferred text construction does.
- Shared settings opening preserves notification-driven destruction and scope switching. Invalid configuration remains untouched and reports file repair/restart rather than an impossible Apply.
- Cube/Plane creation belongs to Aurora.Editor and snapshots project defaults through the existing registry. Component serialization collects explicit fields only. Mesh cooking fills material slots and publishes deduplicated dependencies, including Unlit fallback; missing fallback sources fail cooking.

## Native editor evidence

Only `cmake-build-debug/project-defaults-validation/Project` was used. The helper app executable was refreshed from the final build and a new process launched.

- Project Settings displays `Engine/materials/basic_lit.mat`, matching the existing project configuration.
- Opened `engine-assets-unified.world`, created a Cube through the registry-driven menu, and verified the configured material plus `Engine/shapes/cube.glb` in Inspector. Rendering remained visible.
- Undid creation and saved. The scene retains two actors and only its explicit mesh/material world roots; fallback dependency ownership now belongs to cooked mesh products.
- Left the editor open with the original Cube selected.
- Key screenshots: `cmake-build-debug/project-defaults-validation/08-review-project-settings.png` and `09-review-created-cube.png`.

## Limits

Old mesh products need recooking before packaging under the new fallback-dependency contract. Existing source UUIDs and component schemas are unchanged. Referenced assets such as basic_lit.mat, box.glb and plane.glb are retained.

Native hover timing/floating-popup visuals were not recaptured: the available Computer Use API has no pointer-move-only operation. Prior user observation confirmed basic hover display; automated host/provider/integration tests cover the modified behavior. Windows/Linux native branches were not built or run on this macOS host.
