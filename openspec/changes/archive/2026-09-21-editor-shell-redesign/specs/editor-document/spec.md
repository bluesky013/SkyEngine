## ADDED Requirements

### Requirement: Toolkit-independent document lifecycle
The editor SHALL provide a document service, independent of any UI toolkit, that opens, loads, saves, and tracks
the dirty state of an editable document.

#### Scenario: Dirty tracking
- **WHEN** a document is modified after loading
- **THEN** the document SHALL report itself dirty, and saving SHALL clear the dirty state

#### Scenario: Load then save round-trip
- **WHEN** a document is loaded and saved without modification
- **THEN** the saved content SHALL be equivalent to the loaded content

### Requirement: Asset-to-document mapping
The service SHALL map an asset handle to its editable document so asset editors can open a document for an asset.

#### Scenario: Open asset as document
- **WHEN** an asset is opened for editing
- **THEN** the service SHALL provide the corresponding document, creating it if none exists
