## ADDED Requirements

### Requirement: Native floating input reaches its existing surface dispatcher
SandboxModule SHALL route key-down, key-up and filtered text events from a known floating native window to the existing EditorShell surface APIs. A known floating window SHALL NOT be learned as the main window when its event arrives first. Control-character filtering and existing main-window routes SHALL remain in effect.

#### Scenario: Restored floating window receives the first event
- **WHEN** a known floating native window receives keyboard/text input before the main window
- **THEN** the host SHALL resolve its current surface identity and dispatch there without replacing the main-window identity
