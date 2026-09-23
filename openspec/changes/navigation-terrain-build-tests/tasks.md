## 1. Harness

- [ ] 1.1 Add a World-level nav mesh build harness (World + `NavigationSystem` + registered recast factory + injectable geometry sources)
- [ ] 1.2 Provide a helper to build terrain LOD0 geometry (flat / sloped) into the harness
- [ ] 1.3 Provide a helper to add collision-component geometry to the harness

## 2. End-to-end tests

- [ ] 2.1 Test flat terrain LOD0 produces a walkable nav mesh over the region
- [ ] 2.2 Test sloped terrain LOD0 excludes areas steeper than the agent max slope
- [ ] 2.3 Test terrain provider + collision geometry combine into one nav mesh
- [ ] 2.4 Test terrain-only build is unaffected by the presence of the combined path

## 3. Validation

- [ ] 3.1 Build the engine and run the new integration tests with no regressions
- [ ] 3.2 Confirm the tests do not change engine/nav/terrain behavior (test-only)
