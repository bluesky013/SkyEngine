## 1. Core PMR

- [x] 1.1 Remove the `SKY_USE_BOOST` branch in `engine/core/include/core/std/Container.h`; always use `std::pmr` (`PmrResource/PmrUnSyncPoolRes/PmrSyncPoolRes/PmrMonoBufferRes/PmrVector/PmrList/PmrString/PmrHashMap`)
- [x] 1.2 Drop `3rdParty::boost` from `engine/core/CMakeLists.txt`

## 2. Core graph utility

- [x] 2.1 Add `engine/core/include/core/std/Graph.h` (`sky::Graph`: `AddVertex`, `AddEdge`, `VertexCount`, `TopologicalSort`; dependencies first, deterministic, cycle-aware)
- [x] 2.2 Add a Core unit test for the graph (linear order, diamond, duplicate edges, empty/single vertex, cycle handling)

## 3. ModuleManager

- [x] 3.1 Replace `boost::adjacency_list`/`boost::topological_sort` in `ModuleManager` with `sky::Graph`; change `vertex_descriptor` to `uint32_t`
- [x] 3.2 `TopoSort()` uses `Graph::TopologicalSort()`; `UnLoadModules()` walks the load order in reverse
- [x] 3.3 Confirm load order (dependencies first) and unload order (reverse) via `ModuleTest`

## 4. Boost isolation

- [x] 4.1 Add `3rdParty::boost` explicitly to `engine/shader` and `engine/render/core` (and `engine/render/builder` if used)
- [x] 4.2 Rewrite `cmake/thirdparty/Findboost.cmake` as a header-only INTERFACE target (no `sky_3rd_static`)
- [x] 4.3 Mark the Boost package `header_only: true` in `cmake/thirdparty.json` (bootstrap now configures + installs headers only)

## 5. Aurora cleanup

- [x] 5.1 Remove the stale `ShaderCompiler.Static` link from `engine/aurora/core/CMakeLists.txt`

## 6. Verification

- [x] 6.1 Configure + build the engine; confirm no non-legacy target links Boost
- [x] 6.2 Run `FrameworkTest` (module load/unload, serialization) - all green
- [x] 6.3 Run the new graph unit test
