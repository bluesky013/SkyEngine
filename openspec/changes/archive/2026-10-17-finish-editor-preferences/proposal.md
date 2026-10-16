## Why

T11-A/B/C deliver Preferences sessions, container composition and a checked portable persistence/application protocol. T11 is still incomplete in the real host: SandboxModule uses its old three-page serializer rather than the generic service, reflected ORDER metadata is not honored, and the remaining-screen review lacks a fixed migration inventory. A single bounded closeout should finish those implementation gaps while preserving the separate Windows/native display/GPU gates.

## What Changes

- Honor generic reflected ORDER metadata with explicit MemberAppearance override precedence and a stable fallback to the existing reflection iteration order; do not expand Framework registration metadata or hand-code page field layouts.
- Supply a real file storage adapter honoring the portable failed-write byte-preservation contract and wire the host's preference Load/service injection/application path to the generic service.
- Preserve contributed preference-page registrations when refreshing built-in pages and retain saved unknown/temporarily absent plugin data through the existing codec contract.
- Apply UI scale through one consistent system-DPI × user-preference policy for main and floating windows, preserving actual surface identity through host GUI dispatch and selecting the target theme before input. Describe other settings' actual supported/application status without inventing renderer/backend behavior.
- Migrate the frozen four-screen inventory's outer regions/control slots: FileBrowserDialog, NewWorldDialog, ProjectManagerView and WorldConfigPanel. Shared containers own those allocations; specialized list rows/glyph painting and existing business operations remain in their owning leaves/models.
- Record implementation and portable integration evidence together, keeping actual Windows/native/display/GPU verification explicit and separate.

## Capabilities

### New Capabilities

None. The closeout composes and finishes existing editor preference, reflection and layout capabilities.

### Modified Capabilities

- `editor-preferences`: generic host persistence/application integration and honest implementation/platform acceptance boundaries.
- `editor-preference-persistence`: native-file adapter behavior under the existing portable storage contract.
- `editor-reflected-form`: reflected ORDER precedence and stable existing-iteration fallback.
- `editor-ui-scaling`: consistent main/floating system-DPI and user-scale application.
- `editor-ui-shell`: the frozen four-screen container composition contract, preserving existing view/model behavior.

## Impact

- EditorCore persistence/reflection, the four named EditorShell compositions, settings-only SandboxModule lifecycle wiring and focused CPU/file/ASan tests. Other editor screens are not silently included in this batch.
- **Narrow scope exception:** settings registration, file storage/load/service injection, UI-scale application and its necessary settings-service lifetime declarations may change in SandboxModule. The existing GUI callback may retain its supplied surfaceId through PaintGui into EditorShell::PaintSurface so the target window receives its scale. Renderer initialization/tick/surface creation, renderer interfaces/resources, RHI/RDG/pipeline/backends, Cocoa/native-window ownership, Core Event and related contracts remain protected.
- Existing session/partial-effect/history, generic field controls and previous change artifacts/evidence remain intact. No new claim that arbitrary settings or native rendering features now apply immediately.
- Windows, real display/DPI/GPU and native integration execution remain separately recorded gates. No commit or OpenSpec archive is authorized.
