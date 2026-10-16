## ADDED Requirements

### Requirement: File storage honors the portable replacement contract
FilePreferenceStorage SHALL implement the existing IPreferenceStorage over an explicit path, distinguish NotFound from ReadFailed, and complete/close an exclusive same-directory temporary file before replacing the destination. A failed write SHALL preserve previous destination bytes and clean its temporary file. The owner SHALL serialize calls. The adapter SHALL NOT claim cross-writer compare-and-swap or power-loss durability; JSON/schema handling remains in PreferencesPersistence.

#### Scenario: Replace a real saved document
- **WHEN** a complete file Write succeeds
- **THEN** a subsequent Read SHALL return the new bytes and no operation-owned temporary file SHALL remain

#### Scenario: File write cannot complete
- **WHEN** directory, temporary-write or replacement work fails
- **THEN** Write SHALL report failure, keep any previous destination bytes and clean temporary artifacts created by that attempt

#### Scenario: Missing file versus invalid location
- **WHEN** the path is absent or cannot be read as a valid preference file
- **THEN** Read SHALL distinguish absence from read failure under the existing storage result protocol
