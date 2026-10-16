## ADDED Requirements

### Requirement: Viewer model storage outlives interaction cleanup
AssetViewerWidget SHALL cancel live reflected form interactions and detach bindings before replacing owned settings storage. Closing or opening another asset SHALL cancel outstanding live previews while their model remains valid. Viewer destruction SHALL release child form interactions/elements before its owned settings and registered view-data models are destroyed. This contract SHALL NOT imply that existing command-history targets remain valid after replacement.

#### Scenario: Reload replaces heap-backed cook settings
- **WHEN** Reload replaces a settings Any while its form has an active preview
- **THEN** cancellation and detachment SHALL complete before the old settings storage is released

#### Scenario: Close cancels a live preview
- **WHEN** the viewer closes with an active preview
- **THEN** the model SHALL be restored and the form SHALL retain no pending preview rollback

#### Scenario: Viewer destruction with an active preview
- **WHEN** a viewer owning heap-backed reflected data is destroyed during a preview
- **THEN** child forms SHALL clean up while their model/services remain alive and SHALL NOT access already-destroyed model members
