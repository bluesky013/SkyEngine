## ADDED Requirements

### Requirement: Listener represents the audio viewpoint

The audio API SHALL provide an `AudioListener` whose position and orientation are updated each frame from a world
transform (typically the active camera), and the backend SHALL spatialize 3D sources relative to that listener.

#### Scenario: Listener updates from camera transform

- **WHEN** the active camera transform changes during a frame
- **THEN** the listener position and orientation SHALL be updated before 3D sources are mixed

#### Scenario: No listener produces no spatialization error

- **WHEN** a `World` has 3D sources but no listener attached
- **THEN** playback SHALL continue without crashing and 3D sources SHALL be treated as non-spatialized

### Requirement: Per-source spatial blend

Each `AudioSource` SHALL expose a spatial blend in the range `[0, 1]`, where 0 is fully 2D (no spatialization) and
1 is fully 3D (attenuation, panning, and doppler apply).

#### Scenario: 2D source is unaffected by listener

- **WHEN** a source has spatial blend 0
- **THEN** its output level SHALL NOT depend on the listener position

#### Scenario: 3D source pans with listener

- **WHEN** a source has spatial blend 1 and the listener moves to one side of it
- **THEN** the source SHALL pan toward the listener and its level SHALL follow the attenuation curve

### Requirement: Distance attenuation

A 3D `AudioSource` SHALL expose a minimum distance, a maximum distance, and an attenuation curve, and the backend
SHALL apply the resulting gain based on listener-source distance.

#### Scenario: Inside minimum distance is full volume

- **WHEN** the listener is within the source's minimum distance
- **THEN** the source SHALL play at its full configured volume

#### Scenario: Beyond maximum distance is silent

- **WHEN** the listener is beyond the source's maximum distance
- **THEN** the source SHALL be inaudible

#### Scenario: Curve controls falloff between the bounds

- **WHEN** the listener is between the minimum and maximum distance
- **THEN** the gain SHALL follow the configured attenuation curve

### Requirement: Doppler effect

A 3D `AudioSource` SHALL support an optional doppler factor that shifts pitch based on relative velocity between
the source and the listener.

#### Scenario: Doppler disabled by default

- **WHEN** a source is created without enabling doppler
- **THEN** its pitch SHALL NOT change due to relative motion

#### Scenario: Approaching source rises in pitch

- **WHEN** doppler is enabled and a source moves toward a stationary listener
- **THEN** the perceived pitch SHALL rise relative to a stationary source
