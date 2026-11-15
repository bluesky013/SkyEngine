## 1. Persistent asset implementation

- [x] 1.1 Split persistent codecs and pure material definitions from runtime resource conversions in existing paths.
- [x] 1.2 Extract persistent component data and separate asset/runtime reflection entrypoints without changing identities.
- [x] 1.3 Add Aurora.Assets.Static with unique source ownership and replace Cook's Adaptor dependency.

## 2. Metadata ownership and offline cook

- [x] 2.1 Wire Aurora.Metadata and Aurora.Render unique registration owners and explicit module dependencies.
- [x] 2.2 Migrate test metadata initialization and cover definition reuse across retry/manager recreation.
- [x] 2.3 Verify persistent-only World cooking and dependency extraction without a renderer session, including actual AssetTool execution.

## 3. Editor domain policies

- [x] 3.1 Add neutral edit preparation hooks to existing EditorExtension/Host with checked failure/conflict dispatch.
- [x] 3.2 Move Aurora actor creation and skeletal/animation policies to Aurora.Editor; retain generic host transactions.
- [x] 3.3 Cover normal editing, failed preparation/attachment, undo/redo, conflict and document/extension lifetime regressions.

## 4. Integration and acceptance

- [x] 4.1 Reconfigure existing build, check source ownership, and build real affected libraries/modules/tools/editor plus tests.
- [x] 4.2 Run focused and integration regression suites; review includes, explicit errors, metadata calls and dependency graph.
- [x] 4.3 Run native editor smoke checks and retain a few key screenshots; record platform limits and exact test results.
- [x] 4.4 Validate OpenSpec and diff conventions, update implementation status and prepare the combined change for review without commit/archive.
