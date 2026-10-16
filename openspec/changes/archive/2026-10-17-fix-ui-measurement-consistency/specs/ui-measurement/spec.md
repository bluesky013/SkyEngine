## MODIFIED Requirements

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

## ADDED Requirements

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
