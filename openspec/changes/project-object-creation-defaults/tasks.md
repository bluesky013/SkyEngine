## 1. Project settings
- [x] 1.1 Add a separate reflected project page registry and configs/editor.json storage.
- [x] 1.2 Support UUID scalar persistence and approved explicit-error migration.
- [x] 1.3 Add Project Settings using the shared dialog and reflected controls.

## 2. Repository asset creation
- [x] 2.1 Restore the existing component schema and remove new procedural/reference abstractions.
- [x] 2.2 Use repository primitive mesh sources and ordinary default material sources.
- [x] 2.3 Apply the configured material as a creation-time snapshot with explicit validation.

## 3. Validation
- [x] 3.1 Run settings, asset creation, serialization and material regression tests.
- [x] 3.2 Build affected modules/editor and run ownership, diff and spec validation.
- [x] 3.3 Verify native settings, creation, save and legacy display workflows; record rendering/hover timing limits explicitly.

## 4. Unify former built-ins with Engine assets
- [x] 4.1 Persist matching primitive geometry and material sources with legacy manifest identities.
- [x] 4.2 Remove virtual Builtin registration and runtime generation; migrate preview/navigation consumers.
- [x] 4.3 Keep normal asset dependency collection and path/hover-hint display.
- [x] 4.4 Build affected targets and run repository identity, tooltip, loading and renderer regressions.
- [x] 4.5 Verify old/new scene asset paths in the editor and retain key screenshots.

## 5. Review remediation
- [x] 5.1 Enforce writable source ownership and report mutation persistence failures.
- [x] 5.2 Retire unused asset watches and remove the unused revision counter.
- [x] 5.3 Move Cube/Plane registration into Aurora.Editor and unify settings opening.
- [x] 5.4 Make malformed configuration recovery instructions actionable and consolidate catalog queries.
- [x] 5.5 Publish fallback material dependencies during mesh cooking rather than component serialization.
- [x] 5.6 Run affected regressions, build the editor, update native evidence and remove stale documentation.
