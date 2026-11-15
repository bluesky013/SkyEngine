## Context

Preferences already provides reflected pages, owned working copies, undo history and atomic file storage. ProjectDescriptor exposes ConfigsDir. The engine repository contains the primitive mesh sources and Aurora material shaders.

## Decisions

- A separate ProjectEditorSettings singleton owns the project page registry. The shared settings dialog uses project ConfigsDir/editor.json; user preferences keep their own registry and file.
- Aurora.Editor owns MeshCreationSettings with one material UUID field. Reflection provides the asset picker and tooltip. The extension initializes it from materials/lit.mat through AssetDataBase and validates the selected AuroraMaterial in the catalog.
- Aurora.Editor registers Cube/Plane directly in EditorActorCreation and resolves shapes/cube.glb or shapes/ground_plane.glb through AssetDataBase. The same owner copies its configured material before the create command is recorded; Sandbox does not dispatch string-based configuration callbacks to every extension. Unregistration revokes retained creation callbacks.
- StaticMeshComponentData remains UUID-only with its existing shadow flags. No per-component procedural description, source enum, typed resource reference or global builtin-resource singleton is introduced.
- New objects retain their copied references across Apply, undo/redo and world serialization. Existing objects are not retargeted when the project default changes.
- AssetBuilderManager layers project builder config files over engine config files. Project cook JSON (including the legacy filename) takes priority over engine cook JSON. Sandbox supplies the engine root; missing config uses defaults and still registers the common product bundle. Malformed cook JSON returns failure before publishing configuration, checked by Sandbox and AssetTool.
- The normal asset cook/load pipeline handles these sources. No synchronous material cooking is added to UI creation callbacks. Material identity lives in assets/materials/assets.jsonl, not a C++ UUID literal.
- Adopt the four old runtime resource IDs as manifest identities of real Engine source files: cube.glb (unit cube), ground_plane.glb (four-unit plane), lit.mat and unlit.mat. The GLBs preserve the old positions, winding and bounds. Existing scene UUIDs therefore resolve through the source/product catalogs without a field migration or virtual display aliases. Existing box.glb, plane.glb and basic_lit.mat retain their identities for scenes already referencing those files.
- Remove the runtime fixed-ID dispatch and procedural material/mesh generation. Preview and navigation look up Engine source/product paths through AssetManager.ResolveAssetPath; the same loader handles user and Engine assets. Synthetic geometry/materials used by renderer unit tests live in a private test fixture and are explicitly injected.
- Engine assets participate in normal component asset-reference collection and world dependency roots. Mesh cooking publishes complete material slots and deduplicated product dependencies; an unspecified slot table resolves the repository Unlit asset in the cooker. Missing fallback assets fail cooking. Component serialization only collects its explicit fields. Old products retain runtime fallback support, but must be recooked before packaging with the new dependency contract. Aurora.Editor no longer registers Builtin virtual entries; display paths and hover hints come from actual catalog items.
- Reuse UUID scalar persistence and the user-approved migration of PreferencesPersistence, PreferenceOperations and guards to explicit PreferenceResult/bool failures. Preserve working copies, history, partial-effect flags and cleanup. Standard-library exceptions are not caught.
- An absent config file uses provider defaults. Invalid loaded config blocks mesh creation until the settings file is repaired and the editor restarted. Invalid settings encountered while saving report the same recovery action. The preserving JSON save path does not overwrite malformed input implicitly.

## Validation

Test repository mesh creation, world save/reopen, failed creation without an asset, authored material decoding, project/user isolation, malformed UUIDs, material type validation and snapshot semantics. Build the actual editor modules and run related regression suites. Record native editor evidence separately from automated results.

## Review remediation

- Remove/move validate the source mount in AssetDataBase. Persistence failure retains cached identity; failed moves roll back the file and any committed destination manifest, reporting rollback failures. Engine sources cannot bypass read-only protection through direct calls.
- SceneAssetSource retires all asset watches with the invalidated cache generation; it does not consume a lifetime-wide watch budget. Published resource snapshots keep their own ownership.
- Preferences and Project Settings share one guarded opening flow. Asset field labels and hints consume one catalog item per lookup.
- Existing duplicate-looking assets with stored references are retained. Directory moves and new modules are outside this change.
