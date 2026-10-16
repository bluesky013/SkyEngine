## ADDED Requirements

### Requirement: Reflected order metadata participates before field construction
Effective form member order SHALL use explicit MemberAppearance.order when hasOrder is set, otherwise reflected CommonPropertyKey::ORDER when present, otherwise zero. Stable sorting SHALL preserve existing reflection iteration for ties; the current std::map fallback is key order rather than source registration order. The rule SHALL apply before constructing root and nested struct fields so inline owned-value/child-descriptor storage is not relocated by sorting built fields. This correction SHALL NOT require new Framework registration-order ABI or per-settings-page layout code.

#### Scenario: Explicit appearance overrides reflection
- **WHEN** a member declares reflected ORDER and an explicit appearance order
- **THEN** the form SHALL use the explicit appearance value, including an explicit zero

#### Scenario: Reflected order and stable ties
- **WHEN** reflected members have ORDER values or no explicit order and some effective values tie
- **THEN** fields SHALL follow effective order with stable existing reflection iteration for the tied members

#### Scenario: Nested fields retain valid bindings
- **WHEN** nested reflected members are ordered and later edited/reset/undone
- **THEN** their descriptors SHALL remain bound to valid owned storage rather than addresses invalidated by sorting already-built fields
