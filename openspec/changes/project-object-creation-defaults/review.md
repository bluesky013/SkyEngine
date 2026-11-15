# Architecture review and remediation

Updated 2026-11-15 after consolidating the former Builtin resources into Engine assets.

| Finding | Final remediation |
| --- | --- |
| Component data contained procedural resource definitions. | Keep only existing mesh/material UUIDs and shadow flags. |
| A builtin service duplicated asset ownership and resolution. | Use real repository GLB/material sources, existing catalogs and cook/load pipeline. |
| Fixed runtime IDs bypassed asset loading and dependency collection. | Adopt the four old identities in source manifests; remove all production ID dispatch and procedural generation. |
| Rollback exposed GUIDs again in legacy scenes. | Actual Engine catalog records supply paths and generic hover hints; no virtual Builtin display entries remain. |
| An implicit material could be omitted during packaging. | Mesh cookers publish complete material slots and the resolved Unlit dependency in the product header. Components collect explicit references only; old products require recooking for packaging. |
| Default material was not project configurable. | One reflected material field in configs/editor.json; creation copies the selected UUID without retargeting existing objects. |
| Projects without local cook configuration could not build repository sources. | Project config layers over shipped engine config with explicit malformed-JSON failure. |
| Renderer tests depended on production procedural shortcuts. | Inject private test assets through the existing loader interface. |
| Reused preference paths relied on exceptions. | Approved explicit-result migration retains working-copy, undo and partial-write behavior. |

Cube and ground_plane preserve the old geometry dimensions and winding. Existing box.glb, plane.glb and basic_lit.mat remain for reference stability. Lit uses the existing Lit/Shadow shaders; this is not a new full PBR implementation.

No new target, module entry, directory migration, bespoke settings form, commit or archive.
