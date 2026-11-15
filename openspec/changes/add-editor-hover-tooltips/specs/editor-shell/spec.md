## ADDED Requirements

### Requirement: Surface-owned delayed tooltip lifecycle

Each editor surface SHALL own an independent TooltipHost. A stable valid target SHALL become eligible after 500 ms of hover, including when the pointer is stationary. A target/binding/root change SHALL restart the delay. Pointer leave/cancel, pointer press, scroll, key/text input, focus reset and surface retirement SHALL dismiss pending and visible tooltips. Pressed or dragging interactions SHALL suppress display; eligible picker popups SHALL NOT be suppressed merely because they retain popup capture. Tooltips SHALL remain non-interactive and SHALL NOT acquire focus or capture.

#### Scenario: Pointer stays over one target
- **WHEN** the pointer remains over a valid target for less than 500 ms
- **THEN** no tooltip is built or displayed
- **AND** after 500 ms without invalidation the host builds and displays its content

#### Scenario: Panel moves to another surface
- **WHEN** a hovered panel is detached and placed in a floating surface
- **THEN** the former surface's candidate is invalidated and the new surface starts its own hover delay

#### Scenario: Input dismisses visible help
- **WHEN** a visible tooltip is followed by a pointer press, scroll, key/text event or input reset
- **THEN** the tooltip is cleared and the event retains its existing dispatch behavior

### Requirement: Tooltip roots follow active editor input routes

EditorShell SHALL select tooltip query roots according to its current input routing for main, modal, floating-panel and independent asset-tool surfaces. An active modal, menu, close-decision prompt or picker SHALL exclude covered lower-priority targets. Preferences and AssetViewer manual routes SHALL expose their eligible reflected content and popup entries through the common protocol. A floating panel blocked by a main modal SHALL NOT continue showing its prior tooltip; independent asset tools SHALL retain their established independent routing.

#### Scenario: Preferences covers a main-window asset row
- **WHEN** Preferences is active above a previously hovered toolbar action
- **THEN** the toolbar tooltip is discarded and only eligible Preferences content is queried

#### Scenario: Asset viewer requires a close decision
- **WHEN** an embedded or standalone asset viewer displays its close-decision prompt
- **THEN** underlying viewer toolbar and field tooltips do not appear through the prompt

#### Scenario: Picker owns popup input
- **WHEN** a reflected picker is open and a visible result is hovered
- **THEN** that result can supply a tooltip while covered fields cannot

### Requirement: Themed final tooltip overlay

TooltipHost SHALL use UiTheme/UiSkin and UITextLayout for colors, metrics, measurement and wrapping. Its rectangle SHALL remain within the available surface bounds using anchor-adjacent placement, flipping and clamping as needed. Explicit line breaks and long unbroken text SHALL remain bounded. The shell SHALL paint the tooltip after ordinary surface content and outside child scroll clips, under the correct per-surface scale. Tooltips SHALL NOT alter retained layout or input targeting.

#### Scenario: Long logical path near a window edge
- **WHEN** a tooltip contains a long asset path and its anchor is near the right or bottom edge
- **THEN** text wraps and the tooltip is repositioned within the current surface without truncating the underlying path data

#### Scenario: Floating surface uses a different scale
- **WHEN** the same control is displayed on a floating surface with another UI scale
- **THEN** its tooltip uses that surface's theme, text measurement and bounds

### Requirement: Toolbar labels use the shared tooltip host

ToolBar SHALL expose action labels through the common tooltip provider protocol. It SHALL retain icon-only presentation and SHALL NOT draw an additional local tooltip after shared hosting is enabled.

#### Scenario: Hovering a toolbar action
- **WHEN** a toolbar action is hovered through the host's delay
- **THEN** its label appears once in the surface tooltip overlay without invoking the action
