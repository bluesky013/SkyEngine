## 1. Contracts and inputs
- [x] 1.1 Add bounded generic highlight inputs and preserve resolved render identity; validate malformed inputs.
- [x] 1.2 Map live selected Actors through the existing bridge and gate supported surface facts without another World traversal.

## 2. Rendering and editor integration
- [x] 2.1 Add registered shared mask/outline shaders and compose selected-only masks with existing scene depth after tone mapping.
- [x] 2.2 Connect Editor selection/theme/play/document lifecycle and bounds/marker/proxy fallback feedback.

## 3. Validation
- [x] 3.1 Test empty/multiple/stale selection, styles, surface eligibility and editor fallback without world or undo mutations.
- [x] 3.2 Build affected targets and verify native Vulkan dynamic/render-pass and DX12 pixel readbacks, including occlusion and clearing.
- [x] 3.3 Review cost/lifetime/dependency boundaries, validate shader exports and OpenSpec, and record actual coverage and limits.
