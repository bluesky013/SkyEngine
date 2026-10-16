## ADDED Requirements

### Requirement: Asset viewer owns an editing-session history
AssetViewerWidget SHALL own and inject a history for its reflected editing session. Opening, reopening, switching asset, closing, and catalog replacement SHALL clear the session history without reverting persisted commits. Viewer scoped actions SHALL traverse and report enabled state from that history rather than global editor history. The history and borrowed catalog service SHALL outlive all forms/commands that use them.

#### Scenario: Viewer edit leaves global history unchanged
- **WHEN** a reflected cook-settings form commits an asset edit
- **THEN** viewer history SHALL gain the command and global editor history SHALL remain unchanged

#### Scenario: Close and asset switch end history ownership
- **WHEN** the viewer closes or opens another asset
- **THEN** old commands SHALL be released and later viewer Undo SHALL NOT edit the previous asset

### Requirement: Cook-setting history restores persisted target state
Cook-setting commands SHALL own asset UUID, target identity, reflected schema identity, and before/after target sparse settings. They SHALL NOT retain viewer/field/settings storage targets. Do, Undo, and Redo SHALL use one current-identity persistence path that updates the manifest and catalog notifications while preserving other targets and unrelated cook-block data. Normal settings reload SHALL preserve valid session history. Save/Cook after Undo SHALL observe the restored persisted state.

#### Scenario: Undo after heap-backed settings reload
- **WHEN** a committed edit caused the previous heap-backed settings Any to be destroyed and the viewer invokes Undo
- **THEN** Undo SHALL restore persisted target settings and refreshed form values without accessing that Any

#### Scenario: Another target changes between edit and undo
- **WHEN** another target receives an independent persisted update before Undo of the original target
- **THEN** Undo SHALL restore the original target and preserve the other target's update

#### Scenario: External traversal uses the same persistence
- **WHEN** a caller traverses the injected viewer command service directly
- **THEN** persistence SHALL update synchronously and the viewer SHALL reload before its next layout

### Requirement: Stale asset history cannot write through obsolete targets
Command construction SHALL reject missing/read-only assets, missing properties/targets, incompatible values, and replaced schemas. Execution SHALL re-resolve current asset/target/schema identity. A missing asset, removed target, or replaced schema SHALL make the stale operation inert without accessing old model memory. The generic history cursor MAY advance for an inert command; the viewer SHALL clear its history when reload observes invalid targets/models/schema.

#### Scenario: Asset disappears with populated history
- **WHEN** the edited asset is removed before Undo or the next viewer synchronization
- **THEN** no stale model or missing source SHALL be written and viewer reload SHALL clear obsolete history

#### Scenario: Settings schema or target changes
- **WHEN** a command is traversed after its target is removed or reflected settings type identity changes
- **THEN** the command SHALL leave persistence unchanged and SHALL NOT use retained schema/model pointers

### Requirement: Viewer lifecycle work waits for a committing widget event
AssetViewerWidget SHALL defer Open, Close, catalog replacement and model/tree reload requested synchronously while any owned reflected form is editing. Nested synchronization SHALL NOT destroy or replace that form. Pending work SHALL execute at the next safe host layout boundary after the widget event returns; it SHALL NOT execute from edit-scope cleanup. A replacement session SHALL contain no command from a cleared previous session.

#### Scenario: Observer switches assets during inline commit
- **WHEN** a catalog observer requests Open for another asset and synchronizes while a widget is committing
- **THEN** the old widget/form SHALL remain alive through event return and the next layout SHALL open a fresh session for the requested asset

#### Scenario: Observer replaces targets or closes during commit
- **WHEN** a catalog observer changes the visible target set or requests Close and synchronizes during a widget commit
- **THEN** target-form destruction or close SHALL wait until the post-event layout boundary and invalidated history SHALL be cleared safely

#### Scenario: Observer replaces the session during Undo
- **WHEN** a synchronous undo observer opens another asset and clears the previous session
- **THEN** the completed old operation SHALL NOT populate Undo or Redo in the new session
