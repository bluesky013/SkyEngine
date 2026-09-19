## ADDED Requirements

### Requirement: Easing functions
The animation system SHALL provide easing functions that map a normalized time to an eased value in the range
0 to 1.

#### Scenario: Ease out quad
- **WHEN** EASE_OUT_QUAD is evaluated at 0.5
- **THEN** the result SHALL be 0.75

### Requirement: Keyframe track
A float track SHALL store time/value keys and linearly interpolate between them, clamped at the ends.

#### Scenario: Midpoint interpolation
- **WHEN** a track with keys (0, 0) and (1, 10) is evaluated at 0.5
- **THEN** it SHALL return 5

### Requirement: Time-driven tween
A `UIAnimation` SHALL call its update callback with the eased normalized progress and SHALL report completion;
the context SHALL advance animations with `Tick(delta)` and remove completed ones.

#### Scenario: Advance updates progress
- **WHEN** an animation of duration 1 is advanced by 0.5 with linear easing
- **THEN** the update callback SHALL receive 0.5

#### Scenario: Completion
- **WHEN** an animation is advanced past its duration
- **THEN** its completion callback SHALL run and the context SHALL remove it

#### Scenario: Loop
- **WHEN** a looping animation is advanced past its duration
- **THEN** its time SHALL wrap and it SHALL remain active
