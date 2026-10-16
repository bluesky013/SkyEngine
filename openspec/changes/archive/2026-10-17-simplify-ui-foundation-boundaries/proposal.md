## Why

The user confirms that UI is an engine foundation and requests its implementation back in the original UI module. The preceding foundation refactor incorrectly placed it under `plugins/ui`. A follow-up review also found broad exception wrappers and callback reentry paths that could invalidate Preferences copy sources or destroy the shell while opening its dialog. The native host dropped floating-window keyboard and text input before reaching the existing surface dispatch APIs.

## What Changes

- Move the UI runtime, rendering, optional FreeType and document-editor targets back to `engine/ui/{core,render,freetype,editor}`; retain target names, domain includes and responsibility boundaries.
- Remove unnecessary whole-operation catches. Use RAII for `Any` construction/file handles/temporary files and existing error-return APIs for IO; retain structured exception handling only at injected guard, reflection/provider and storage-adapter calls.
- Own Preferences descriptors and working-copy sources across callback reentry; preserve replacement session/history state and stop stale operations.
- Guard Preferences opening against shell/dialog destruction or replacement during history notifications.
- Forward floating-window key/text events through the existing surface dispatch APIs, including when a restored floating window receives the first event.

## Capabilities

### Modified Capabilities

- `engine-plugin-layering`: UI foundation ownership in engine with independent targets.
- `editor-preferences`: callback-safe capture, synchronization, application and opening.
- `editor-preference-persistence`: narrow exception boundaries and RAII resource ownership.
- `editor-ui-shell`: native floating key/text routing through existing surface APIs.

## Impact

UI placement/CMake, Preferences/Any lifetime handling, EditorShell opening, bounded SandboxModule input routing, documentation and regression tests. Render implementation bytes are moved without behavior changes. RHI/RDG/pipeline/backend implementations, renderer resource/submission contracts, native-window ownership and Core Event remain outside this change. Windows/native/display/GPU execution remains an explicit pending gate. No commit or archive is authorized.
