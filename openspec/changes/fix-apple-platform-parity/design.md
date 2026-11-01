## Context

The accepted OHOS workflow provides explicit toolchain identity, production-target compilation, owner-thread lifecycle, staged resources and device evidence. The Apple review at 60309307 found reproducible CMake failures and configuration fields that did not reach the real artifact. Existing Cocoa/AppKit and UIKit/UIScene ownership stays in Framework; Core, Renderer and the established targets remain unchanged.

## Goals / Non-Goals

Goals: restore executable Apple regression entry points; make architecture/SDK/configuration/version choices observable in actual artifacts; stop inactive Cocoa busy polling; cover UIKit dialog transitions; expose a separate signed archive/export/install workflow.

Non-goals: new module/target ownership, directory moves, store upload, automatic credential creation, full CPython cross-host build infrastructure, a Mach semaphore API redesign or claiming P02 GPU/long-duration completion from platform tests.

## Decisions

1. Keep the UIKit fixture on the production Core and Framework targets with default built-in compression enabled. Reuse the shared CMake finder helper for the minimal SFMT/RapidJSON/LZ4 set and load the GoogleTest checkout's top-level project so its own version initialization remains authoritative. Avoid disabling a production dependency just to make a test compile. Use the existing third-party entry with an explicit IOS device/simulator platform and separate output root.
2. Normalize path identity at test comparisons. Do not change correct resolved toolchain/plist paths to match a macOS symlink spelling.
3. Bind MacOS-x86/MacOS-arm to explicit CMake architectures, include target SDK/architecture in build identity and verify real artifacts before metadata publication or cache reuse. Tools validate the files they actually supply. Optional custom builders must reject unsupported cross-host prerequisites explicitly rather than publish wrong-architecture output.
4. Derive one Apple configuration from the preset and use it for configure/build/collection. Generate export metadata copies for version fields and orientation while preserving project-owned source plists. Keep the unsigned collector and add a separate signed archive/export entry; resources must be staged as a build input before Xcode signs. Export options are explicit, credentials remain external and install is opt-in on a selected device.

macOS app bundles place non-code runtime data under Contents/Resources; native executables/modules remain under Contents/MacOS. The bundle filesystem uses Resources while loose command-line tools preserve their executable-directory root. This is required by code signing, which treats data beneath MacOS as nested code.

5. Cocoa WAIT_IF_INACTIVE waits on AppKit events with a bounded service interval, allowing timers/main-queue work to affect frame state. NON_BLOCKING remains immediate. Exit publishes a thread-safe flag and wake event. This retains the desktop main thread instead of copying the OHOS worker model.
6. UIKit native tests exercise the real presented controller, UIKit field/view delegate paths, marked text and UI dismissal. Shared policy tests remain useful but do not replace native UI execution. A simulator run is distinct from a signed iPad run. Main-window teardown must explicitly complete an active dialog exactly once if native coverage exposes that gap.

The UIKit test host starts the suite with a one-shot main-run-loop timer after the single scene has connected and activated. Running the entire synchronous GoogleTest suite inside a main dispatch-queue block prevents nested UI waits from servicing UIKit's main-queue presentation and keyboard completions; the earlier selector-only cases did not exercise that dependency. Native dialog assertions continue to require a completed presentation and a real first responder.

Native UIKit fixtures temporarily disable animation interpolation and flush implicit Core Animation transactions while pumping the run loop, restoring the prior animation setting during teardown. This avoids tying synchronous test waits to the outer UIApplication frame commit; production presentation stays animated, and assertions still require a window-backed presented controller with native keyboard focus.

The iOS dialog handle privately claims the first terminal decision and retains its text and native error during UIKit retirement. It stops accepting changed events and keeps the terminal out of the shared receiver queue until UIKit releases the presenter: an already-running shared dispatch loop may otherwise collect it when a changed callback reenters native confirmation. Dismissal completion schedules token-checked posting and delivery on the next main-queue turn, so the terminal callback can open another dialog. Every delayed completion checks its captured token before posting or dispatching, preventing an old controller from bypassing a newer dialog's gate. Explicit Hide and inactive/window teardown close synchronously and deliver the first decision already claimed by that handle; an empty handle cannot cancel another owned request.

## Risks / Trade-offs

- Bounded Cocoa waits trade a low-rate wake for service of timer/dispatch-only work; tests cover recovery, quit and NON_BLOCKING rather than asserting a brittle CPU percentage.
- Apple artifacts and third-party metadata from different architectures/configurations are not interchangeable; rebuild affected packages instead of rewriting their keys.
- Simulator and device outputs require separate SDK-compatible dependencies; compile-only evidence is labeled as such.
- Personal-team/export profiles and hardware availability can limit actual signed distribution. Preserve explicit failure and logs; never fake a successful archive/export/install.
- Existing command entry points use Python CLI exception translation. New helpers return explicit statuses; do not introduce exception-based engine errors or new catch-all fallbacks.

## Migration Plan

Implement and verify in the reviewed sequence: fixture/path tests; architecture/configuration/metadata; Cocoa waiting; UIKit simulator then device coverage; signed archive/export/install. Keep all change artifacts and implementation together for later review. No archive, commit or push occurs without separate user confirmation.
