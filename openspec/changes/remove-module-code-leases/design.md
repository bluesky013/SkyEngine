## Contract

Plugin and RHI code loaded by DynamicModule remains available until process exit. Module configuration changes requiring another binary take effect after restarting. Closing a module session still withdraws its services and destroys/drains its runtime objects; code residency is not service availability or ownership of captured data.

## Native library owner

An implementation-private ResidentLibraries derives from sky::Singleton to share the cache through Environment across statically linked Core copies. It caches successful native library references by module name. Native loading runs outside the registry mutex because native initializers can load other libraries. Concurrent first loads may acquire additional native references; all successful references deliberately live until process exit. Failed loads are not cached.

DynamicModule Load attaches to this process owner; its destructor/Unload only clears the local handle view. The owner does not call FreeLibrary/dlclose. This also covers RHI and compiler clients that load through DynamicModule outside ModuleManager.

## Shared singleton lookup

Resident DLLs retain their statically linked Core copies across service destruction and recreation. Singleton therefore resolves the current object from the shared Environment on every access instead of caching a DLL-local raw pointer. Environment atomically finds or creates an object under its recursive registry mutex; constructors may resolve another singleton type. Destroy atomically removes the registry entry and deletes the object outside the lock. Callers must still drain object users before destroying a service; this does not make concurrent object destruction safe.

## Module sessions and metadata

ModuleManager keeps only unique wrapper/module instances and a set of successfully initialized sessions. Dependency preflight checks that set, never whether a native library happens to be resident. Init failures run Shutdown and destroy the partial instance; consumers of failed dependencies are not initialized.

Start/Tick operate only on initialized sessions; failed Init receives Shutdown for partial cleanup, while cold instances receive no service callbacks. Shutdown removes each session from the readiness set before its callback and destroys that instance before stopping its providers. Reverse dependency order therefore covers both shutdown callbacks and destructors. Manager destruction shuts down remaining sessions. The former Current() helper existed only to obtain leases and is removed.

A single Idle/Initializing/Dispatching/Stopping phase owns host-thread lifecycle dispatch. Init/Start/PreTick/Tick/PostTick cannot recursively load, unload or dispatch callbacks. LoadModules, StartModules, Tick and UnLoadModules return false when the active phase rejects a transition. RegisterModule, AddStaticModule and TopoSort also return false during lifecycle callbacks, preventing mutation of the containers being traversed. Repeated shutdown during Stopping is harmless; rejected teardown can be retried after dispatch returns. This does not make ModuleManager a concurrent multi-threaded service.

ModuleDefinitions remains a process Singleton keyed by module identity. It invokes RegisterMeta only for a new definition, including after failed Init, manager replacement or another runtime session. No code-lease scope, weak library wrapper, StopModule/Environment detach callback or duplicate-reflection guard is needed. Runtime service registration remains symmetric.

## Removing code ownership plumbing

- SerializationContext, component/asset/builder registries and World subsystem registration retain their data/callbacks without code tokens. World continues to own subsystem instances and apply transactional creation/rollback.
- Animation request/result types retain operation, asset and resource ownership. Skeleton/Clip/AnimationWorldHost factories need no module argument. Source interfaces are still withdrawn by their registration owner.
- RegisterRenderFeature retains the actual Feature shared_ptr. FrameFeatureLease, per-frame KeepAlive, resource pins, native output borrowers and GPU retirement remain intact.
- ShaderCompilerLease now owns only the compiler instance. Async futures retain compiler and writer objects after the service unregisters; DLL residency does not keep a compiler object alive.
- Jolt keeps SDK/session/world/mesh ownership and destruction order; only code tokens are removed.
- Editor action/extension/view/preview/picker code fields disappear. Scope/generation guards, providers, World/document/content, cursor/geometry and undo data retain their existing object ownership.

## Scope and compatibility

All implementation remains in existing targets and paths. Root/Aurora agent rules are updated to the new code-lifetime contract while preserving metadata uniqueness, cross-DLL Singleton use, explicit errors, task drain and object ownership requirements. Historical archived records are not rewritten. The two external PartySoftBody call sites move to the shorter Feature registration shape without changing gameplay or tuning.

## Validation

Build real Core/Framework and all affected plugin/runtime/editor targets, not only tests. Validate failed dependency/retry metadata behavior, cold instances, nested lifecycle rejection and destructor shutdown order. Dynamic/static module tests must observe withdrawn services and process-resident code after manager destruction. Shader/animation/Jolt/editor tests must continue proving real instance/snapshot lifetimes. Native rendering tests must still prove Feature/GPU/output retirement. Rebuild the external PartySoftBody modules against the new public registration API.

## Integration with origin 741a3371

The rebase preserves upstream Vulkan 1.1/KHR capability negotiation, owned launcher scene sessions, simplified AssetManager storage and image-local Jolt SDK initialization. Explicit launcher sessions take precedence over project scene sources, which take precedence over default World rendering; session ownership, retirement generations and reentrant shutdown protection remain intact.

New navigation factory/products/query tasks, editor actions/overlays, compressor providers and launcher sessions use the same process-resident code contract. Mesh/filter/world/session/compressor ownership and task completion are unchanged; code-only parameters disappear. Launcher registration no longer accepts a module name used only to acquire a code lease. Public RHI configuration is resolved once on the device, with RenderPass2 capability and commands required for fallback.
