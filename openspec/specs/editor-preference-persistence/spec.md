# editor-preference-persistence Specification

## Purpose
Provide portable checked Preferences capture, validation, storage and publication with explicit conflict, failure and partial-effect results.

## Requirements
### Requirement: Preference operations return explicit results and conservative effect flags
Portable preference operations SHALL return a PreferenceResult with a status code, failing page/path/message context where available, and storageWritten/liveWritten/applicationApplied flags for that operation. storageWritten SHALL become true only after explicit Write success. liveWritten SHALL become true before entering the first live setter and applicationApplied before entering the first page application callback, including rejection/exception paths. These two flags SHALL indicate entry into potentially effectful stages rather than prove a value changed or application completed. Complete success SHALL be distinct from absent storage, storage failure, invalid/unsupported data, validation/application rejection, identity conflict and busy/reentrant rejection. A failed result SHALL NOT imply that potential effects were rolled back.

#### Scenario: Application fails after a durable write
- **WHEN** persistence succeeds but a later live setter or page application rejects
- **THEN** the result SHALL report failure with storageWritten and conservative live/application stage flags rather than report complete success or claim rollback

#### Scenario: Invalid data identifies its location
- **WHEN** a known page/member cannot be decoded or validated
- **THEN** the failure SHALL identify its page/path where available without depending on native UI state

### Requirement: Owned candidate copy exceptions unwind safely
If a reflected working-value copy throws during candidate capture, Apply SHALL return InvalidData before storage/live/application stages and retain the current working value/history for retry. Any::Create SHALL publish its type/storage state only after successful construction and SHALL use RAII to release raw storage if placement construction fails, without invoking the complete-object destructor on an incompletely constructed value. The construction exception SHALL propagate naturally to the existing capture boundary. Its public signature, failure contract and the generic JSON loader SHALL remain unchanged by this local correction.

#### Scenario: Nontrivial working settings reject copying
- **WHEN** an edited settings value containing a heap-backed string throws from its reflected copy constructor before candidate capture completes
- **THEN** the operation SHALL report InvalidData with no storage/live/application stage flags, preserve current working/history/live/storage state, and safely release incomplete candidate storage
- **AND** a later corrected candidate SHALL remain available for retry

### Requirement: Storage is injected and failed writes preserve old bytes
PreferencesPersistence SHALL use an injected IPreferenceStorage for document Read/Write. Read SHALL distinguish NotFound from a read error. A failed or throwing Write SHALL preserve that adapter's previous document bytes; adapters with partial replacement on failure/throw SHALL be unsupported. Operations on one store SHALL run under its owner's serialized-call contract. This SHALL NOT promise a transaction spanning storage, live setters and application callbacks.

#### Scenario: Storage is absent
- **WHEN** the adapter reports NotFound during Load
- **THEN** Load SHALL report absence and preserve provider-supplied live values

#### Scenario: Storage write rejects
- **WHEN** Write returns failure for a prepared document
- **THEN** previous bytes SHALL remain and session Apply SHALL publish no live values through that failed storage gate

### Requirement: Reflected preference decoding is checked before publication
The portable codec SHALL read the existing root-page-id to classId/elements object envelope through checked JSON/reflection operations over owned candidates. It SHALL validate known data shape, type/class identity, numeric validity/range and supported schemas without invoking assertion-based JsonInputArchive decoding or blindly executing arbitrary custom codecs. Invalid or unsupported known schemas SHALL return explicit failures before live publication. Runtime registration numbers and object addresses SHALL NOT be serialized as persistent identity.

#### Scenario: Field metadata retains current boundaries
- **WHEN** known reflected fields are encoded, decoded and published
- **THEN** encoding SHALL include readable fields, decoding SHALL change only writable candidate fields, and publication SHALL retain the existing top-level const/no-setter/READONLY/invisible filtering
- **AND** Load/Save SHALL NOT filter fields solely by the unreliable existing isStatic metadata

#### Scenario: Malformed or incompatible known data
- **WHEN** stored JSON has a malformed known value, mismatching classId or unsupported reflected schema
- **THEN** the operation SHALL fail with context and SHALL publish no partially decoded live page

#### Scenario: Missing fields retain provider defaults
- **WHEN** a valid document omits a registered page or one of its known fields
- **THEN** its owned candidate SHALL retain that provider-supplied value rather than reset unspecified data to zero

