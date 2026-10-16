## ADDED Requirements

### Requirement: Every editor surface multiplies device DPI by user scale once
Main and floating editor windows SHALL retain separate device-DPI and user-preference inputs. SetUiScale SHALL set the main device input, SetSurfaceScale the floating device input, and SetUserUiScale the user multiplier; effective scale queries SHALL use the same normalized product clamped to 0.5–4. The supported user preference range SHALL remain 0.5–2. A floating device override SHALL NOT bypass or duplicate the user multiplier. Startup/hub preference load and later Apply SHALL share this policy, retaining the developer environment override at the device-input boundary.

#### Scenario: Floating surface has a different DPI
- **WHEN** main/floating device scales differ and the same user multiplier is applied
- **THEN** each surface SHALL compute its own device×user result once rather than reuse an already-multiplied main value

#### Scenario: User multiplier changes after construction
- **WHEN** UI scale is applied from Preferences
- **THEN** the next main/floating layout SHALL use the updated effective scale and existing theme-revision invalidation

#### Scenario: Scale input is invalid or out of range
- **WHEN** a supplied scale is nonfinite or outside its supported range
- **THEN** normalization/clamping SHALL keep effective theme geometry finite and within the declared effective range

### Requirement: Surface presentation and input establish the target theme
The existing host GUI dispatch SHALL preserve its supplied surface identity through Shell presentation. Shell pointer, key and text entry points SHALL select that target surface's effective theme and prepare its layout before routing input, with existing owner/attachment lifetime guards. A geometry query without a supplied window context SHALL NOT be assumed to infer the target surface; the presentation/input host establishes that context.

#### Scenario: Main modal receives input after a floating paint
- **WHEN** a different-DPI floating surface paints and the main window next receives pointer, key or text input
- **THEN** input preparation SHALL select the main window's theme/layout before routing, including modal Cancel and field hit targets

#### Scenario: Host presents a floating surface
- **WHEN** the GUI callback is invoked for a floating surface
- **THEN** the supplied identity SHALL reach EditorShell::PaintSurface rather than presenting the main surface in that window
