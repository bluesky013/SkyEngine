## 1. Restore native regression entry points

- [x] 1.1 Reuse the shared third-party finder for the minimal UIKit fixture and retain production LZ4 compression.
- [x] 1.2 Load the pinned GoogleTest checkout through its top-level project and verify real configuration/build.
- [x] 1.3 Normalize macOS path assertions and run the portable platform/package contracts with declared dependencies.
- [x] 1.4 Bootstrap matching simulator dependencies through the public entry and run the native UIKit baseline.

## 2. Bind Apple build and export identity

- [x] 2.1 Bind macOS architecture and IOS SDK/architecture in build options, cache identity and real artifact validation.
- [x] 2.2 Validate actual tool inputs in strict preflight and reject unsupported custom-builder cross-host prerequisites explicitly.
- [x] 2.3 Propagate Apple variant through configure/build/artifact lookup and reject stale output from another configuration.
- [x] 2.4 Export version metadata without rewriting project-owned plists and verify the generated/final metadata.
- [x] 2.5 Run focused Python contracts plus real affected Apple target/configuration checks.

## 3. Fix Cocoa inactive waiting

- [x] 3.1 Implement bounded native inactive waits and a thread-safe quit wake, retaining immediate NON_BLOCKING behavior.
- [x] 3.2 Add native recovery/quit/timer and committed/marked text regressions.
- [x] 3.3 Build production Framework and run native Cocoa/shared Framework tests.

## 4. Verify UIKit dialog ownership

- [x] 4.1 Add native single-line/multiline/password/Unicode/IME and terminal/reopen coverage.
- [x] 4.2 Fix any dialog teardown gap exposed by main-window/scene/native UI regression.
- [ ] 4.3 Rerun the final expanded native suite on a simulator; baseline 21/21 and expanded 31/32 before the reentry fix are retained, final 33/33 is verified on iPad. See verification.md for the popup-related boundary.
- [x] 4.4 Add the signed real-device regression path and execute the available iPad cases; record physical keyboard/IME and P02 GPU limits separately.

## 5. Complete signed Apple distribution

- [x] 5.1 Add a separate archive/export command with external export options and resource staging before signing.
- [x] 5.2 Validate signatures/artifacts and make selected-device installation opt-in; invalidate stale results on failed stages.
- [x] 5.3 Execute available development archive/export/install checks and retain exact signing/environment limits.

## 6. Review and handoff

- [x] 6.1 Run source ownership, formatting/ASCII/exception-pattern checks and strict OpenSpec validation.
- [x] 6.2 Update current owner/index/status documentation and verification evidence without claiming remaining P02/long-duration completion.
- [x] 6.3 Review the final diff; leave commit/archive/push for separate user confirmation.
- [ ] 6.4 Run the shared-path Windows regression on the existing VS2022 configuration: third-party preflight/build smoke, FrameworkTest/source ownership, and generated Launcher resource deployment including an incremental update. Mac-hosted contracts do not close this task.
