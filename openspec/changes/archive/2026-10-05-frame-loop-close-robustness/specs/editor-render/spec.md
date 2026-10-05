## MODIFIED Requirements

### Requirement: Clear and present frame loop
`EditorRender` SHALL run a per-frame loop that acquires a backbuffer, clears it, submits, and presents it to the
editor window, tolerating a null backbuffer or zero extent by skipping the frame. It SHALL check the surface
extent BEFORE acquiring so a zero-size surface does not acquire an image it cannot present, and SHALL release an
acquired image that turns out unusable instead of leaving it unreleased. It SHALL react to window lifecycle
events so it never acquires a window that is closed: a closed preview window's viewport SHALL be dropped.

#### Scenario: Present a cleared frame
- **WHEN** a frame is rendered with a valid backbuffer
- **THEN** the backbuffer SHALL be cleared to the configured color and presented to the editor window

#### Scenario: Skip invalid frames
- **WHEN** the backbuffer is null or the extent is zero (for example while minimized or out of date)
- **THEN** the loop SHALL skip rendering that frame without crashing

#### Scenario: Zero extent does not acquire
- **WHEN** the surface extent is zero at the start of a frame
- **THEN** the loop SHALL skip acquiring a backbuffer for that frame

#### Scenario: Unusable acquired image is not leaked
- **WHEN** an image was acquired but no backbuffer is available
- **THEN** the loop SHALL release the acquired image before returning

#### Scenario: Closed preview window is not acquired
- **WHEN** the standalone preview window receives a close request
- **THEN** the renderer SHALL drop that window's viewport and SHALL NOT acquire it again
