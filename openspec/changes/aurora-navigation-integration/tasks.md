## 1. Aurora debug view

- [ ] 1.1 Design an aurora debug-draw hook for the nav mesh (RDG/debug renderer)
- [ ] 1.2 Render the `NaviDebugGeometry` triangle list (from `NaviMesh::BuildDebugGeometry`) under aurora

## 2. Runtime wiring

- [ ] 2.1 Create a game-runtime world and attach `NavigationSystem`
- [ ] 2.2 Trigger the nav mesh build in the game runtime (not only the editor tool menu)
- [ ] 2.3 Fix the editor `IWorldBuilderGather` timing so the "Build Navigation" action appears

## 3. Verification

- [ ] 3.1 Verify the nav mesh debug view renders under aurora
- [ ] 3.2 Verify navigation loads and builds without any legacy render dependency

## Related backlog

- [ ] 4.1 `navigation-path-query`: `NaviPath` output API + query-filter factory + nav components (split into its own change)
