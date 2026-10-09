## ADDED Requirements

### Requirement: Cook targets from platform preset bundles

The editor SHALL resolve an asset's cook targets to the active platform's preset product bundles when
the asset declares no cook-target override and the project declares no named targets, and the Cook
action SHALL cook each of them.

#### Scenario: Target-less project
- **WHEN** an asset with no cook override is cooked and the project has no named targets
- **THEN** the cook SHALL run for each product bundle in the active platform preset (e.g. common,
  tex_pc) instead of doing nothing

#### Scenario: Asset override wins
- **WHEN** an asset declares cook targets
- **THEN** the cook SHALL run for the declared targets
