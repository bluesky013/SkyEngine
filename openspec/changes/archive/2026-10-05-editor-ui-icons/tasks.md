## 1. Derived data cache (engine/framework)

- [x] 1.1 Add `IDerivedDataBuilder` (stable id, version, `Build`) and a `DerivedDataCache` (register / find / fetch) in `engine/framework`.
- [x] 1.2 Address entries by `hash(source + builder id + version + settings + platform)`; store as `<root>/<key>.bin`; on a miss run the builder and write the entry, on a hit read it back.
- [x] 1.3 Report failure without writing an entry when the builder is unregistered or `Build` fails.

## 2. SVG icon builder (sandbox module)

- [x] 2.1 Add NanoSVG as a managed header-only third-party (`thirdparty.json` + `Findnanosvg.cmake`), referenced only by the sandbox module.
- [x] 2.2 Implement `UiIconBuilder` (id `ui-icon-svg`) rasterizing SVG source bytes to RGBA8 at the requested `WxH` size, falling back to the intrinsic size.
- [x] 2.3 Register the builder with the DDC from `SandboxModule::Init` via `InstallUiIconBuilder()`.

## 3. Editor icon sample (sandbox)

- [x] 3.1 Add the save icon source `engine/sandbox/resources/icons/save.svg`.
- [x] 3.2 Bake it through the DDC from the reflection demo panel, register the texture and draw it as an icon button; fall back to a directly generated glyph when the SVG is unavailable.
- [x] 3.3 Point the DDC root at `<sandbox resources>/cache` in `SandboxModule::Init` so derived icons are stored under the sandbox resources directory.
- [x] 3.4 Make the icon button interactive: route `MOVE` events to the extra-header hook and draw hover/press feedback so hovering and clicking respond visibly.

## 4. Validation

- [x] 4.1 Add framework tests for DDC miss/hit, settings/platform keying, and builder-version invalidation.
- [x] 4.2 Build `SandboxEditor` and `FrameworkTest`; run the DDC tests green.
- [x] 4.3 Run the editor and confirm the derived icon entry is written under the sandbox resources dir.
