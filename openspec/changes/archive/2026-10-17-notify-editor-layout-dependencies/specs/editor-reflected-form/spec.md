## ADDED Requirements

### Requirement: Form preferred extent is valid before painting
ReflectedFormView SHALL calculate natural content/preferred height from its current sections, expanded fields, widget row counts, header visibility, and theme metrics without requiring a previous Paint. Bind, Refresh, expansion/collapse, sequence membership, header visibility, and widget registration/replacement SHALL invalidate layout so a host can allocate the current extent before painting. A per-form registry change SHALL NOT require rebuilding fields or ending an active interaction solely to invalidate layout. Arbitrary state changes inside an already-registered custom widget SHALL require explicit Refresh or conservative volatility. Measurement/height queries SHALL NOT use the previously painted content extent as their sole source of truth.

#### Scenario: First allocation follows binding
- **WHEN** a host binds a nonempty reflected form and asks for preferred height before any Paint
- **THEN** the returned height SHALL fit the current visible rows and header

#### Scenario: Structural change affects the next allocation
- **WHEN** an expanded field, sequence membership, header visibility, or registered widget implementation changes
- **THEN** the form SHALL notify layout and the next pre-paint height query SHALL reflect the changed extent

#### Scenario: Widget replacement preserves an active interaction
- **WHEN** a form replaces a widget registration while a previous widget owns an active interaction
- **THEN** layout SHALL be invalidated to reflect the new row count without destroying the active interaction's retained owner

### Requirement: Form external dependencies synchronize safely before layout
A form SHALL observe its injected PropertyChangeNotifier and CommandService changes, invalidate layout, and retain pending refresh until a safe public pre-layout synchronization step can consume it. Notification callbacks SHALL NOT synchronously rebuild active field/widget interaction state. Pending refresh SHALL survive an active interaction and SHALL be consumed when safe even if no later notification occurs. A destroyed view SHALL remove its subscriptions. Explicit Refresh remains required for arbitrary unnotified external model mutation.

#### Scenario: Property notification changes row structure
- **WHEN** bound data changes externally, its notifier publishes the change, and the host synchronizes before allocation
- **THEN** the form's rows and preferred extent SHALL reflect the current data in that allocation pass without a preliminary Paint

#### Scenario: Undo occurs in another host
- **WHEN** another host sharing the injected command service performs undo or redo and the form synchronizes before layout
- **THEN** the form SHALL refresh to the current bound data before its host allocates rows

#### Scenario: Pending refresh outlives an interaction
- **WHEN** an external change is notified while a text edit, drag, or popup interaction is active
- **THEN** the form SHALL retain safe active interaction state and later consume the pending refresh once safe without requiring another notification

#### Scenario: Destroyed view receives no service callback
- **WHEN** a form is destroyed and its former notifier or command service later changes
- **THEN** no remaining subscription SHALL call the destroyed view
