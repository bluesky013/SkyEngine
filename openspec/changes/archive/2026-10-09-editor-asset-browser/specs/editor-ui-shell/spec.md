## ADDED Requirements

### Requirement: Built-in asset browser panel

The shell SHALL register a built-in view for the `assets` panel id and SHALL include it in the default
layout, composed from the core layout and panel registry like the other built-in panels. The panel view
SHALL render the asset browser with the in-house `sky::ui` toolkit.

#### Scenario: Registered built-in view
- **WHEN** the shell registers its built-in panel views
- **THEN** an `assets` panel id SHALL have a view that renders the asset browser

#### Scenario: Present in the default layout
- **WHEN** the editor starts with the default layout
- **THEN** the `assets` panel SHALL be part of it and SHALL dock, float, and tab like the other panels
