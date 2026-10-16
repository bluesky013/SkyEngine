## Context

T11-A/B/C already implement Preferences working sessions/history, the Preferences container tree, and portable checked persistence/results/application. Their evidence remains in the preceding changes. The running host still had three-page archive persistence, no generic storage injection, and incomplete UI-scale application. Generic ReflectedForm ordering ignored CommonPropertyKey::ORDER, and the original UI-11 request to inventory remaining screens was not a bounded implementation list.

The user authorizes one T11 closeout batch. The inventory is now frozen to four existing views and their outer regions/control slots. This change finishes code and portable integration evidence without treating unrun Windows/native display/GPU gates as passing or expanding into the concurrent renderer refactor.

## Scope and Exception to the Temporary Exclusion

**Included:** EditorCore ORDER handling, FilePreferenceStorage, owned built-in registration, supported settings policy, EditorShell DPI/user-scale inputs and the named four-screen composition, plus settings-only SandboxModule calls/service lifetime declarations and the existing GUI callback's target-surface dispatch needed for scaling.

The narrow SandboxModule exception permits only settings registration, generic preference file Load/service injection, UI-scale application and necessary settings-service construction/destruction/header members. It also permits retaining the existing GUI callback's supplied surfaceId through PaintGui into EditorShell::PaintSurface, so a floating window reaches its own scale/layout. Those statements may reside within startup/hub/editor functions that also contain renderer code, but surrounding renderer initialization/tick/surface-creation/refactor statements and renderer interfaces/resources remain protected. This is not permission to migrate native renderer/window APIs or repair their intermediate build contracts.

**Excluded:** RHI/RDG/pipeline/backend/Cocoa/Core Event changes; renderer/adaptor/texture/frame/submission contracts; unrelated editor views; general settings feature implementation for values with no consumer; commit/archive without confirmation. Windows/native/display/DPI/GPU execution remains separate acceptance.

## Decisions

### Finish ORDER through existing metadata before building fields

Effective order is explicit MemberAppearance.order when hasOrder is set, otherwise reflected CommonPropertyKey::ORDER when present, otherwise zero. Stable sorting preserves existing reflection iteration for equal effective order. TypeNode::members is currently a std::map, so that fallback is dictionary/key order, not source registration order. No new Framework registration-order data/ABI is introduced.

Apply this rule to root fields and recursively reflected struct members before constructing PropertyField/owned inline Any trees. Do not sort already-built fields, which could relocate inline storage and break child descriptors. Existing appearance labels/visibility and generic reflection controls remain in effect.

### Use a real adapter for the existing storage contract

FilePreferenceStorage implements IPreferenceStorage over an explicit filesystem path. Read returns NotFound for absence and distinct failure for an unreadable/invalid path. Write creates an exclusive temporary file in the destination directory, completely writes/closes it, and replaces the destination through the platform's file replacement mechanism only after success. Failed writes clean their temporary file and preserve previous destination bytes.

This adapter supplies the prior portable failed-Write contract; it does not promise cross-process compare-and-swap or power-loss durability. JSON/schema validation and unknown-key merging remain PreferencesPersistence responsibilities. File tests use temporary directories, including byte-for-byte failure preservation and replacement/cleanup cases; no native GPU is needed. Windows-specific execution remains unverified until its platform gate runs.

### Own only built-in registrations and load after contributors register

BuiltinPreferencePages owns the exact registration ids it adds, rather than clearing the shared registry. Bind validates reserved built-in ids against unrelated registrations before replacing its own pages; conflicts return a structured failure. Reset/destruction removes only its own registrations through exact registration identity. Plugin pages and newer replacement registrations remain intact.

The host registers built-ins and contributors, then uses generic PreferencesPersistence::Load at the controlled no-open-session startup boundary. The shared file service is injected into EditorShell/PreferencesDialog; the old void Save-only hook is not used as the checked persistence gate. Existing pageId/classId/elements files continue to load, and saved unknown/temporarily absent plugin data remains under the existing merge contract. The hub also loads available preferences before using its scale. Late page contributions still obey explicit host load/registration boundaries; this is not a DLL-unload or automatic runtime rebasing system.

Settings/service lifetime declarations keep borrowed settings and registry metadata valid for their owned registrations and in-flight calls. Rebuild/reset must not leave duplicate built-in ids, erase plugin contributors, or remove newer unrelated registrations at old-owner destruction. The existing session result/partial-effects/history protocol remains unchanged.

### Separate device DPI from user preference everywhere

EditorShell::SetUiScale takes the main device DPI input, SetSurfaceScale the floating device input, and SetUserUiScale the user preference. GetSurfaceUiScale applies the same normalized device × user calculation for main and floating windows, clamping the effective result to 0.5–4. The registered user range is 0.5–2. Invalid/nonfinite inputs use the defined normalization policy rather than creating nonfinite theme geometry. The existing developer environment override remains a device-input override.

