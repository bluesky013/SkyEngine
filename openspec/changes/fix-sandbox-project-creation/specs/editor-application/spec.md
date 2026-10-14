## ADDED Requirements

### Requirement: Responsive project creation with progress
The project manager SHALL run project creation outside the UI thread and SHALL display the current reported stage and elapsed time while creation is pending. Conflicting project actions SHALL be disabled, and Quit SHALL remain available.

#### Scenario: A slow project creator
- **WHEN** the user confirms a new project and the creator is still running
- **THEN** the picker callback SHALL return without waiting for creation
- **AND** the hub SHALL continue processing input and rendering progress
- **AND** duplicate creation and opening another project SHALL be prevented

#### Scenario: Stage updates arrive across pipe reads
- **WHEN** the creator emits a supported progress marker, including split reads or CRLF
- **THEN** the hub SHALL display the corresponding real stage without inventing a percentage

### Requirement: Bounded project creation lifecycle
Creation SHALL drain stdout and stderr without blocking the UI, retain bounded diagnostics, and terminate on timeout or shutdown. UI and registry updates SHALL occur only on the main thread.

#### Scenario: Creator fails or stalls
- **WHEN** Python cannot start, exits unsuccessfully, or exceeds the creation timeout
- **THEN** the hub SHALL restore project actions and display the failure reason
- **AND** the recent-project registry SHALL remain unchanged

#### Scenario: Shutdown during creation
- **WHEN** the user exits while creation is pending
- **THEN** the module SHALL cancel the process and join its worker before destruction or unload

### Requirement: Validated project creation result
The hub SHALL register creation only after successful process exit and validation of the generated descriptor. The creator SHALL receive paths as separate arguments.

#### Scenario: Successful temporary project with special characters
- **WHEN** a new project name contains spaces and shell metacharacters
- **THEN** the creator SHALL generate the descriptor, native templates and asset directories at the requested location
- **AND** the hub SHALL register that exact descriptor and show a completion message

#### Scenario: Invalid creation result
- **WHEN** the creator exits successfully without a valid named descriptor
- **THEN** the hub SHALL report failure without registering the project