### Requirement: Save preserves unknown object data and replaces known arrays
Save SHALL merge encoded known fields into a fresh storage read while retaining unknown root/page/object keys and pages for currently absent plugins. Known arrays SHALL replace prior known arrays, including replacement by an empty array. Invalid existing documents SHALL be reported rather than silently overwritten as empty storage. Save SHALL NOT itself write live settings.

#### Scenario: Plugin is absent from this registry
- **WHEN** Save updates known registered pages while an existing document contains an unregistered plugin page and unknown object keys
- **THEN** those unknown entries SHALL remain in the saved document

#### Scenario: Sequence is replaced or cleared
- **WHEN** a known candidate sequence is shorter than its stored version or is empty
- **THEN** the saved/loaded known sequence SHALL match the candidate sequence without appending stale elements

#### Scenario: Read fails before Save
- **WHEN** the fresh storage read returns a read failure rather than NotFound
- **THEN** Save SHALL return failure without a document write or live publication

#### Scenario: Save owns input before callbacks
- **WHEN** Save prepares candidates before a storage/getter callback can release its caller
- **THEN** it SHALL retain explicit Any::Create-owned snapshots for the operation and convert a snapshot-copy exception through the safe InvalidData path

#### Scenario: Reflected type has a nonconst getter
- **WHEN** a readable reflected member supplies getterFn with no usable getterConstFn
- **THEN** the codec SHALL support reading that field from its owned candidate, preferring getterFn when present and rechecking validity afterward

### Requirement: Load prepares all candidates at a controlled host boundary
Load SHALL be called at a host-controlled boundary with no open editing session. It SHALL copy current provider values, decode all known data and validate the full candidate set before guarded live publication and page application. Decode/validation/read failures SHALL publish nothing. Publication failures SHALL report conservative partial live/application stage flags. The service SHALL NOT silently replace an open session's working objects.

#### Scenario: A later page fails validation
- **WHEN** an earlier page decodes successfully but another candidate fails decoding or validation
- **THEN** Load SHALL leave all live pages unmodified by that preparation attempt

#### Scenario: Publication callback rejects
- **WHEN** all loaded candidates validate but a later setter/application callback fails during publication
- **THEN** Load SHALL stop further publication and report conservative partial-effect flags without claiming arbitrary external rollback

### Requirement: Page callbacks retain provider ownership and identity guards
PreferencePage MAY supply pure candidate validation and retry-safe application callbacks. Validation SHALL run on an owned candidate before persistence/live writes; application SHALL run after guarded live writes. Callback/value state SHALL be copied where necessary and registrations/session/owner validity SHALL be rechecked after callback boundaries. Re-registration at the same address SHALL remain a new identity. Providers SHALL preserve metadata/object/callback lifetime and tolerate retry after potentially effectful rejection/exception; concurrent mutation and DLL unload are outside this contract.

#### Scenario: Validator mutates registration identity
- **WHEN** a callback removes or re-registers a candidate page
- **THEN** the old operation SHALL report conflict and SHALL NOT continue accessing the obsolete page registration

#### Scenario: Application is retried after partial failure
- **WHEN** a failed operation with partial effects is retried
- **THEN** the provider callback SHALL tolerate repeated application and the new result SHALL describe that new operation's effects

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

### Requirement: Exception translation is limited to provider boundaries
Preferences SHALL translate failures only at injected guard, reflection/provider and storage-adapter calls covered by its structured result protocol. Ordinary JSON/collection/allocation work SHALL NOT acquire whole-operation catches. Session SHALL reuse shared candidate capture. Any and file resources SHALL use RAII or equivalent ownership that releases incomplete resources when construction or allocation exits exceptionally.

#### Scenario: Reflected copy throws before completing construction
- **WHEN** a reflected value constructor throws
- **THEN** unconstructed allocation SHALL be released without running the complete-object destructor, and capture SHALL retain its existing structured failure result

#### Scenario: Ordinary file or memory failure occurs
- **WHEN** file APIs report an IO error
- **THEN** storage SHALL report the existing IO result and preserve failed-write destination bytes; unrelated allocation exceptions SHALL propagate through RAII cleanup without a new blanket catch
