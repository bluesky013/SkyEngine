## Why

The non-Qt sandbox editor had no place of its own for builtin resources (icons, editor fonts). It loaded its font
from the repo-root project `assets/` via a CWD-relative path, which failed whenever the editor was launched from
`output/bin/<config>` (no `assets/` there) — the text system silently fell back to a block-glyph provider. The
repo-root `assets/` is the project-package tree (and is being retired), so editor builtin resources need their own
home and a deployment step that does not depend on the current working directory.

## What Changes

- Add an **editor-owned builtin resource root** at `engine/sandbox/resources/` (icons, editor fonts), distinct from
  the project-package `assets/`.
- Add `SandboxResources` in `EditorCore`: `Root()` resolves the deployed resource root from the platform bundle
  path (`<exe>/resources/`), and `Resolve(relative)` builds an absolute path — no CWD dependency.
- Deploy the resource root next to the executable via a post-build step (`deploy_resources.cmake`) that prefers a
  **symbolic link** (no duplication for large trees) and falls back to a **recursive copy** when symlinks are
  unavailable (e.g. Windows without developer mode).
- Load the editor font through `SandboxResources` and have `EditorRender` link `EditorCore` for the helper.

## Capabilities

### New Capabilities
- `editor-resources`: the sandbox editor's builtin resource root, its bundle-path resolution, and its post-build
  deployment to the executable.

### Modified Capabilities
<!-- None. -->

## Impact

- New: `engine/sandbox/resources/` (icons/, fonts/), `engine/sandbox/core/include/editor/core/resource/`,
  `engine/sandbox/core/src/resource/`, `engine/sandbox/app/deploy_resources.cmake`.
- Changed: `engine/sandbox/app/CMakeLists.txt` (post-build deploy), `engine/sandbox/render/CMakeLists.txt`
  (link `EditorCore`), `engine/sandbox/render/src/EditorRenderer.cpp` (font path via `SandboxResources`).
