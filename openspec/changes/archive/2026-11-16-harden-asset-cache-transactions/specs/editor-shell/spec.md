## ADDED Requirements

### Requirement: Versioned tooltip interaction scope
Each surface SHALL supply its interaction root, capture root, layout revision, overlay revision and theme revision. Main surfaces use the router capture; asset viewer surfaces retain their tool capture contract. Modal/menu roots override background content. Stable visible tooltip targets SHALL reuse hit testing and text layout only while input, scope, revisions and weak target identity remain valid. Painting SHALL revalidate occlusion after tree mutations.

#### Scenario: A stationary target becomes covered
- **WHEN** a visible foreground element covers a displayed tooltip target without pointer movement
- **THEN** the next paint hides the stale tooltip before drawing it

#### Scenario: Capture spans leave and reentry
- **WHEN** a held pointer leaves and reenters a surface on a supported platform
- **THEN** hover remains suppressed until buttons are released and a new hover starts

### Requirement: Atomic settings target replacement
Settings requests SHALL carry registry, persistence, title and applied callback together. During reentrant transitions the last whole request SHALL win. Switching or clearing a target SHALL cancel the old applied callback. Teardown SHALL clear retained target references.

#### Scenario: A callback requests another settings target
- **WHEN** a transition callback requests a different settings target
- **THEN** the final dialog uses all fields of the latest request and does not call the replaced target's callback
