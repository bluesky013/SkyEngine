## Context

The approved source plan is `../fix-asset-reference-boundaries/architecture-follow-up.md`. This change is now the implementation and acceptance owner for that plan. Existing functional fixes, project defaults and tooltip changes remain intact and are not reimplemented here.

Aurora.Adaptor.Static currently owns both persistent codecs and runtime/world integration. Aurora.Metadata calls the combined reflection entry. WorldEditorState knows concrete Aurora components and implements skeletal editing policy. Existing Framework persistent-component registration and EditorExtensionHost provide the required integration seams.

## Goals / Non-Goals

Goals: one reusable persistent-data implementation owner; Cook independent of Adaptor; unique persistent/runtime metadata owners; generic editor transactions with Aurora-owned policy; unchanged UUIDs, persisted fields and visible editing behavior.

Non-goals: directory relocation, additional plugins/registries, generic renderer hierarchy changes, FrameGraph/backend changes, new asset formats, complete isolation from every renderer-related Core symbol, broad historical error-path rewrites.

## Decisions

1. Define Aurora.Assets.Static in the existing adaptor CMake file. It explicitly owns Mesh/LodGroup codecs, material reflection and pure definition processing, Skin codec, asset handlers and persistent component data reflection. Mixed files are split in place. Adaptor excludes those exact sources from its collection and retains runtime resource conversion, component behavior and renderer integration. Cook links Assets. Existing Framework/Core/Animation/Aurora.Core.Static dependencies provide shared types; Assets does not link Adaptor, Pipeline, Editor, Cook or a concrete RHI backend. A header-only duplicate or copying sources into each consumer would violate the single-owner contract.
2. Separate CPU asset declarations from MaterialResource/ImageResource/SkinResource conversion entrypoints. Persistent ComponentData stays in its existing component domain via a CPU-only header; runtime components include it. Preserve serialized names, values, IDs and format versions.
3. Aurora.Metadata owns RegisterAuroraAssetTypes, including persistent component ID-to-data mappings through ComponentFactory::RegisterPersistentDataType. Aurora.Render owns RegisterAuroraRuntimeTypes and runtime creators. Its definition depends on Metadata. Aurora.Render.Editor explicitly depends on Render and Metadata. Init/Shutdown retain only session service responsibilities. Test executables initialize metadata once in main or their gtest Environment. No idempotent guards or full reflection alias can mask duplicate owners.
4. Metadata-only World cook uses the existing Content.Builder.Module and the required framework/content metadata, plus the Aurora.Metadata/Aurora.Cook modules needed by the requested products. It must not load Aurora.Render or create a window/Device merely to inspect persisted Aurora components. There is no second cook tool or registration protocol.
5. Extend EditorExtension/EditorExtensionHost with two neutral hooks: prepare a property edit and prepare a detached component addition. Results distinguish Unhandled, Ready with an owned command, and Error. The host rejects multiple handlers and invalid Ready results. Property preparation leaves the world unchanged; detached-component preparation may change only the detached object. An error never falls back to generic property assignment. Interfaces contain Framework and EditorCore types only.
6. Aurora.Editor owns light/camera/environment creation plus skeletal mesh binding and animation pose/skeleton policy. WorldEditorState keeps Empty creation, document/selection/dirty, generic dependency activation and attach/detach. Hooks validate the actual binding owner/type, not a field-name coincidence. Domain commands participate in the host's existing command history; dependency activation, attach and domain changes form one reversible operation with checked failure compensation. Commands resolve actors by identity with bounded world/document ownership; document rebind clears history before releasing bindings. No window captures or module-code leases are introduced.
7. Aurora.Cook builders belong to the process definition, while MeshAuthoringCompiler is registered per runtime session. Their shared MeshBuildPresets belongs to the uniquely compiled, process-resident Aurora.Cook module entrypoint and is passed explicitly to both consumers. A module-local shared pointer retains this definition state across manager recreation; it is neither a cross-DLL registry nor a reflection guard. This prevents a recreated compiler from silently receiving fresh defaults while the original builder retains loaded project presets.

## Risks / Trade-offs

- Static libraries can hide missing ownership through transitive links: validate real Assets, Cook, Adaptor, Metadata, Render and Editor modules plus AssetTool, source ownership and actual CPU-only World cooking.
- Asset<T> caches runtime resource types and Core contains renderer foundation code: preserve this existing lower-level contract and do not claim fully renderer-free linkage.
- Reflected registration order can create duplicate or missing schemas: cover persistent-only registration, runtime definitions, retry and manager recreation without guards.
- Domain hooks could mutate live state before conflict detection: constrain preparation, reject conflicts, test no mutation on failure and full undo/redo.
- Editor history can outlive a selection: commands use stable actor identities and document-bound ownership; test rebind/unregister and failed attach.
- macOS verification cannot establish Windows/Linux behavior: retain the platform boundary explicitly in the final record.

## Migration Plan

Implement CPU ownership and metadata changes as one reviewable group, then editor-policy extraction as a second group within this architecture change. Reconfigure the existing build after source changes. Build real modules and run focused and integration suites. Launch the editor and check preserved creation/property behavior with a few screenshots. Keep both groups and their specs together for later user-approved commit; no archive until separately approved.
