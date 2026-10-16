# ui-measurement Specification

## Purpose
Define normalized UI size constraints and consistent bounded measurement across containers, text and editor compositions.

## Requirements
### Requirement: Pure normalized size constraints
UI.Interface SHALL expose pure UISize/UIConstraints min/max values and normalization, deflation, finite-result Constrain, and ResolveMeasureConstraints utilities in engine/ui without plugin/runtime/render dependencies. Positive infinity SHALL be allowed as an unbounded maximum. Negative, NaN, or infinite minima SHALL normalize to zero; negative/NaN maxima SHALL normalize to zero. A minimum greater than its maximum SHALL be lowered rather than increasing the hard maximum. Negative/NaN desired components SHALL become zero before clamping; positive-infinite desired components SHALL become a finite maximum if present or the finite minimum if unbounded. Constrain SHALL always return finite components.

#### Scenario: Contradictory bounds preserve the hard maximum
- **WHEN** an axis has minimum 100 and maximum 20
- **THEN** normalization SHALL lower its minimum to 20 and SHALL NOT increase its maximum

#### Scenario: Invalid constraints are normalized
- **WHEN** constraint minima or maxima contain negative/NaN values, or a minimum is infinite
- **THEN** invalid components SHALL normalize to zero while a positive-infinite maximum remains unbounded

#### Scenario: Infinite desired output is finite
- **WHEN** desired output is positive infinity
- **THEN** Constrain SHALL use the finite maximum when available or the finite minimum when unbounded

#### Scenario: Constraint utility is interface-only
- **WHEN** a consumer includes the size/constraint utilities through UI.Interface
- **THEN** it SHALL use those pure operations without linking UI widget or render implementation

### Requirement: Padding budgets are bounded
Constraint deflation and layout ContentRect SHALL treat negative or non-finite padding as zero. Deflation SHALL reduce normalized available extents without negative or undefined results. Excessive padding SHALL produce an empty content rectangle within the original bounds rather than an inverted or out-of-bounds rectangle.

#### Scenario: Excessive padding remains inside the element
- **WHEN** padding exceeds an element's arranged extent
- **THEN** its content rectangle SHALL be empty and remain within that element's bounds

#### Scenario: Deflating an unbounded maximum stays defined
- **WHEN** a normalized unbounded budget is deflated by sanitized finite padding
- **THEN** its maximum SHALL remain unbounded without producing NaN

### Requirement: Common desired-size query enforces constraints
UIElement::MeasureDesired(constraints) SHALL be a non-virtual entry that normalizes constraints, calls protected OnMeasure, and hard-clamps/sanitizes its returned outer desired size. OnMeasure SHALL include the element's own padding in its outer desired result; the common wrapper SHALL NOT add padding again. The default UIElement::OnMeasure SHALL adapt legacy Measure(float&, float&) in one direction for custom widgets inheriting that hook. Migrated Text/HBox/VBox/StackPanel/DockPanel/GridPanel/Overlay/ScrollView/ListView classes SHALL make their legacy Measure wrapper final and delegate it to unbounded MeasureDesired. Subclasses of those native classes SHALL customize OnMeasure rather than legacy Measure, so supported old/new queries use one derived hook without recursion. This source migration SHALL be explicit; legacy-only native subclass overrides SHALL be rejected at compile time.

#### Scenario: Callback output cannot bypass a tight budget
- **WHEN** an OnMeasure callback returns size 500 under a tight axis budget of 100
- **THEN** MeasureDesired SHALL return 100 on that axis

#### Scenario: Default bridge preserves a legacy custom widget
- **WHEN** a custom UIElement subclass inherits the default OnMeasure and overrides only legacy Measure
- **THEN** the default bridge SHALL use that result as outer desired size and the common entry SHALL enforce constraints

#### Scenario: Migrated native subclass uses the same hook through both entries
- **WHEN** a Text or migrated container subclass overrides OnMeasure and is queried through legacy Measure or MeasureDesired
- **THEN** both entries SHALL invoke the same derived hook, the constrained query SHALL enforce its hard budget, and neither entry SHALL recurse

#### Scenario: Ambiguous legacy native subclass requires migration
- **WHEN** a subclass of a migrated native widget attempts to override only legacy Measure
- **THEN** the final wrapper SHALL cause a compile-time override error rather than silently bypassing the override during constrained measurement

#### Scenario: Native subclass cache follows its existing notification policy
- **WHEN** a retained native subclass notifies a measurement-input change with MarkLayoutDirty
- **THEN** old and constrained queries SHALL observe the new result instead of retaining an earlier cached desired size

### Requirement: Rule-aware measurement distinguishes requests from slots
MeasureForLayout and Layout SHALL use pure rule resolution to make finite known Fixed/Fill/Percent/anchor axes tight before measuring width-dependent content. Ordinary explicit Fixed size SHALL be allowed to exceed parent available size. A parent-owned tight slot SHALL instead remain authoritative through MeasureDesired and final Arrange, without reapplying the child's own layout rule. Fill/Percent on an unbounded reference axis SHALL fall back to finite intrinsic measurement.

#### Scenario: Fixed width reaches the callback before height measurement
- **WHEN** a width-dependent child declares a fixed width of 120
- **THEN** its rule-aware measurement callback SHALL receive a tight width of 120 before computing desired height

