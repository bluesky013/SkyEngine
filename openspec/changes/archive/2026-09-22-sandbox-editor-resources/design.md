## Context

The sandbox editor hosts UI text through the `ui` text system. Its font was loaded from the repo-root project
`assets/fonts/OpenSans-Regular.ttf` by a **CWD-relative** path. Run from `output/bin/<config>` (where `assets/`
does not exist) the load failed and the text system fell back to the builtin block-glyph provider. The repo-root
`assets/` is the project-package tree, which is being retired, so editor builtin resources need their own root.

`Platform::Get()->GetBundlePath()` returns the executable's directory on all supported platforms, which is the
stable anchor for editor-owned resources.

## Goals / Non-Goals

**Goals:**
- Give the sandbox editor its own builtin resource root, separate from project-package assets.
- Resolve resource paths relative to the executable, independent of the working directory.
- Deploy the resource root next to the executable without duplicating large trees when possible.

**Non-Goals:**
- Embedding resources into the binary (evaluated and deferred; the file-based approach is kept).
- A full resource/asset pipeline (icons are still loaded ad hoc by the renderer).
- Changing the project-package `assets/` layout.

## Decisions

- **D1 — Editor-owned root, bundle-relative resolution.** Keep resources under `engine/sandbox/resources/` and
  resolve them from the executable directory (`SandboxResources::Root()`), not the CWD. Rationale: the previous
  CWD-relative load is exactly what broke when launching from the output directory. Alternative considered:
  re-point at the repo-root `assets/` — rejected, that tree is project-package oriented and being retired.
- **D2 — Symlink-first deployment with copy fallback.** A post-build CMake script removes any previous deployment
  and then tries `create_symlink`; on failure it falls back to `copy_directory`. Rationale: a symlink avoids
  duplicating a large resource tree and tracks source edits without a rebuild, while the copy fallback keeps
  Windows-without-developer-mode working. The removal step must delete a symlink itself (never the directory it
  points at) before removing real directories.
- **D3 — Keep the helper in `EditorCore`.** `SandboxResources` is a sandbox-wide, render-agnostic concern, so it
  lives in `EditorCore`; `EditorRender` (a higher layer) links `EditorCore` to use it.

## Risks / Trade-offs

- [Windows symlink privilege] → copy fallback (verified: the machine without developer mode copies).
- [Bundling a font in the repo] → the font is small and already present in the repo tree.
- [Embedding deferred] → resources still need a deployment step; documented as a possible future change.

## Migration Plan

1. Add the resource root and the `SandboxResources` helper. *(done)*
2. Add the deploy script + post-build hook; switch the font load to the helper. *(done)*
3. Build and run from `output/bin/<config>`; confirm the font loads without fallback. *(done)*
4. Rollback: remove the resource root/script and revert the font path + post-build hook.
