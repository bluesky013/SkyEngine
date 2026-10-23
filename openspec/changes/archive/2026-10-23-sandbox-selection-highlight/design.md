## Context

The current editor selection service drives the outliner, inspector and transform gizmo. Picking publishes Actor identity, while the renderer consumes generation-bound RenderEntityRef values. Aurora's scene pass already owns scene depth; exposing a second scene traversal or redrawing all objects for selection would add unnecessary frame cost.

Reference behavior, checked 2026-10-23:

| Engine | Publicly documented feedback | Adopted behavior |
|---|---|---|
| [UE viewport settings](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/ULevelEditorViewportSettings) | Selection outline and highlight intensity controls | Clear silhouette and subdued occluded feedback |
| [Unity Gizmos menu](https://docs.unity.cn/6000.1/Documentation/Manual/GizmosMenu.html) | Selection Outline and Selection Wire | Outline without changing the scene material |
| [Godot EditorSettings](https://docs.godotengine.org/en/latest/classes/class_editorsettings.html) | Selection box colors, including active selection | Bounds/marker fallback for non-renderable selections |

These references justify interaction choices, not claims about proprietary rendering implementations or comparative performance.

## Goals / Non-Goals

**Goals:** Immediate selection feedback from either viewport or outliner, constant screen-space width, visible/occluded distinction, no additional scene traversal, no runtime dependency on Editor.

**Non-Goals:** Hover picking, GPU picking, object-specific materials, animated/deformed/alpha-tested silhouettes, runtime gameplay selection, new modules or target reorganization, per-object configuration forms.

## Decisions

1. Selection remains in Editor. Sandbox passes owned Actor IDs only while editing. The existing scene bridge maps live actors to generation-bound render identities; deleted/rebound objects cannot inherit stale highlights. No World dirtiness, material changes or undo commands are generated.
2. Add optional generic scene highlight inputs to existing Aurora Pipeline view inputs. Preserve environment/HDR inputs. Generic rendering knows only render identities and colors, never SelectionService or EditorCore. Keep all code in existing targets.
3. Reuse the scene depth. A private pipeline helper renders only selected supported draws into an RGBA8 mask with its own depth; the mask records total and scene-visible coverage. A fullscreen pass expands the union boundary by a bounded pixel radius and alpha blends over the final scene color after tone mapping. No selection means no mask/depth/composite allocations or passes. No CPU mesh decoding or GPU readback is used for the effect.
4. Snapshot surface facts gate static opaque position-preserving geometry. Unsupported materials/custom queries use selection bounds or a pivot marker; proxy visuals carry their owner so they can receive the same theme accent. The query extension alone does not promise a mesh that can be rendered into a silhouette.
5. Theme owns selection colors and width. Default occluded alpha is subdued; zero alpha supports visible-only presentation. Gizmos remain painted above feedback and retain input precedence. Selection rendering itself is not hit-testable.
6. All data and shader resources follow existing packet/module/GPU lifetime contracts. Bound input count and validate finite colors/widths and identities; compile failures use existing explicit renderer errors. Shader declarations remain authoritative C++ headers with persisted exports and registered programs.

## Risks / Trade-offs

- Mask rendering adds selected geometry draws and one fullscreen pass while selected; measure and record native results rather than claim a frame-rate gain.
- Depth comparisons need a small bounded tolerance; edge and near-plane cases require pixel tests.
- Static geometry capability is deliberately conservative. Arbitrary deformations and alpha coverage need material-owned selection techniques in a later change.
- Windows Vulkan and DX12 can be validated here; Metal/mobile native behavior remains unverified.
- Overlay ordering is owned by the picking change; its final review aligns painting and hit priority.

## Migration Plan

Implement optional inputs with an empty default, wire only the editing host, run native readbacks and editor regressions. Removing the editor inputs disables the effect without modifying assets or saved worlds.

## Open Questions

The user confirmed subdued occluded outlines. No pending architecture ownership changes.
