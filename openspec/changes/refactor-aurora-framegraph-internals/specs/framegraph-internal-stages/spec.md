## ADDED Requirements

### Requirement: FrameGraph internal stages preserve declaration and lifetime semantics
FrameGraph analysis SHALL separate declaration validation, pass access collection, culling and content/lifetime validation without changing public graph declarations or native backend ownership.

#### Scenario: Invalid declarations belong to a culled pass
- **WHEN** a pass contains an invalid declaration and has no surviving output
- **THEN** analysis rejects the declaration before culling with the existing error classification

#### Scenario: An overwritten producer reads undefined contents
- **WHEN** a producer is fully overwritten and its pass is culled
- **THEN** content validation does not require the culled pass's inputs to be defined

#### Scenario: Binding declarations are out of binding-number order
- **WHEN** compute or custom bindings are declared in a non-sorted order
- **THEN** Custom callback arrays retain declaration order and native dynamic offsets retain binding order

#### Scenario: Native compilation fails after allocations
- **WHEN** a backend fails while preparing a pass
- **THEN** its existing lease rollback and error precedence apply, and submitted or borrowed resources remain retained until safe retirement
