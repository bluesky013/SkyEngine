## Context

The general feature-plugin rule does not override the user's explicit ownership instruction for engine foundations. UI implementation belongs to `engine/ui`; retaining separate CMake targets still prevents runtime dependencies on editor, font rasterizer or rendering implementations. Existing foundation artifacts are corrected to describe that ownership, while prior validation checkpoints retain their historical meaning.

## Decisions

### Restore foundation ownership without changing its public graph

Move 75 files from `plugins/ui` to the original `engine/ui` domain: core, render, freetype and editor. Keep `UI.Interface`, `UI`, `UIRender`, `UI.FreeType`, `UI.Editor` and `ui/...` include prefixes. Root `UI.Interface` retains contracts/data/pure functions; core retains the toolkit; optional/specialized implementations remain separate targets. Remove the plugin entry and update test/tool/doc source paths. Except for four CMake folder labels, relocated files remain byte-identical; relocation does not authorize renderer implementation edits.

### Catch provider failures at the provider call

Ordinary JSON/collection/string/allocation work does not gain a catch wrapper. Session reuses the shared capture protocol instead of duplicating exception translation. `Any::Create` publishes ownership only after successful construction and frees unconstructed heap storage through RAII. File storage uses owned FILE/temporary-file handles and explicit OS error paths; allocation failures propagate normally. There is no new power-loss durability or concurrent-writer guarantee.

Keep eight physical try blocks: five preference operation calls (reflected copy/getter/setter+changed/validation/application), one reflected codec/provider helper, one storage adapter helper and one injected current-guard helper. Those calls have existing structured failure/partial-effect contracts. Catch scope should exclude surrounding encoding loops and incidental descriptor/vector construction. Shared guards return Conflict on failure and callers verify state again after callbacks.

### Retain copy sources and commit synchronized state only when current

Copy owned page descriptors before any reflected constructor can mutate the registry. Capture checks its current guard before and after each provider copy and only replaces output on complete success. Session pages are shared ownership units; Apply holds the source pages across candidate construction, so End/Begin cannot release the object a running constructor still reads.

Synchronize builds an independent descriptor/candidate set while retaining old pages. Check generation, registry identities and history revision after each callback-capable construction, then publish the completed page set. An old operation must not replace a reentrant Begin or release the target of a new history command. Invalid/duplicate raw registry pages must not silently disappear from Apply validation.

### Guard open-dialog continuation and forward existing input identities

OpenPreferences retains weak shell/dialog identity and chrome generation, checking after SetModel, Arrange and Open. History notifications may destroy or replace those owners; the old call then stops without accessing members.

SandboxModule resolves known floating windows through its existing window-to-surface mapping. Main-window acceptance excludes known floats before learning a main ID, and key-down/up plus filtered text use the existing surface dispatch APIs. This is a bounded input-routing fix; it does not alter renderer/window ownership or event contracts.

## Validation

Build and run the six portable suites in normal and ASan profiles; normal additionally covers the optional FreeType target. Retain prior throw/storage/history regressions and add five reflected-copy reentry/lifetime cases plus one shell reopening destruction case. Compile the bounded SandboxModule source with configured flags without linking/running the excluded native stack. Verify moved-file hashes, protected-scope hashes, strict active-change validation and tracked/untracked whitespace. Actual native keyboard/text, Windows file replacement, display/DPI and GPU checks stay pending on suitable hosts.
