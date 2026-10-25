# Material Editor View

Status: implemented contract, 2026-10-25; native UI interaction acceptance is recorded separately from compilation and model tests.

`MaterialEditorView` loads `.mat` / `.mati` through MaterialReflection. It resolves the
complete Material schema from the referenced shader declarations/products, then shows
canonical keys and typed values. Material v4 persists these values; v3 aliases and technique
options are explicit migration inputs. MaterialInstance v2 exposes inherited ordinary values,
textures and samplers, excluding static options. Unknown/conflicting mappings are diagnostics.

Each generated key owns a typed `MaterialPropertyData` model and a transient reflection node
created by the adaptor's `MakeMaterialPropertyReflection`. The keys are runtime shader metadata
and cannot be registered as a fixed C++ structure. The shell binds these nodes to the existing
`ReflectedFormView`; it does not create per-parameter controls. Nested samplers and texture
pickers use the same generic editor registry. Commands carry generation leases so reloading
cannot apply an old edit to a different property. Undo of the first inherited-value edit removes
the instance override. The existing IAssetViewDocument owns each window's CommandService;
schema rebuilds and queued saves run from its host tick, outside input dispatch. Save validates
the regenerated schema before writing and requesting cook. Module leases outlive content and history.

Technique rows still expose Tag, explicit source/entry and render-state fields through the
existing GridPanel/UiTheme/UiSkin path. Collapse and migration of those legacy manual technique
controls are separate UI work. Runtime support remains narrower than the full editable
render-state schema; unsupported scene state fails explicitly.

Schema resolution currently uses the staged development bundle (Vulkan on Windows, Metal on
Apple). Missing source/compiler/product metadata is shown as a schema error. Ordinary runtime
rendering still consumes the selected backend's reflection and upload plan.

See [shader design and migration](../aurora/shader-design-and-migration.md) and the active
variant change's verification record for tested boundaries. Compilation/model tests do not
claim manual editor interaction or native Metal acceptance.
