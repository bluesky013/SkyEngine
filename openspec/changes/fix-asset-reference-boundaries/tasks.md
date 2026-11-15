## 1. Source identity and mutations

- [x] 1.1 Preserve mount-qualified source identity and owner-based file access; test same-path Engine/Project assets.
- [x] 1.2 Lock and reread manifests for mutations; test stale editor cache after worker commit and rollback failures.
- [x] 1.3 Separate unregister from durable source deletion and test catalog rebuild after editor Delete.

- [x] 1.4 Preserve overlay identity in product path publication and AllowCook resolution; cover both cook orders.

## 2. Material boundaries

- [x] 2.1 Make mesh binding/compilation consume explicit materials and expose effective dependencies.
- [x] 2.2 Resolve fallback policy at builder preparation and update SoftBody dependency propagation and fixtures.
- [x] 2.3 Add independent material resolution and remove navigation Plane/global-path coupling; test resource ownership.

## 3. Editor boundaries

- [x] 3.1 Use exact Engine lookup for creation/preview defaults, preserve configured material and asset labels/hints.
- [x] 3.2 Move DDC persistence out of deployed resources and remove stale asset retirement comments.

## 4. Validation

- [x] 4.1 Build affected libraries, modules, editor and current test binaries; run relevant regressions.
- [x] 4.2 Validate OpenSpec, source ownership and changed C++ conventions; review combined implementation.
- [x] 4.3 Run native editor smoke validation, retain a few key screenshots and document platform/visual limits.
