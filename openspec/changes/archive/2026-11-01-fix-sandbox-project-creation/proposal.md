> Status (2026-11-01): **ready-to-archive**；owner：`fix-sandbox-project-creation`。已交付范围和既有验证齐备，主 spec 同步与 archive 等待用户确认；不扩大已记录的平台验收边界。

## Why

Confirming a new project currently runs the packaging CLI through `std::system()` on the UI thread. The project manager cannot repaint or process input until Python exits, and provides neither progress nor actionable failure details.

## What Changes

- Launch the existing Python creator with the framework process API on an owned background task.
- Poll completion from the module tick; update the hub and recent-project registry only on the main thread.
- Show actual creation stages and elapsed time. Disable conflicting hub actions until completion; keep Quit available.
- Drain both output pipes, retain bounded diagnostics, time out stalled creation, and cancel/join before module shutdown.
- Validate the generated descriptor before registering a successful creation.

## Impact

Affected code: Sandbox module, project manager view, packaging CLI and Sandbox regression tests. The existing Add Project descriptor validation remains unchanged. Creation still returns to the hub; Open is an explicit subsequent action.
