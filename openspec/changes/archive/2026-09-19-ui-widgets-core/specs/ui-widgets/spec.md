## ADDED Requirements

### Requirement: Theme-driven widget painting
Widgets SHALL resolve their appearance from the active `UITheme` through their style classes during paint,
rather than from hard-coded constants. The paint context SHALL expose the active theme to widgets, and a theme
replacement SHALL affect subsequently painted widgets.

#### Scenario: Theme color used for paint
- **WHEN** a widget with a style class is painted and the theme maps that class to a color
- **THEN** the emitted draw data SHALL use the theme's color

#### Scenario: Theme replacement affects paint
- **WHEN** the active theme's style for a widget class is changed and the widget is painted again
- **THEN** the emitted draw data SHALL use the new color

### Requirement: Panel container
A `Panel` SHALL be a container element that clips its children to its bounds and paints a theme-driven
background over its bounds.

#### Scenario: Panel clips children
- **WHEN** a child of a panel paints a rect extending beyond the panel bounds
- **THEN** the child's draw command clip SHALL be limited to the panel bounds

#### Scenario: Panel paints background
- **WHEN** a panel with a style class is painted
- **THEN** it SHALL emit geometry covering its bounds using the resolved background color

### Requirement: Image widget
An `Image` SHALL paint a textured quad over its bounds using an opaque texture handle and a UV rect. An invalid
texture handle SHALL emit no geometry.

#### Scenario: Image binds texture handle
- **WHEN** an image is configured with a texture handle and painted
- **THEN** its draw command SHALL reference that handle

#### Scenario: Image without texture
- **WHEN** an image has an invalid texture handle
- **THEN** painting it SHALL emit no draw command

### Requirement: Button widget
A `Button` SHALL track interaction state (normal, hover, pressed, disabled), paint theme-driven colors per
state, ignore input while disabled, and invoke its click callback when a press and release both occur inside
its bounds.

#### Scenario: Click fires inside bounds
- **WHEN** a button receives a pointer down and a pointer up both inside its bounds
- **THEN** its click callback SHALL be invoked

#### Scenario: Release outside does not click
- **WHEN** a button receives a pointer down inside its bounds and a pointer up outside its bounds
- **THEN** its click callback SHALL NOT be invoked

#### Scenario: Disabled ignores input
- **WHEN** a button is disabled and receives pointer input
- **THEN** it SHALL change no state and SHALL NOT invoke its click callback

#### Scenario: Pressed state affects paint
- **WHEN** a button is pressed and painted
- **THEN** the emitted draw data SHALL use the theme's pressed color

### Requirement: Core paint clipping for containers
The core paint walk SHALL push and pop the clip rect for any element that clips its children, so container
widgets do not implement clip bookkeeping themselves.

#### Scenario: Clip restored after children
- **WHEN** a clipping container finishes painting its children
- **THEN** the paint clip SHALL be restored to the value before the container
