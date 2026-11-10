## ADDED Requirements

### Requirement: Contiguous compatible draw batching

UIPaintContext SHALL merge adjacent commands with contiguous indices and identical clip, texture and shape state. Per-vertex geometry, color and SDF parameters SHALL remain unchanged. State changes SHALL preserve draw order and create a separate command.

#### Scenario: Multi-channel curve lines

- **WHEN** 384 visible line segments use the same clip and shape pipeline
- **THEN** they share one indexed draw command with all segment geometry retained

#### Scenario: Render state changes

- **WHEN** clip, texture or shape state changes between primitives
- **THEN** commands remain separate and subsequent primitives are not reordered across that boundary
