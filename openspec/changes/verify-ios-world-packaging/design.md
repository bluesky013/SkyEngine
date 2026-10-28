## Authoring, cooking and runtime

`.world` remains editable JSON. The existing Content builder converts it to a typed `World` asset whose schema and checked codec are in Framework, the runtime consumer. Source UUID, checked product saves, `product.index`, dependency headers and product audits are reused. Framework does not depend on Content or Aurora. CoreReflection uniquely owns the WorldAssetData schema; AssetManager installs its builtin World handler on each manager lifecycle; Content.Builder owns the offline builder. Unknown metadata fails explicitly rather than silently dropping components.

The v1 payload has magic/version/length, actor UUID/name records, component type/Data type/payload records, subsystem name/enabled/config type/payload records and resource roots. Scalar bytes use fixed-width little-endian encoding. Reflected values retain field names/type identities and lengths. String, record count, depth, cumulative node and byte budgets precede allocation. The decoder rejects truncated/invalid/unknown values, duplicate identities and invalid hierarchies. It publishes a completely instantiated candidate World only on success. Existing typed BinaryArchive bodies are unchanged; the outer product envelope is defined by `simplify-asset-product-storage`; the new codec serializes persistence Data instead of editor-facing properties and never wraps JSON text.

Packaging carries `defaultScene` into the existing game config. GameApplication loads the product through AssetManager with ProductsOnly after module Start hooks have registered runtime services. Its application Tick advances the World before module presentation, independently of BUSY renderer submissions. Shutdown stops the World and removes its provider before unloading modules. Camera and lights are World data.

## Presentation protocol

ILauncherSceneSource remains in the existing Aurora adaptor target. A single owner registers an ILauncherSceneSession using an actual module lease and an exact host/generation token. Append receives scene, main view, output and extent; OnEvent receives events from the sole poll owner. Main view revision remains host-owned. Session snapshots retain callback code while a callback replaces/unregisters itself. Replacements retire the old scene generation before using a new Bridge. Provider-only mode has no launcher scene service.

The default presentation connector consumes Framework IGameWorld and only extracts/render-presents; it does not tick simulation. Explicit sessions can take over and then return to the default World. No second packet producer or event polling loop is introduced.

Logical camera projection stays in logical view space. Frame composition owns the target's `logicalToClip` mapping; Runtime must not pre-rotate the same camera before the scene feature applies that mapping. Android's non-identity Vulkan surface transform exposes duplicate mapping that identity Metal outputs hide.

Prepare drain hooks retain their job state during callbacks. Extracting a `std::function` must explicitly clear the source: libc++ may copy a small callable during move, so move alone can leave State -> dispatcher callback -> State alive after completion. Callback transfer keeps a local strong pin while clearing the original owner. Repeated completion/consumer retirement tests cover capture and product destruction independently of frame allocation budgets.

## Package and fixture boundaries

World logical paths remain stable even when a permitted internal symlink resolves elsewhere inside assets. Only path validation resolves symlinks. The native auditor resolves selected logical paths through existing product indexes, checks type and walks the existing dependency closure. Authoring documents are not staged into a special World directory.

Product storage is now defined by the successor `simplify-asset-product-storage`: one checked, UUID-addressed flat file per product, with no product group, owner manifest, generation directory or old-layout reader. Android uses ordinary asset filtering and packaging audits the UUID dependency closure. Clear old cook output and recook before packaging. Existing grouped-package verification describes the intermediate implementation and must be rerun against the flat format; it is not a compatibility promise.

The Android Emulator's SwiftShader parser rejects Slang's optional `OpSource Slang(11)` metadata. The shared SPIR-V finalizer validates word boundaries and entry declarations, omits only `OpSource` and `OpSourceContinued`, and retains every other instruction and reflection record. It publishes code/entry together on success. A target-specific producer policy changes SPIR-V cook/cache identity; MSL/DXIL identity and runtime code paths remain unchanged. This is a general artifact policy rather than a simulator environment override.

The smoke fixture contains the camera and validates the ordinary template/defaultScene path. Simulator SDK, architecture and dependency roots are parameters; local absolute paths are not public defaults. Device archives remain unsigned without developer credentials. Runtime rendering is verified in Simulator, not claimed as physical-device validation.
