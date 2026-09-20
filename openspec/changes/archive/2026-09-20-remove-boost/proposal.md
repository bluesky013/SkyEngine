## Why

Boost is the engine's heaviest third-party dependency, and after `core/std/Container.h` was written with a
`std::pmr` fallback, the only reason Core still pulls Boost in is the non-Windows PMR branch. The remaining Boost
uses are: `ModuleManager` (framework) graph topological sort, the legacy shader compiler (`engine/shader`), and the
legacy render dependency graph (`engine/render`). Isolating Boost to the legacy modules lets the current engine
(Aurora + core + framework) build without Boost and removes Boost's compiled library (`boost::container`) from the
bootstrap, which matters most on Android.

## What Changes

- **Core PMR**: drop the `SKY_USE_BOOST` branch in `core/std/Container.h`; always use `std::pmr`
  (`PmrVector/PmrList/PmrString/PmrHashMap/PmrResource`...). Remove Core's link to `3rdParty::boost`.
- **Core graph utility**: add a small, self-contained graph helper in Core (vertices, directed edges, topological
  sort) sufficient for module ordering - no Boost.
- **ModuleManager**: replace `boost::adjacency_list`/`boost::topological_sort` with the Core graph utility,
  preserving load order (dependencies first) and unload order (reverse).
- **Boost isolation**: make the legacy modules (`engine/shader`, `engine/render`) link `3rdParty::boost`
  explicitly, and turn `Findboost` into a header-only interface target (after the PMR migration the remaining
  Boost uses - `boost::graph`, `boost::tokenizer` - are header-only).
- Clean the stale `ShaderCompiler.Static` link on `engine/aurora/core` (Aurora sources do not use it).

**Non-goals**: removing the legacy shader compiler / legacy render itself, and rewriting the legacy RDG.

## Capabilities

### New Capabilities
- `core-graph`: a minimal, self-contained directed-graph utility in Core (vertices, edges, topological sort).
- `boost-isolation`: Boost is limited to the legacy shader and legacy render modules; Core and Framework do not
  depend on Boost.

### Modified Capabilities
<!-- none -->

## Impact

- `engine/core` (`std/Container.h`, new graph header, `CMakeLists.txt`), `engine/framework` (`ModuleManager.*`,
  `CMakeLists.txt`), `engine/shader` and `engine/render/*` (`CMakeLists.txt`), `cmake/thirdparty/Findboost.cmake`,
  and tests (`ModuleTest`, a new graph unit test).
- Builds on `engine/core` and `engine/framework`; no runtime behavior change for module ordering.
