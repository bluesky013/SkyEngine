## Context

Runtime modules are SHARED libraries that self-register through `REGISTER_MODULE`
(`engine/framework/include/framework/interface/IModule.h`, which defines `StartModule`/`StopModule` and calls
`Environment::Attach/Detach`) and are loaded by `ModuleManager` (`framework/src/application/ModuleManager.cpp`,
tracking `dynLibs`). The project packaging done by `python/package_project.py` (see `add-game-project-build`)
currently produces the **all-dynamic** shape: one shared library per module.

The intended convention is a **static core** plus a **dynamic registration shim** (the shim only wires the module
into the registry). This split is **not yet realized** across the engine/plugins (every module is a single SHARED
library today), so this change must also establish/verify it - see task 1.1.

Two packaging shapes are therefore possible from the same sources:

1. **Dynamic** (today): every module is a separate shared library, loaded at runtime.
2. **Static bundling**: selected modules have their static cores linked into the host/bundle per rules; only the
   dynamic registration behavior that is still required is kept.

## Goals / Non-Goals

**Goals:**
- Support both packaging shapes from the same module sources, controlled by build/packaging options.
- Keep dynamic loading as the default (no behavior change for existing projects).
- Expose the mode + rules through the project export preset (`add-game-project-build`) and the engine build system.

**Non-Goals:**
- Hot reload of modules.
- Per-module ABI/versioning.
- Module-level codegen changes.

## Decisions

### D1. Two-mode model with per-module static/dynamic parts
Each module keeps its convention: a static part (`<Name>`) and a dynamic registration shim (`<Name>.Dynamic`).
Packaging mode selects whether the shim is emitted as a SHARED library or replaced by a compiled-in registration
call:
- **dynamic**: link `<Name>.Dynamic` as SHARED (today's behavior).
- **static**: link `<Name>` (core) into the host/bundle and compile the registration in, so no `.so` is needed.

### D2. Central selection + rules, not per-module edits
The mode and the static set are declared once (build option + project preset), not by editing each module. The
engine derives, per module, whether to build the shared shim, the static core, or both. Rationale: avoid N edits and
keep the split uniform (alternative considered: per-module opt-in flags in each `plugin.json` — rejected as
error-prone and hard to reason about globally).

### D3. Runtime resolution order and environment attachment
`ModuleManager` resolves modules provided statically (compiled-in registration) first, then dynamically loaded ones.
A module declared both static and dynamic resolves to the static instance; the loader must not double-register.
`REGISTER_MODULE` calls `Environment::Attach(env)` on start and `Environment::Detach()` on stop (the cross-DLL
`Singleton` environment); the compiled-in registration path must attach/detach the shared environment exactly once
and must not assume the module's own `DynamicModule` handle exists.

### D4. Option surface
The packaging options reuse the `add-game-project-build` preset schema: a `packing` mode (`dynamic` | `static` |
`mixed`) plus a `staticModules`/rules list, mapped to engine build options (e.g., `SKY_MODULE_PACKING`,
`SKY_STATIC_MODULES`) via `cacheVariables`. CLI/preset/env precedence matches that change.

### D5. Platform constraints
Some platforms restrict loading unsigned code (iOS): a project targeting them may require static bundling. The mode
is a packaging choice, not a hard platform switch, but the design must allow a platform preset to default to static.

### D6. Revised mechanism: tagged factory + plugin-config-driven generated registration

Implementation refines D1/D2/D4 (no `<Name>.Dynamic` shim split, no hand `SKY_STATIC_MODULES` list):

- **One macro**: `REGISTER_MODULE_TAGGED(tag, name)` (`IModule.h`). `SKY_MODULE_STATIC` defined → a uniquely named
  factory `sky_module_create_<tag>()` (tag = the module's target/module id); otherwise it falls back to the existing
  `REGISTER_MODULE` dlopen shim. So a module is compiled SHARED (dynamic) or STATIC (tagged) from the same source.
- **Source of the static set = the plugin config**: `plugins/plugins.json` (`enabled`) + each `plugins/<x>/plugin.json`
  `targets` tagged with `kind` (`runtime`/`render`/`editor`), plus an engine-side `engine/runtime_modules.json` for
  non-plugin runtime modules. This reuses the existing "which plugins are built" source of truth (D2) with no new list.
- **Generated registration cpp** (configure-time, only when `SKY_MODULE_PACKAGING=static`): reads that config and emits
  `RegisterStaticModules.cpp` declaring `sky_module_create_<tag>()` and calling `ModuleManager::AddStaticModule("<id>", …)`.
- **Option**: `SKY_MODULE_PACKAGING=dynamic|static` (`cmake/options.cmake`), default `dynamic` (no behavior change).
- **Runtime**: `ModuleManager::AddStaticModule` registers compiled-in modules; `LoadModules` skips dlopen for them and
  still calls `Init`. Static modules share the host process `Environment` (no `Attach`/`Detach`).

### D7. Resolution diagnostics

`ModuleManager` reports module-resolution problems explicitly (needed for both shapes):

- **Missing static registration** (`SKY_MODULE_STATIC` build): an id that is neither statically registered nor
  loadable dynamically logs an error stating it must be compiled in (`sky_add_module` + `REGISTER_MODULE_TAGGED`) and
  listed in the runtime module config.
- **Dynamic load failure**: the loader's error string (`DynamicModule::GetLastError`) is included in the log.
- **Duplicate static registration**: `AddStaticModule` on an existing id logs a warning and keeps the first
  registration (deterministic; no double `Init`).
- **Editor layering (2.6) is by construction**: only runtime modules declare `sky_add_module`, so editor/`requires_editor`
  modules stay dynamic and never enter a runtime static bundle (convention + docs; a `kind`-based filter is only
  needed if selection becomes plugin-config-driven).

## Risks / Trade-offs

- [Duplicate symbols] -> statically linking many modules can collide (especially third-party). Mitigation: bundle as
  static OBJECT/user libraries with symbol visibility rules; keep third-party shared where needed.
- [Registration order] -> compiled-in registration order is undetermined; D3 requires resolving order deterministically
  (or raising a load error on cycles).
- [Double registration] -> a module both statically linked and present as a shared lib must not register twice (D3).
- [Editor vs runtime layering] -> editor-only plugins must not be pulled into a static runtime bundle
  (`engine-plugin-layering`). Mitigation: respect `requires_editor` and the module classification.
- [Build complexity] -> two shapes per module increases CMake wiring; keep a single shared helper rather than
  ad-hoc per-module logic.

## Migration Plan

- **Prerequisite**: `add-game-project-build` (dynamic packaging) and `add-project-native-layout` (the `native/`
  layout + engine entry this change's options plug into) SHOULD land first.
- Ship dynamic-only first. This change adds the static path behind a mode option defaulting to `dynamic`, so existing
  projects are unaffected.
- Rollback: unset the mode (back to `dynamic`).

## Open Questions

- Exact rule format (explicit list vs group/resolver-based selection, e.g. "all runtime plugins").
- Whether static bundling produces one host binary or one intermediate static bundle linked by the host.
- How packaged (cooked) module metadata describes the static set to the runtime.
