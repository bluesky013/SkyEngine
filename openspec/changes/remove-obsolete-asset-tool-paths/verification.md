# Verification

Verified in the existing macOS Xcode Debug workspace alongside `fix-asset-reference-boundaries`; the complete build/test record is in that change's `verification.md`.

- Repository references confirmed ToolApplicationBase was used only by the disabled `tools/asset_builder`; both obsolete entrypoints were removed. XRApplication remains supported and only its unused legacy path constant was removed.
- CMake was reconfigured after removing GLOB-owned Framework source. Framework and the supported AssetTool built successfully, together with affected real modules and editor targets.
- Source ownership checks: 25 passed, 1 skipped, 0 failed (`cmake-build-debug/asset-boundaries-source-ownership.log`).
- Python Apple bootstrap regressions: 24 passed (`cmake-build-debug/asset-boundaries-python.log`).
- Targeted Ninja lookup checks passed for an explicit executable path containing spaces, an invalid explicit override returning no executable, and PATH discovery with no override.
- Strict OpenSpec validation and `git diff --check` pass.

The Windows CPython build was not executed on this Mac. Windows installations must expose Ninja via `NINJA` or `PATH`; the former developer-specific Enterprise installation path is intentionally no longer a fallback.

No target was added, directory relocated, exception policy changed, commit created or change archived.