Changing the user value applies through the editor page callback immediately to Shell/Theme and recomputes each surface's effective scale at its next layout/paint boundary; a floating device override must not bypass the user multiplier. Do not multiply an already-effective main scale a second time. Startup Load, main-window calls, hub theme setup and later Apply use the same policy. Existing shared theme revision invalidation still refreshes the affected tree; this does not redesign per-surface theme ownership.

The host presentation callback forwards the actual surfaceId instead of painting the main surface for every window. Shell pointer/key/text entry points select the target surface's effective theme and prepare its layout before routing, with existing weak owner/attachment guards around callbacks. A main-window modal must keep the main theme even when a different-DPI floating window painted most recently. Arbitrary const geometry queries do not infer a surface they were never given; presentation/input hosts establish that context before observation.

### Describe stored-only settings honestly

All built-in settings remain reflected, validated where appropriate, editable and saved. Only editor.uiScale has a runtime consumer connected in this batch. General page language/theme/autosave, Editor gridSize/snap/undoDepth and Rendering vsync/resolutionScale are stored-only: there is no current consumer to apply them either immediately or on restart. Mark General/Rendering page titles and relevant Editor labels/tooltips accordingly; do not invent restart support or route rendering values into excluded backend/refactor code.

| Setting | This batch's actual application |
| --- | --- |
| editor.uiScale | Immediate Shell/Theme user multiplier for main/floating; hub/startup reads the persisted value |
| general.language, general.theme, general.autosave | Stored only; future feature consumers remain separate |
| editor.gridSize, editor.snap, editor.undoDepth | Stored only; future editing/history consumers remain separate |
| rendering.vsync, rendering.resolutionScale | Stored only; renderer consumers wait for the coordinated refactor scope |

### Freeze four outer compositions, not every editor screen

| View | Inventory before this batch | Frozen migration |
| --- | --- | --- |
| FileBrowserDialog | Independent sidebar/toolbar/list-header/list/footer/filter/name/button rectangles; fully self-drawn host | Shared containers allocate outer/sidebar/navigation/list/footer and control slots; compatibility queries use arranged slots. Specialized file rows, popup/context painting, FileBrowserModel/navigation/rename/create/filter behavior stay local. |
| NewWorldDialog | Name/location and Create/Cancel all hand-positioned; no composed arrangement | Shared container panel/header/body field rows/footer/control slots. Preserve TextEditState, validation, path construction, keyboard and create/cancel callbacks. |
| ProjectManagerView | Banner/top actions/selection actions/header/status/list hand-positioned | Shared Dock/Grid allocation for those outer regions/control slots with bounded responsive action groups; queries use arranged nodes. Preserve recent-project model/selection/open/add/new/remove/confirmed-delete behavior and specialized row contents. |
| WorldConfigPanel | Existing parent Arrange plus stable reflected form, but manual subsystem column/row/form placement | Shared container header/list/form allocations. Preserve generic reflected fields, explicit document/refresh/provider compatibility, selection and enable/rebuild/save behavior; specialized row/checkbox glyph painting remains local. |

Slots, margins, gaps and extents come from UiTheme and are clamped before feeding containers. Hosts pass explicit unpositioned logical allocations rather than reconstructing budgets from rounded absolute endpoints. Existing public geometry queries prepare/read the same arranged tree used by paint/input, reject invalid indices and check visible viewport bounds. Small/degenerate regions stay finite/nonnegative; long labels are clipped within allocated leaves. Paint consumes prepared geometry rather than constructing layout. No promise is made that every editor view now uses containers, and specialized per-list row/glyph coordinates are not a second dialog-level layout engine.

The Project Manager hub retains conservative host volatility because its UIContext can observe the global theme without EditorShell's pre-layout hook; controlled child containers can still reuse unchanged allocation. Theme-dependent slots and geometry queries synchronize before observation. No native hub/render tick rewrite is needed for that compatibility boundary.

## Validation and Completion Names

Use two explicit outcomes:

- **T11 implementation closeout:** ORDER semantics, real adapter, generic host wiring, plugin preservation, UI-scale inputs and the frozen four compositions are implemented, with actual portable/file/ASan/spec/scope evidence.
- **T11 platform acceptance:** Windows/native module execution and real display/cross-DPI/GPU observations are recorded on suitable hosts. Portable CPU evidence does not close this outcome.

Verify ORDER precedence/ties/nested structs without relocating built fields; file round trips/replacement/read/write failures; built-in owner reset/rebind/reserved-id conflicts and plugin survival; generic Load of legacy JsonOutputArchive bytes and real-file injected Save; immediate scale multiplication across main/floating, target-surface pointer/key/text/modal routing and normalized boundary values; stored-only presentation; and each view's first layout/query, resize, theme change, clipped hits and preserved business callbacks. Retain all preceding session/history/partial-effect/owner-lifetime regressions.

Root runs targeted portable normal/ASan suites and bounded settings-source integration checks without starting an excluded full/native renderer build. Record exact executed tests, source-level wiring checks and unrun module/platform checks separately. Existing changes/evidence remain intact. No commit or archive occurs without separate explicit approval.
