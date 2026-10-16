## ADDED Requirements

### Requirement: Indexed sequence descriptors preserve element semantics
An indexed PropertyDescriptor representing a sequence element SHALL NOT report itself as the owning sequence container merely because it retains that member's container metadata. Its descriptor type SHALL use the sequence valueType's TypeInfo rather than the owning container type. It SHALL expose the indexed element value and SHALL reject add-sequence-container command construction. Container sequence operations SHALL continue to belong to the owning sequence descriptor. Scalar sequence children SHALL therefore be classified and displayed as their scalar editor kind rather than fictitious nested sequence or Unknown controls.

#### Scenario: Float sequence exposes scalar child rows
- **WHEN** the model builds children for a reflected vector of floats
- **THEN** each indexed child SHALL expose float TypeInfo/value, select the Float editor kind, report IsSequenceElement true and IsSequence false, and produce no add-sequence-container command

#### Scenario: Container edits retain undo behavior
- **WHEN** the form adds or removes an element through its owning sequence control and undo is requested
- **THEN** the container element count and the form's current scalar-row extent SHALL follow the applied or undone operation
