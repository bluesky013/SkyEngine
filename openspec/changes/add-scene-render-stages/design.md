## Context

`SceneRenderFeature` currently builds the shadow pass, the opaque scene pass, optional HDR color, tonemap and selection in one `BuildView` callback (`engine/aurora/pipeline/src/SceneRenderFeature.cpp:281-390`). The opaque color and depth are transient graph images created inside that callback and are not reachable from other features.

Transparent effects need three things that are unavailable today:

- Execution after the opaque pass.
- Execution before tonemap.
- Read access to scene color and depth.

## Goals / Non-Goals

**Goals:**

- Provide one ordered extension point after opaque and before tonemap.
- Share the scene color and depth handles with the extension without exporting them into a global namespace.
- Keep extension lifetime and retirement coordinator-owned.
- Preserve existing scene behavior when no extension is registered.

**Non-Goals:**

- No general-purpose render graph reordering.
- No OIT or per-pixel transparency sorting framework.
- No public export of scene internals to arbitrary features.
- No particle implementation in this change.

## Decisions

### Ordered scene extension, not feature splitting

Add `ISceneRenderExtension` to the renderer feature contract:

```cpp
struct SceneRenderStageContext {
    const PreparedViewData &view;
    GraphImageHandle        color;
    GraphImageHandle        depth;   // valid when the extension requires depth
    Extent2D                extent;
};

class ISceneRenderExtension {
public:
    virtual ~ISceneRenderExtension() = default;
    virtual Name GetName() const = 0;
    virtual uint32_t GetOrder() const { return 0; }
    virtual bool RequiresDepth() const { return true; }
    virtual std::optional<RendererError> Validate(const FrameExtensionPayload &) const = 0;
    virtual FrameOperationResult Prepare(FrameFeaturePrepareContext &) { return FrameOperationComplete{}; }
    virtual FrameOperationResult Build(FrameGraphBuildContext &,
                                       const SceneRenderStageContext &,
                                       const FrameExtensionPayload &) = 0;
    virtual void OnRetired() {}
};
```

Rationale:

- The extension runs inside the scene feature's `BuildView`, so it naturally shares the already-created color/depth handles and cannot accidentally run after tonemap.
- No global image export/import namespace is introduced.
- Splitting the scene feature into separate opaque/postprocess features was considered but rejected for this change: it changes output acquisition, pass ownership and the existing tonemap/highlight contract at the same time.

The scene feature SHALL create a depth image when at least one registered extension requires it. Extensions SHALL use `LoadOp::LOAD` for color and depth and SHALL NOT clear the scene attachments.

### Registration and retirement

Add coordinator-owned commands:

```cpp
struct RegisterSceneExtension {
    RenderFeatureId                    scene;
    RenderFeatureId                    extension;
    Name                               name;
    std::shared_ptr<ISceneRenderExtension> instance;
};
struct RetireSceneExtension {
    RenderFeatureId scene;
    RenderFeatureId extension;
};
```

- `FrameFeatureRegistry` is the sole registration owner. Validation projects every extension command in packet order, including capacity, names and generations; application cannot silently ignore a failed registration.
- Extensions are sorted by `GetOrder()` and then by name when registration state changes. Scene features consume the ordered frame snapshot, without their own mutable registration collection.
- Extension leases pin their parent scene and are retained through preparation, recording and GPU submission. Independent retirement releases the extension after its final borrower; scene retirement also retires its extensions.
- `OnRetired` runs once after the final CPU/GPU borrower retires. Retired capacity becomes reusable while the scene remains active. Payloads route by registered extension name and explicitly selected outputs.

### Host extension sources

Hosts discover providers through the `SceneRenderExtensionSourceRegistry` cross-DLL singleton instead of a single-slot service locator. Each registered source appends its own registrations and payloads for the shared `SceneExtensionSourceContext`; the registry calls sources in registration order and propagates the first error. Adding a second scene-contributing subsystem therefore neither fails module initialization nor silently replaces the first source.

### Execution order

Inside the scene feature `BuildView`:

```text
shadow pass
  -> opaque scene pass
  -> scene extensions (ordered, after opaque)
  -> tonemap
  -> selection / outline
```

Extensions may declare raster and compute work, but must not submit, wait, or retain borrowed context. They may resolve the same prepared draws and resources as other features.

### Failure handling

- An extension build failure follows the existing `FrameOperationResult` protocol; the scene feature propagates the error and does not produce a partial successful frame.
- An extension that requires depth when the scene has no depth format returns an explicit error at validation.
- Extensions that request unsupported attachments or invalid load/store state are rejected before encoding.

## Risks / Trade-offs

- **Extension misuse of shared attachments** -> document LOAD-only and validate clear/load state.
- **Ordering ambiguity** -> deterministic order field plus name tie-break.
- **Lifetime bugs** -> graph retains instances; retire after borrower completion.
- **HDR/MSAA variants** -> pass the actual handles and extent; tests cover HDR and non-HDR paths.
- **Scene feature becomes a coordination point** -> keep the extension interface narrow; no scene internals beyond color/depth/view.

## Migration Plan

1. Add the extension interface and registration commands.
2. Invoke extensions in `SceneRenderFeature` before tonemap.
3. Add lifetime/retirement handling and validation.
4. Add pipeline tests for ordering, color/depth sharing, no-extension behavior and failure propagation.
5. Migrate no existing feature; particles consume this in `add-particle-system`.
