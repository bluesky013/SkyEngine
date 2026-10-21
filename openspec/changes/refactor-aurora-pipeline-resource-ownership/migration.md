## File migration review

The module roots and persistent-export policy are confirmed. This list separates existing-file relocation from the functional implementation review, as required by AGENTS.md. Shared C++ declaration headers remain at their current source locations in this step; the registered logical include path determines each new persistent export.

| Existing file | Proposed file |
|---|---|
| engine/aurora/pipeline/shaders/SceneForward.slang | engine/aurora/assets/shaders/aurora/materials/Unlit.slang |
| engine/aurora/pipeline/shaders/SceneForward.slang.json | engine/aurora/assets/shaders/aurora/materials/Unlit.slang.json |
| engine/aurora/pipeline/shaders/SceneLit.slang | engine/aurora/assets/shaders/aurora/materials/Lit.slang |
| engine/aurora/pipeline/shaders/SceneLit.slang.json | engine/aurora/assets/shaders/aurora/materials/Lit.slang.json |
| engine/aurora/pipeline/shaders/SceneShadow.slang | engine/aurora/assets/shaders/aurora/materials/Shadow.slang |
| engine/aurora/pipeline/shaders/SceneShadow.slang.json | engine/aurora/assets/shaders/aurora/materials/Shadow.slang.json |
| engine/aurora/pipeline/shaders/SceneToneMap.slang | engine/aurora/assets/shaders/aurora/pipeline/ToneMap.slang |
| engine/aurora/pipeline/shaders/SceneToneMap.slang.json | engine/aurora/assets/shaders/aurora/pipeline/ToneMap.slang.json |
| engine/aurora/shader/assets/shaders/slang/config/pipeline_variants.slang | engine/aurora/assets/shaders/aurora/config/pipeline_variants.slang |
| engine/aurora/core/test/plugins/first/shaders/ToonPass.slang | engine/aurora/core/test/plugins/first/assets/shaders/first/pipeline/ToonPass.slang |
| engine/aurora/core/test/plugins/first/shaders/ToonPass.slang.json | engine/aurora/core/test/plugins/first/assets/shaders/first/pipeline/ToonPass.slang.json |
| engine/aurora/core/test/plugins/second/shaders/ToonPass.slang | engine/aurora/core/test/plugins/second/assets/shaders/second/pipeline/ToonPass.slang |
| engine/aurora/core/test/plugins/second/shaders/ToonPass.slang.json | engine/aurora/core/test/plugins/second/assets/shaders/second/pipeline/ToonPass.slang.json |

The relocation itself preserves algorithms and declaration layouts. Functional edits update includes to exported .slangh paths and update ShaderRef/companion/test/cook references. Unlit/Lit/Shadow are explicitly selected material programs; the generic ScenePass does not own a universal shader. Technique/binding implementation removes implicit fallback selection and fixed Batch assumptions.

Existing .h declarations are not moved or merged into similarly named public Pass class headers. New .slangh outputs are source assets under the confirmed owner roots. Production module/target ownership is unchanged.

Status: the user explicitly approved this list on 2026-10-21; all 13 files have been relocated. Include and consumer updates are implemented; fresh validation is recorded in tasks.md.
