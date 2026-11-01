# Tasks

> Status (2026-11-01): **ready-to-archive**；owner：`enforce-cmake-source-ownership`。已交付范围和既有验证齐备，主 spec 同步与 archive 等待用户确认；不扩大已记录的平台验收边界。

- [x] Audit repository source lists and configured source owners; distinguish production duplication from test fixtures and compile variants.
- [x] Replace duplicated production sources in Jolt, Animation editor, Enet and Sandbox tests with implementation target links, without source moves or a new core.
- [x] Give shared physics codec and Android input implementation single owners, preserving module entries and native export dependencies.
- [x] Automatically enforce source ownership through common CMake helpers, including independent game projects and deferred additions.
- [x] Declare the existing Aurora static-factory compile variant with exact source/owner/definition validation.
- [x] Add executable guard regressions, CTest/CI integration and contributor rules.
- [x] Complete final configure, regression checks and diff review; record exact evidence and platform limits.

Commit and OpenSpec archive require separate user confirmation under AGENTS.md.
