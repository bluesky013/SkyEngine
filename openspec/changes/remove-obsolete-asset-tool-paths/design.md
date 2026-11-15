## Context

Repository-wide reference search found ToolApplicationBase referenced only by the disabled tools/asset_builder executable. That executable calls removed APIs. XRApplication remains referenced by launcher templates; only its unused legacy path constant is removed.

## Goals / Non-Goals

Remove misleading dormant entrypoints and machine-specific discovery. Do not rewrite the supported AssetTool bootstrap, remove XRApplication, relocate directories or introduce new targets.

## Decisions

Delete the unreferenced tool and base class rather than repairing an unused parallel host. Existing AssetTool remains the cook worker. Ninja lookup uses NINJA when explicitly configured, otherwise PATH; an invalid explicit override does not silently select another executable. No exception handling changes are introduced.

## Risks / Trade-offs

External users of the removed, inactive ToolApplicationBase must migrate to supported application/module bootstrap. Framework's GLOB requires CMake reconfiguration. Windows users who relied on the single hardcoded Enterprise path must put Ninja on PATH or set NINJA.
