## ADDED Requirements

### Requirement: Preferences uses shared container composition
Preferences SHALL compose category navigation, reflected page content, and footer actions through shared UI containers using UiTheme dimensions and explicit logical allocation sizes. The selected page SHALL remain a generic ReflectedFormView bound to the existing session-owned working page and local history. Layout SHALL finish before paint/input. Compatibility rectangle queries, paint, clipping, and hit-testing SHALL consume container-arranged geometry rather than independent per-control placement formulas. Painting SHALL NOT create or repair the layout tree.

#### Scenario: First layout precedes paint
- **WHEN** Preferences opens and is arranged before its first Paint
- **THEN** category rows, page viewport and footer actions SHALL already have their final bounds and logical allocations for paint and input

#### Scenario: Query and input share the arranged tree
- **WHEN** a caller queries a category or action rectangle after layout and sends pointer input within its visible bounds
- **THEN** hit-testing SHALL target the same arranged element that painting uses
- **AND** invalid indices and points outside the relevant clipped region SHALL NOT resolve to an active category/action

#### Scenario: Fractional origin retains logical allocation
- **WHEN** the dialog is arranged at a fractional origin or scaled size
- **THEN** nested measurement and arrangement SHALL use their explicit logical sizes without deriving a contradictory budget from rounded endpoint subtraction

#### Scenario: Paint consumes a prepared composition
- **WHEN** page registration changes after layout and Paint runs before the next explicit preparation boundary
- **THEN** Paint SHALL consume the prepared tree without synchronizing registrations or creating/rearranging category/action elements
- **AND** the next layout, input entry, explicit SynchronizeLayout or compatibility geometry query SHALL prepare the changed composition

### Requirement: Category and reflected page scroll independently
The category list SHALL have a bounded vertical scrolling viewport independent of the selected reflected page's scroll state. Scrolling SHALL route by the visible hovered region, while active reflected popup/capture interactions retain their established routing priority. Category/content scrolling SHALL NOT move footer actions. Offscreen or clipped category rows SHALL NOT be selectable through another region, and scroll offsets SHALL remain clamped after page/extent/viewport changes.

#### Scenario: Select a category beyond the initial viewport
- **WHEN** contributed pages overflow the category viewport and the user scrolls to a later visible row
- **THEN** that row SHALL be selectable and bind its session page through the existing generic form
- **AND** rows outside the viewport SHALL NOT receive selection input

#### Scenario: Wheel stays in the hovered region
- **WHEN** the category list and selected form both overflow and wheel input occurs over one of them
- **THEN** only that region's scroll offset SHALL change and the footer SHALL retain its arranged position

#### Scenario: Form popup keeps input priority
- **WHEN** a reflected page owns an active popup or captured interaction extending beyond the page viewport
- **THEN** category/footer routing SHALL preserve its existing delivery and cancellation semantics

#### Scenario: Foreign pointer cannot release form capture
- **WHEN** a form interaction is captured by one pointer and another pointer sends MOVE, UP or cancellation
- **THEN** the unrelated pointer SHALL NOT modify or end the captured form interaction

#### Scenario: Matching cancellation permits fresh capture
- **WHEN** the owning pointer is cancelled during a form interaction
- **THEN** the dialog SHALL clear its capture and forward cancellation safely to the form before a later pointer starts a new interaction

### Requirement: Category scrollbar stays within its reserved track
The category scrollbar SHALL paint through UiSkin using its actual reserved track rectangle with the viewport height. Its theme-sized thumb SHALL remain inside that rectangle at fractional, ordinary and increased scale and for small valid tracks. Content that fits the viewport SHALL draw no thumb. This painter SHALL NOT create a second layout or scroll owner.

#### Scenario: Reserved track changes with scale
- **WHEN** a reserved category scrollbar track is painted at 0.5x, 1x or 2x theme scale
- **THEN** emitted thumb geometry SHALL remain within the supplied track and represent the clamped scroll offset

#### Scenario: Track is tiny or content fits
- **WHEN** the reserved track is smaller than the normal themed thumb or the content does not overflow
- **THEN** thumb geometry SHALL be bounded to the valid track or omitted when content fits, without negative/inverted draw bounds

### Requirement: Preferences responds to available bounds and theme
Preferences SHALL derive panel, category, row, footer, action and spacing dimensions from UiTheme and constrain composition to the available surface. Smaller windows SHALL use bounded category/page allocations and an adapted shared-container footer configuration rather than overlapping fixed targets. Long labels SHALL stay within their allocated painted/hit regions. Theme revisions, same-scale metric changes, resize, page reconciliation and reopen SHALL prepare the affected tree before subsequent paint/input. Degenerate bounds MAY yield clipped or empty regions but SHALL NOT produce negative/NaN allocations or active targets outside their visible region.

#### Scenario: Smaller window adapts the layout
- **WHEN** available width cannot fit the normal footer row and nominal category width
- **THEN** container allocation SHALL adapt to the constrained bounds with finite nonnegative regions and nonoverlapping visible footer action targets

#### Scenario: Long category label stays contained
- **WHEN** a contributed category title exceeds its allocated row width
- **THEN** its text SHALL be clipped or ellipsized within that row and SHALL NOT extend the category's hit target or overlap the reflected page

#### Scenario: Scale changes before the next paint
- **WHEN** theme scale or same-scale layout metrics change after the dialog has been constructed
- **THEN** copied container inputs and affected layout state SHALL update before paint using the new theme without requiring dialog reconstruction

### Requirement: Composition preserves Preferences session behavior
Container and row rebuilding SHALL preserve the existing PreferencesSession working objects, local history, page registration reconciliation, Apply/OK/Cancel/Reset behavior, and guarded transition/callback lifetime. The form SHALL remain bound to session-local services and SHALL be unbound/destroyed before those services or working objects are released. Recomposition alone SHALL NOT clear valid session history or apply settings. Generic persistence/application and native module wiring SHALL remain unchanged by this layout migration.

#### Scenario: Cross-page undo survives navigation composition
- **WHEN** the user edits multiple working pages, scrolls/selects categories, or the dialog resizes
- **THEN** Undo/Redo SHALL retain the session's existing cross-page behavior and SHALL NOT consume editor-wide document history

#### Scenario: Apply and Cancel retain their baselines
- **WHEN** the user applies working changes, edits again and then cancels after a layout/page change
- **THEN** the applied live values SHALL remain and the subsequent unapplied working changes SHALL be discarded according to the existing session contract

#### Scenario: Layout transition notifies an external callback
- **WHEN** a safe pending transition or session operation triggers an observer that closes, reopens, replaces or destroys Preferences
- **THEN** the composition code SHALL preserve existing service retention and lifetime/generation checks without accessing released form/row/dialog storage or cancelling a replacement session
