# ui-invalidation Specification

## Purpose
Support explicit incremental measurement and arrangement while preserving complete visible draw output on every paint.

## Requirements
### Requirement: Desired-size and arrangement invalidation propagate upward
MarkLayoutDirty SHALL invalidate the element's and ancestors' measurement entries/generations and remembered Layout parent-input keys, and mark them for arrangement and paint without recursively marking descendants. MarkArrangeDirty SHALL mark the element and ancestors for arrangement/paint without invalidating measurement. Repeated invalidation SHALL remain effective even while the branch is already dirty. Child membership/order and size-affecting setters SHALL use desired-size invalidation; scroll offsets SHALL use arrangement-only invalidation.

#### Scenario: A leaf changes while its AUTO parent is clean
- **WHEN** a controlled leaf changes intrinsic size after a completed incremental layout
- **THEN** its ancestors SHALL be eligible for remeasurement and arrangement and an unrelated unchanged sibling branch SHALL remain reusable

#### Scenario: Dirty state does not suppress a second cache invalidation
- **WHEN** a measurement is cached while a branch is dirty and that branch's size input changes again
- **THEN** a subsequent same-constraint query SHALL recompute rather than reuse the earlier result

#### Scenario: Scrolling preserves desired-size results
- **WHEN** only a controlled ScrollView or ListView offset changes
- **THEN** the next incremental layout SHALL update content placement without discarding otherwise valid desired-size entries

#### Scenario: Explicit old-bounds arrangement cannot revive a stale layout key
- **WHEN** desired size changes, an external host arranges the element at its previous bounds, and LayoutIfNeeded is then requested with the previous parent rectangle
- **THEN** the new desired size SHALL still be resolved instead of skipping with the old Layout parent-input key

### Requirement: Incremental layout has explicit compatible entries
UIElement SHALL provide LayoutIfNeeded(parentRect) and ArrangeIfNeeded(finalRect), and UIContext layout SHALL use the incremental arrangement entry for its stable internal root. A branch MAY be skipped only when its relevant previous input is unchanged, arrangement is clean, and its subtree contains no volatile element. Layout SHALL compare parent-rectangle inputs; Arrange SHALL compare final-rectangle inputs. Existing external Layout/Arrange SHALL remain forced entries; nested container calls within an incremental pass SHALL participate in incremental checks.

#### Scenario: Unchanged controlled tree skips layout work
- **WHEN** a controlled tree completes an incremental pass and the same allocation is requested without mutation
- **THEN** its measurement/arrangement hooks SHALL NOT run again solely because another frame begins

#### Scenario: A changed allocation reaches descendants
- **WHEN** a parent rectangle or parent-owned final rectangle changes
- **THEN** the affected branch SHALL update descendant allocations before return without requiring blanket descendant dirty propagation

#### Scenario: Explicit existing host call remains forced
- **WHEN** an external host calls existing Layout or Arrange twice with the same rectangle
- **THEN** its requested arrangement hooks SHALL execute on both calls

#### Scenario: Stable sibling of volatile content can skip
- **WHEN** a parent executes because one child branch is volatile while another child branch is controlled and unchanged
- **THEN** the unchanged child's nested incremental layout MAY skip independently

### Requirement: Volatile layout protects unnotified dependencies
UIElement SHALL be layout-volatile by default. Controlled native widgets SHALL explicitly opt out through SetLayoutVolatile(false), and opting out SHALL require notification of size/allocation-affecting state. Ancestors SHALL maintain the count of volatile nodes in their live subtree across policy changes, add/remove/clear, and reattachment. A nonzero count SHALL disable branch skipping and measurement reuse for that branch. Text with a nonempty localization key SHALL remain volatile; clearing its key SHALL restore its controlled policy. Legacy/editor polling and externally mutable theme consumers SHALL remain compatible without a global revision redesign.

#### Scenario: Legacy intrinsic size changes without a setter
- **WHEN** a default-volatile custom widget changes externally observed Measure output between incremental frames
- **THEN** the next pass SHALL observe the new output rather than reuse a cached ancestor measurement

#### Scenario: Removing a volatile descendant restores reuse
- **WHEN** the last volatile descendant is detached from an otherwise controlled branch
- **THEN** the old ancestor counts SHALL update and the branch SHALL become eligible for reuse after its invalidated work completes

#### Scenario: Reattachment transfers volatile ancestry
- **WHEN** a detached volatile subtree is attached to another controlled parent
- **THEN** the new ancestor chain SHALL account for its volatile nodes without retaining membership in the old chain

