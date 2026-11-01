> 2026-11-01 整理：**partially-superseded**。现行 owner：共享 CPU 图片目录契约仍有效；GPU/浮窗验收归 `aurora-renderer-platform-parity` P02-EDITOR。
> 剩余：3.5 保留未验收；旧 inflight=1/BeginFrame GPU 段不得原样同步，先保留 CPU 契约再按 owned-frame 机制整合。下方较早状态与验收记录保留为历史。

## Why

Floating editor surfaces receive glyph atlas pages, but SVG icons and the color picker wheel upload
only to the default renderer. Their handles therefore resolve to the white fallback in other windows.
Mirroring late updates/removals directly would also expose existing immediate staging writes and
resource erasure to submitted GPU work.

## What Changes

- Add a backend-independent CPU shared-image catalog and a separate UITextSystem shared-registry
  entry point, preserving GetRegistry's borrowed default-backend contract.
- Replay existing shared images when a surface attaches, mirror later additions/updates/removal,
  and detach without calling a destroyed or closing backend.
- Reserve disjoint automatic shared and explicit shared id ranges; keep viewport images local and
  existing atlas registration independent.
- Route editor SVG icons, the immutable color wheel, and the reflection demo's image through the
  shared entry point. Preserve repeated wheel-registration recovery after a backend texture reset.
- Queue renderer CPU updates, create a fresh GPU generation on upload, and retain removed/replaced
  generations until the host's completed-frame safe point under the current inflight=1 policy.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `ui`: UITextSystem owns shared CPU images and attaches/detaches their backend registries.
  deferred texture generation and retirement preserve submitted GPU resource lifetime.

## Impact

UI core catalog/text facade, Aurora UI adaptor, EditorRenderer's existing fence-wait integration,
and editor image upload sites. CPU tests exercise replay/update/remove/reset/isolation and detached
registry destruction. Actual floating toolbar/color-wheel display and GPU validation remain required
platform checks; CPU tests do not prove native presentation. No RDG or multi-inflight redesign is
included, and commit/archive still require user confirmation.
