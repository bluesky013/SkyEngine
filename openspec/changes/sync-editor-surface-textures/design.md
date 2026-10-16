## Context

T01 provides paired borrowed atlas registration and T02 defers floating-surface destruction to a
GPU-safe boundary. T07 extends the same surface group to images used by editor widgets. Atlas pages
retain their independent stable-handle path; viewport images remain local to each renderer.

## Goals / Non-Goals

Provide current-image replay on attach, live additions/updates/removal, explicit id ownership,
registry-safe detach, and safe GPU generations. Preserve the existing main/default registry facade.

Do not introduce a device-independent GPU resource manager, RDG migration, multithreaded registry
mutation, or multiple inflight editor frames. Actual visual/GPU verification remains a separate gate.

## Decisions

### Own shared CPU pixels in the UI runtime

UISharedTextureRegistry implements IUITextureRegistry in engine/ui/core and owns UIImageData by id.
It contains only CPU data and borrowed IUITextureRegistry pointers. UITextSystem::GetSharedRegistry
returns this entry point even before a surface exists; GetRegistry continues to return the current
borrowed default backend and clears when that backend detaches. Existing AddRegistry/RemoveRegistry
forward to both atlas and shared-image paths, so one surface cleanup boundary detaches both.

Attach replays each current image via RegisterTextureAs and UpdateTexture. Updating on attach also
refreshes an older image generation retained by a previously detached backend. Duplicate attach is
idempotent. New images, updates, and releases are sent only to attached registries. Detach performs
no backend calls or releases; already delivered resources stay backend-owned until its safe shutdown.
A detached renderer that is reused may retain unused old handles until it is reset/shut down.

All mutation occurs on the host UI/render thread. One catalog owns the shared id domain for one
surface group; attach a backend to only that group. Independent catalogs may allocate the same id
because their backend registries are disjoint. This is explicit object ownership, not a process
singleton or function-static registry.

### Partition handle ownership

| Range | Owner |
| --- | --- |
| 0 | Built-in white/invalid sentinel |
| 1–0x1FFFFFFF | Surface-local renderer images/automatic handles, including the viewport |
| 0x20000000–0x3FFFFFFF | Catalog-generated shared handles |
| 0x40000000 onward | Existing atlas page allocator (reserved below explicit shared resources) |
| 0x80000000–0xFFFFFFFF | Caller-reserved explicit shared resources, currently the wheel at 0x80000000 |

Shared RegisterTexture allocates monotonically within its range and fails on exhaustion. Shared
RegisterTextureAs accepts only the explicit shared range; duplicate registration preserves the
stored pixels. It re-ensures that id on attached backends so reopening a picker after a texture-table
reset restores the wheel. Changes to pixels require UpdateTexture; release/recreate is also explicit.
Updates/releases of unknown, glyph, or viewport ids do not propagate because the catalog owns none
of those entries. Invalid images (zero dimensions or incomplete RGBA8 payload) are rejected.

The existing atlas allocator's enormous-range exhaustion is not redesigned here. Its owner must
not allocate into the explicit shared namespace. CPU editor images use the shared facade; GPU
viewport registration continues to use RegisterImage on the individual renderer.

### Keep GPU writes and releases out of cross-surface callbacks

UIRenderer registration/update only queues CPU pixels; repeated updates coalesce before upload.
UpdateTexture does not map an existing staging buffer or release its resources. EnsureTextureReady
builds a fresh image/staging/group generation with the current dimensions, maps only the new staging,
records upload barriers/copy, and makes the new group available for the next draws. Each new image
starts UNDEFINED, avoiding a false old-layout assumption on an in-place update.

Removed/replaced generations move to retiredTextures. ReleaseTexture removes the lookup immediately
but preserves image, staging, and group referenced by preceding or currently recorded commands.
Replacing RegisterImage also retires its old generation. Failed generation preparation retains the
previous visible group and pending CPU data for a later retry. White fallback remains available for
missing/not-yet-prepared groups.

EditorRenderer calls CollectRetiredTextures on the main renderer and live floating renderers just
after DeviceFrameContext::BeginFrame has waited for the preceding submission, before viewport
acquire/early returns. This is safe because the editor explicitly configures inflight=1. Retirement
created during current recording survives to the following safe point. Collect must not be moved
into EnsureTextureReady or called after arbitrary registry mutations without completed submissions.
Shutdown additionally relies on T02's device idle wait. Increasing inflight requires per-generation
fence ownership and is outside this repair.

Fresh generations trade some allocation churn for simple lifetime correctness. Updates coalesce
once per renderer upload and immutable icons/wheel allocate only once; texture-generation reuse
should be profiled before a later optimization adds in-place update synchronization.

### Route editor images through the shared facade

SandboxModule's SVG icon registration, ColorPicker's immutable wheel, and ReflectionDemoPanel's
owned icon use GetSharedRegistry. A floating surface attached later receives existing resources;
opening the first picker in any window mirrors its wheel to every attached surface. Closing a
picker does not release the immutable shared wheel. The demo's owned icon release propagates to
all live surfaces and the adaptor retires its GPU generations safely.

## Risks / Trade-offs

- Borrowed registry pointers still require paired detach before destruction. T01/T02 supply that
  boundary; the catalog deliberately never invokes registries from its destructor.
- CPU pixels are retained for replay, increasing memory by one copy per shared image. Individual
  renderers still own their GPU resources and transient upload copies.
- Renderer reset while attached must preserve/restablish bindings. Explicit wheel registration
  re-ensures presence; general surface reconstruction should detach and reattach for a full replay.
- Native GPU/presentation coverage is unavailable to pure CPU tests; leave the visual gate open
  until toolbar icons, wheel, late changes, removal, and close/reopen are checked with GPU validation.

## Validation Strategy

- Fake registries assert initial replay, later image registration, resized updates, and current-pixel
  replay into a newly attached or reattached backend.
- Assert explicit-id policy, duplicate immutability, reset/re-ensure, idempotent removal, and explicit
  recreation. Assert viewport/glyph ids and unrelated surface groups remain isolated.
- Detach/destroy a secondary registry, then mutate shared images and grow atlas pages under ASan.
- Preserve the existing real ColorPicker reset/reopen regression through the shared facade.
- Root integration builds UI/UIRender/editor consumers, runs relevant runtime/text/widget tests,
  validates OpenSpec strictly, and records real GPU/visual checks separately.

## Full texture-id binding state

The explicit shared-id range includes UINT32_MAX. UIRenderer tracks whether a texture has been bound with a separate boolean, so this valid id cannot collide with an unbound sentinel on the first draw. CPU catalog tests exercise registration/replay at that boundary; native draw validation remains part of the GPU gate.