#### Scenario: Localized Text follows external locale changes
- **WHEN** a Text with a localization key is laid out, the active locale/table changes, and incremental layout runs again
- **THEN** its resolved desired size and allocation SHALL follow the new text without a separate Text setter call

### Requirement: Stable measurement queries use a bounded normalized cache
MeasureDesired MAY reuse measurements only for branches containing no volatile node. Its cache SHALL retain at most four results keyed by normalized minimum/maximum width/height and the current measurement generation. Hits and misses SHALL preserve the common constraint normalization/clamping and legacy adaptation contract. The common query framework and built-in measurements SHALL NOT change bounds, child membership/order, scroll offsets/arranged extent, or dirty flags. A dependency invalidation raised defensively by a custom measurement callback SHALL prevent storing a result for an obsolete generation.

#### Scenario: Equivalent normalized constraints reuse a result
- **WHEN** a controlled branch is queried repeatedly with equivalent normalized constraints and unchanged generation
- **THEN** the common entry MAY reuse its constrained desired size without invoking OnMeasure again

#### Scenario: Distinct budgets retain distinct desired sizes
- **WHEN** width-dependent content is queried under different normalized widths
- **THEN** a cached result for one budget SHALL NOT be returned for a different budget

#### Scenario: Query does not consume pending layout
- **WHEN** a dirty controlled element is measured repeatedly and a cache hit occurs
- **THEN** its bounds, tree, scroll state, and pending layout/paint flags SHALL remain unchanged

#### Scenario: Constraint history remains bounded
- **WHEN** more than four distinct normalized budgets are queried
- **THEN** retained measurement entries SHALL remain bounded to four and any recomputed result SHALL retain the common query semantics

### Requirement: Geometry assignment and callback invalidation remain distinct
Arrange SHALL assign its final bounds internally without invalidating ancestor measurement. Public SetBounds SHALL request arrangement/paint as needed. Changed geometry SHALL flow to descendants through actual allocation calls. Arrange SHALL retain any invalidation raised during its child arrangement callback rather than clearing it unconditionally at return. Layout SHALL also preserve invalidation raised during its measurement stage before Arrange begins. Render transforms SHALL invalidate paint without requesting layout.

#### Scenario: Assigning a child slot does not invalidate parent measurement
- **WHEN** a parent arranges a child into its final slot during a stable incremental pass
- **THEN** that geometry assignment SHALL NOT itself discard the parent's desired-size entries

#### Scenario: Callback requests another layout pass
- **WHEN** ArrangeChildren changes the live tree or otherwise raises layout invalidation
- **THEN** the invalidation SHALL remain observable after the current arrangement returns until a later pass consumes it

#### Scenario: Custom measurement invalidates before arrangement starts
- **WHEN** a custom OnMeasure callback raises layout invalidation during Layout before its subsequent Arrange call begins
- **THEN** Layout SHALL preserve that invalidation after returning even if Arrange itself raises no new invalidation

#### Scenario: Virtual rows settle after membership changes
- **WHEN** a ListView arrangement creates or removes live rows
- **THEN** a subsequent pass SHALL be allowed to settle the new branch without losing invalidation or recreating an unchanged visible range

#### Scenario: Transform changes rendered geometry only
- **WHEN** only an element's render transform changes after a completed layout
- **THEN** its absolute layout bounds SHALL remain valid and the next paint/input geometry SHALL use the changed transform without layout invalidation

### Requirement: Native setters notify newly incremental behavior
Controlled native widgets SHALL notify changes that affect their measurement or allocation before subsequent incremental layout. Setting a ListView item factory SHALL discard existing children/live-row metadata and request desired-size/layout invalidation so the next arrangement rebuilds rows through the new factory. Changes that only affect draw appearance SHALL not unnecessarily discard stable measurements. Direct custom/editor UIElement subclasses SHALL retain the conservative volatile default unless they explicitly opt in; a subclass of a controlled native widget SHALL restore volatility if it introduces dependencies without notification.

#### Scenario: A late list factory materializes rows
- **WHEN** an arranged ListView with a nonempty item range receives its item factory after the first layout
- **THEN** the next incremental pass SHALL materialize and arrange the visible rows

#### Scenario: A replacement factory rebuilds live rows
- **WHEN** a ListView with materialized rows receives a different item factory
- **THEN** existing row children SHALL be discarded and the next incremental arrangement SHALL construct the visible range using the replacement factory

#### Scenario: Paint-only property does not request measurement
- **WHEN** a controlled widget changes only its color, opacity, hover, or pressed appearance
- **THEN** its otherwise valid constrained desired-size entries SHALL remain reusable
