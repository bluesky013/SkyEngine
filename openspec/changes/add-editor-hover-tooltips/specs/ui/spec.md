## ADDED Requirements

### Requirement: Deferred tooltip target protocol

UIElement SHALL provide an optional tooltip query/build protocol without editor dependencies. QueryTooltip SHALL use untransformed absolute layout-surface coordinates and return a stable provider-local target identity, anchor rectangle and weak binding lifetime. BuildTooltip SHALL construct owned display content only for a still-valid queried target. The protocol SHALL NOT load assets, execute commands or modify model state.

#### Scenario: Existing element has no tooltip
- **WHEN** an element does not provide tooltip content
- **THEN** querying it produces no target and preserves its existing input behavior

#### Scenario: Same row object is rebound
- **WHEN** a provider reuses a visible row for another field or asset
- **THEN** the old binding token or target identity becomes invalid and cannot build content for the new row

### Requirement: Tooltip discovery respects geometry and lifetime

Tooltip discovery SHALL respect effective visibility, hit priority, ancestor clipping and accumulated geometry. The host SHALL validate element lifetime, tree attachment and binding lifetime before calling a provider or using its anchor. Hidden, detached, expired or non-invertible targets SHALL NOT produce a tooltip. Popup providers SHALL select their active branch without exposing covered underlying content.

#### Scenario: Scrolled row is clipped
- **WHEN** a tooltip-bearing row exists in a scroll container but is outside its visible clip
- **THEN** a pointer over that region cannot select the clipped row as a tooltip target

#### Scenario: Target is detached during the delay
- **WHEN** a queried provider is removed or its binding expires before display
- **THEN** the pending tooltip is discarded without invoking the stale provider or dereferencing its former model

#### Scenario: A target has transformed geometry
- **WHEN** a visible target is rendered through an accumulated scale or translation
- **THEN** the query receives that target's layout coordinates and its anchor maps to the corresponding surface location
