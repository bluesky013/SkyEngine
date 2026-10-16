## ADDED Requirements

### Requirement: Host-bound actions reject expired owners
EditorShell's asset-viewer action callbacks SHALL remain safe when process registry entries or copied EditorAction records outlive their creating shell. The shell SHALL invalidate its owner-lifetime signal before child/member teardown. Expired-owner enabled predicates SHALL return false and invoke handlers SHALL perform no shell/viewer access. Callback invocation SHALL NOT retain the owner signal in a way that makes an already-destroyed shell appear live during reentrant dispatch.

#### Scenario: Independent viewer consumes actions after shell destruction
- **WHEN** a shell is destroyed and another viewer builds toolbar/menu state from the retained registry entries
- **THEN** enabled predicates SHALL not dereference the expired shell and its actions SHALL be inert

#### Scenario: Copied action outlives its owner
- **WHEN** a consumer retains an EditorAction copy after the creating shell is destroyed
- **THEN** evaluating its enabled predicate or invoking it directly SHALL perform no access to the expired owner

#### Scenario: Older owner dies after action replacement
- **WHEN** another live shell replaces the same scoped action IDs before the former owner is destroyed
- **THEN** former-owner teardown SHALL preserve the newer registrations and old callback copies SHALL become inert independently

#### Scenario: Live shell rebuilds its viewer tree
- **WHEN** a live shell rebuilds its owned viewer/chrome while its actions remain registered
- **THEN** action predicates SHALL observe an absent target during destruction/reconstruction and SHALL NOT dereference the previous viewer
