## ADDED Requirements

### Requirement: Physics deformation composes through the pose owner
Completed softbody control snapshots SHALL contribute to the existing animation pose owner through an engine-side interface. Contributions SHALL carry world, instance, skeleton layout and revision identity and SHALL not replace the global skeletal pose source or step physics during render queries.

#### Scenario: Render a physics driven seal
- **WHEN** a compatible completed softbody sample is available
- **THEN** the animation host composes the interpolated deformation with its base pose and publishes one complete final pose