#### Scenario: Ordinary Fixed can exceed available size
- **WHEN** ordinary Layout resolves a fixed width of 500 in a parent with available width 200
- **THEN** its fixed request SHALL remain 500 unless a parent-owned hard slot overrides that request

#### Scenario: Final slot overrides the child's own rule
- **WHEN** a container measures a child with a tight width of 100 and arranges that final slot while the child declares Fixed 500
- **THEN** the child's measured allocation and final arranged width SHALL be 100 without another own-rule resolution

#### Scenario: Unbounded Fill and Percent remain finite
- **WHEN** a Fill or Percent rule has no finite reference extent
- **THEN** it SHALL use intrinsic measurement and SHALL NOT return an infinite desired size

### Requirement: Measurement does not mutate layout state
Built-in measurement queries SHALL NOT assign bounds, arrange children, modify scroll offsets or stored arranged extents, create/remove/reorder elements, or mark/clear dirty flags. This requirement SHALL NOT introduce a geometry/measurement cache or invalidation redesign.

#### Scenario: Repeated query preserves state
- **WHEN** an arranged subtree is measured repeatedly under different constraints
- **THEN** its bounds, child membership/order, scroll state, and dirty flags SHALL remain unchanged

### Requirement: Containers propagate available budgets
HBox/VBox/StackPanel/DockPanel/GridPanel/Overlay SHALL propagate constraints through their content areas and include padding once in outer desired size. Linear allocation SHALL reserve non-Fill child extents and spacing before assigning nonnegative remaining Fill space. Slot-owning containers SHALL measure against their slot budgets and call child Arrange on final rectangles without another Layout resolution.

#### Scenario: Fill uses remaining space
- **WHEN** a linear container has preceding Fixed/Auto children, spacing, and a Fill child under finite width
- **THEN** the Fill allocation SHALL use only the remaining nonnegative space and SHALL NOT overlap preceding allocations

#### Scenario: Grid and nested content see cell budgets
- **WHEN** a grid measures nested width-dependent content under finite bounds
- **THEN** children SHALL receive their cell budgets rather than only an unconstrained intrinsic query

### Requirement: Editor custom content uses actual column width
AssetViewer SHALL measure custom content with the actual allocated column width after relevant padding and scrollbar reservation. Its content height and final Arrange rectangle SHALL correspond to that width rather than an unconstrained intrinsic query.

#### Scenario: Scrollbar reservation changes wrapped height
- **WHEN** the viewer's scrollbar reservation narrows width-dependent custom content
- **THEN** the height used for arrangement/scrolling SHALL come from measuring that resulting content width

### Requirement: Explicit logical allocation preserves measurement budgets
UIElement SHALL retain a finite nonnegative logical size assigned only by SetBounds, Arrange, or rule-aware Layout, independent of float positioned endpoint subtraction. Layout/Arrange and their incremental entries SHALL accept an explicit logical parent/allocation size. Rect-only Layout SHALL use actual endpoint differences as its parent reference budget and still resolve the element's own layout rules. Rect-only Arrange and SetBounds SHALL derive their final allocation from endpoint differences rather than an earlier desired size or Fixed request. Native containers SHALL derive content budgets through pure logical padding deflation and preserve child logical allocations before adding their absolute position. Measurement queries SHALL NOT mutate this allocation output.

#### Scenario: Fractional final slot preserves its assigned budget
- **WHEN** a slot owner passes a logical size of width 12.5 to Arrange alongside positioned float bounds
- **THEN** the child's arranged logical size SHALL remain 12.5 even if subtracting those bounds yields a different width
- **AND** its content budget SHALL use pure deflation of 12.5

#### Scenario: Rect-only allocation remains an actual slot
- **WHEN** a caller supplies only a final rectangle to Arrange or SetBounds
- **THEN** logical size SHALL be derived from the rectangle's actual endpoint differences rather than an earlier desired size or a Fixed request

#### Scenario: Identical positioned endpoints retain distinct logical budgets
- **WHEN** two explicit allocations have identical positioned float rectangles but different logical sizes
- **THEN** incremental arrangement SHALL reevaluate the new allocation rather than skipping solely because its rectangle is unchanged

#### Scenario: Incremental layout includes parent logical size
- **WHEN** an unchanged parent rectangle is supplied to incremental Layout with a changed logical parent size
- **THEN** the child SHALL resolve and measure against that changed budget before arranging

#### Scenario: Query cannot replace the current allocation
- **WHEN** an arranged Text or container is measured with an unrelated width budget
- **THEN** the stored arranged logical size, positioned bounds, and subsequent paint budget SHALL remain unchanged

#### Scenario: Native no-argument force reflow preserves its allocation
- **WHEN** DockPanel, GridPanel, or StackPanel's no-argument Layout wrapper forces a new pass
- **THEN** it SHALL preserve the existing positioned bounds and logical allocation size while reevaluating descendants

#### Scenario: Nested containers preserve logical widths
- **WHEN** HBox, VBox, StackPanel, DockPanel, GridPanel, Overlay, ScrollView, ListView, or the default UIElement arranges children at fractional origins
- **THEN** their child width/height budgets SHALL come from the pre-positioning logical allocation rather than subtraction of absolute edges
