## Context

Current Boost usage (from a repo-wide scan):

- `engine/core/include/core/std/Container.h` - `boost::container::pmr` aliases, but only on non-Windows
  (`SKY_USE_BOOST` is defined `#ifndef WIN32`; Windows already uses `std::pmr`). This is the only place that needs
  Boost's **compiled** library (`boost::container`).
- `engine/framework/.../ModuleManager` - `boost::adjacency_list` + `boost::topological_sort` (header-only).
- `engine/shader` - `boost::tokenizer` (ShaderCompiler) + `boost::graph` (ShaderVariant) - legacy compiler.
- `engine/render/core` (legacy RDG) - `boost::graph` (RenderGraph/AccessGraphCompiler/RenderGraphExecutor).
- `engine/aurora/core` links `ShaderCompiler.Static` but does not use it (stale link).

`Findboost.cmake` currently declares `LIBS container graph` (expects a compiled `boost::container`).

## Goals / Non-Goals

**Goals**
- Core and Framework build without Boost.
- A small, sufficient directed-graph utility in Core replaces Boost graph for module ordering.
- Boost becomes header-only and is linked only by the legacy shader and legacy render modules.

**Non-Goals**
- Removing the legacy shader compiler or legacy render, or rewriting the legacy RDG.
- Changing module load/unload ordering semantics.

## Decisions

### D1. Core PMR uses `std::pmr`
Remove the `SKY_USE_BOOST` branch from `core/std/Container.h`; always alias to `std::pmr`
(`unsynchronized_pool_resource`, `monotonic_buffer_resource`, `memory_resource`, `vector`, `list`, `string`, and
`std::pmr::unordered_map` for `PmrHashMap`). The Windows path already does this, so the change is
removing the alternate branch rather than introducing new behavior. Drop `3rdParty::boost` from
`engine/core/CMakeLists.txt`.

### D2. A minimal Core graph utility
Add `engine/core/include/core/std/Graph.h` (header-only, `sky::Graph`):

- `using Vertex = uint32_t; using Edge = std::pair<Vertex, Vertex>;`
- `Vertex AddVertex();` - append a vertex, return its index.
- `void AddEdge(Vertex from, Vertex to);` - `from` depends on `to`; duplicate edges are ignored.
- `uint32_t VertexCount() const;`
- `std::vector<Vertex> TopologicalSort() const;` - returns vertices with every dependency before its dependent
  (for edge `from->to`, `to` precedes `from`). Deterministic via Kahn's algorithm with a FIFO queue.
- `std::vector<Edge> const &Edges() const;` (optional, for debugging).

Rationale: only module ordering needs a graph; a ~60-line utility is enough. Cycles are reported (return the
partial order or assert) - the module graph is expected to be acyclic.

### D3. ModuleManager uses the Core graph
Replace `boost::adjacency_list` with `sky::Graph`:

- `using Graph = sky::Graph;` and `vertex_descriptor` becomes `uint32_t`.
- `RegisterModuleImpl` -> `graph.AddVertex()`; `RegisterModule` -> `graph.AddEdge(src, dst)`.
- `TopoSort()` -> `sortedContainer = dependencyGraph.TopologicalSort();` (dependencies first, matching the current
  `boost::topological_sort` + `back_inserter` behavior).
- `UnLoadModules` walks `sortedContainer` in reverse instead of `boost::front_inserter`.

### D4. Boost isolated to legacy modules and made header-only
- Add `3rdParty::boost` explicitly to `engine/shader` and `engine/render/core` (and `engine/render/builder`
  if it uses boost).
- Rewrite `Findboost.cmake` as an INTERFACE target adding Boost headers only (no `sky_3rd_static`), since
  `boost::graph`/`boost::tokenizer` are header-only.
- Optionally mark the Boost third-party package header-only in `cmake/thirdparty.json`.

### D5. Clean the stale Aurora link
Remove `ShaderCompiler.Static` from `engine/aurora/core/CMakeLists.txt` (Aurora core does not reference it), so
building Aurora does not pull the legacy compiler.

## Risks / Trade-offs

- [`std::pmr` vs `boost::container::pmr` behavior] -> both provide the same resource types; Windows already uses
  `std::pmr`. Verify pool resources behave as expected in Core tests.
- [Graph semantics mismatch with Boost] -> define and test the exact order; cover `ModuleTest` and a new unit test.
- [Hidden Boost use in legacy modules] -> the isolation step fails the build if a non-legacy module still needs
  Boost, which is the intended signal.
- [Boost package still cloned for headers] -> acceptable; only the compiled library is dropped.

## Open Questions

- Whether to also mark the Boost package `header_only` in `cmake/thirdparty.json`.
- Whether the legacy shader/render modules stay in the default build for now (they do; Boost remains for them).
