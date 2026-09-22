## 1. Decouple the core, drop the legacy editor adaptation

- [x] 1.1 Split `runtime/` into `runtime/core` (render/editor-free) and `runtime/render` (legacy glue); add the
      `PVS.Core` and `PVS.Render` targets
- [x] 1.2 Extract `PVSVisibility` (object id, bit-layout, fail-safe query) into `PVS.Core`
- [x] 1.3 Make the sector provider abstract (`IPVSSectorProvider`) so the loader can be driven in-memory
- [x] 1.4 Point the shared `PVS` target at `PVS.Render`
- [x] 1.5 Remove the legacy editor adaptation: delete `plugins/pvs/editor/` and the `PVS.Editor` target, and drop
      it from `plugins/pvs/plugin.json`
- [x] 1.6 Abstract serialization too: add `IPVSSectorWriter` / `PVSSectorWriter` so the core owns both read and
      write of the PVS on-disk format

## 2. Build hygiene

- [x] 2.1 The unbalanced-brace compile break lived in the removed `PVSWorldBuilder.cpp`; resolved by removing the
      editor adaptation
- [x] 2.2 The plugin-escaping include lived in the removed `PVSWorldBuilder.cpp`; resolved by removal
- [x] 2.3 Clean up the runtime `PVSCulling.cpp` stray line-continuation backslash and the dead
      `image.format = colorFormat` assignment in `PVSDrawIDPass` (the latter removed with the editor adaptation)
- [x] 2.4 No editor target remains to build, so the `SKY_BUILD_EDITOR=ON` verification is moot

## 3. Ownership and lifetime

- [x] 3.1 Confirm the per-view culling view data ownership (already a `CounterPtr`-owned `RefObject`; no leak),
      and document it
- [x] 3.2 Null-guard `loader` in `PVSCulling::PrepareCullingViewData` and the visibility pointer in
      `QueryVisible`
- [x] 3.3 The per-build `Buffer` / `ResourceGroup` growth lived in the removed bake primitive; moot for the runtime

## 4. Streaming and query semantics

- [x] 4.1 Make the query fail safe: unavailable/invalid visibility data SHALL report visible, and the reserved
      invalid object id SHALL be rejected without indexing
- [x] 4.2 Resolve the sector from the queried cell (not the main-view `currentSector`) so cells in loaded neighbor
      sectors are answered
- [x] 4.3 Record the sector miss when the load is absent, without failing the streaming update

## 5. Tests

- [x] 5.1 Add loader streaming tests covering the load/unload hysteresis band
- [x] 5.2 Add query tests covering valid/culled/unavailable and invalid-id cases
- [x] 5.3 Remove the unnecessary legacy `RenderCore` linkage from the core tests (`PVSTest` links `PVS.Core`)
- [x] 5.4 Add neighbor-sector query and missing-sector tests
- [x] 5.5 Add `PVSConfig` / `PVSSector` serialization round-trip tests

## 6. Follow-up

- [ ] 6.1 Open a separate change for the PVS-to-Aurora migration if the runtime culling glue is to be revived
      (the editor bake pipeline is removed, not migrated)
- [x] 6.2 Keep PVS out of `configs/modules_game.json` and `configs/modules_editor.json` until it is migrated
