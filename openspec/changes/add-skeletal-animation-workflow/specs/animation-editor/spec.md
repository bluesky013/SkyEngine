## ADDED Requirements

### Requirement: Production-path isolated animation preview
The editor SHALL provide a separate Preview World for skeletal mesh, Clip and Graph inspection using the production components and skinning path. Its scene, view and output SHALL use the existing renderer host and packet producer, without creating another Device or renderer. Preview time and parameter changes SHALL NOT mutate the editing World or PIE instance.

#### Scenario: Independent preview
- **WHEN** a user changes preview time or parameters while the same asset is used in the editing World
- **THEN** only the Preview World instance SHALL change

#### Scenario: Close with frames in flight
- **WHEN** a preview window is closed while its output or palettes are in flight
- **THEN** logical teardown SHALL begin immediately and native/output resources SHALL be released only after the existing retirement protocol permits it

### Requirement: Skeleton and clip inspection
The preview SHALL provide independent Mesh/Skin/Skeleton selection, resolved binding diagnostics, skeleton tree, selected-bone transform, bone/bounds display and Play/Pause/Stop/rate controls. Clip preview SHALL provide loop and scrub; Graph preview SHALL use its node loop settings and SHALL NOT expose whole-graph Seek. Pending assets and failures SHALL be visible. Paused Clip scrubbing SHALL use the runtime evaluation path.

#### Scenario: Scrub a known two-bone clip
- **WHEN** the timeline is moved to a known sample time while paused
- **THEN** the displayed mesh, bone transform and bounds SHALL correspond to that pose and playback SHALL remain paused

#### Scenario: Preview another binding of the same mesh
- **WHEN** the user keeps the Mesh selection and changes Skin/Skeleton
- **THEN** the preview SHALL validate and display the selected combination or identify the incompatible slot without mutating other instances

### Requirement: Reusable graph canvas with domain-owned semantics
Sandbox SHALL provide a domain-neutral graph canvas for zoom/pan, node movement, selection, typed-pin interaction, connection display and editing actions. The animation plugin SHALL provide node definitions, validation and state-machine semantics. Runtime and builder targets SHALL NOT depend on canvas or editor modules.

#### Scenario: Canvas edits a graph
- **WHEN** nodes are moved or connected through the canvas
- **THEN** the document SHALL receive stable-identity editing commands and the canvas SHALL NOT instantiate animation runtime objects directly

### Requirement: Reflected and undoable graph editing
Graph documents SHALL use CommandService for node, connection, parameter, transition and layout edits; a drag SHALL be one undo transaction. Node/settings properties SHALL use ReflectedFormView or descriptor-provided reflected objects. Menus, toolbars and shortcuts SHALL use EditorActionRegistry; dimensions/colors SHALL use UiTheme and UiSkin.

#### Scenario: Undo a node deletion
- **WHEN** a node with links is deleted and the action is undone
- **THEN** its identity, properties, position and links SHALL be restored together

#### Scenario: Reflected parameter edit
- **WHEN** a graph default is edited in the reflected form and undone
- **THEN** both the document value and preview compilation input SHALL return to their previous value

### Requirement: Deferred save and honest preview versions
Source edits SHALL remain buffered until Save. Save SHALL resolve paths through AssetDataBase, write a serializable schema-valid source atomically once, and enqueue deferred cook. Incomplete executable graphs SHALL remain saveable drafts and SHALL fail cook with diagnostics. UI dispatch SHALL NOT cook or synchronously rebuild/destroy its active widget. Saved-source, cook and preview revisions SHALL be tracked separately. v1 preview SHALL instantiate the last successful cooked product through the product loader; unsaved drafts SHALL receive structural diagnostics but SHALL NOT masquerade as a new running product.

#### Scenario: Save then cook fails
- **WHEN** a source save succeeds but asynchronous cook fails
- **THEN** the source SHALL remain saved, the cook diagnostic SHALL be visible, and any retained old preview SHALL be identified by its old revision

#### Scenario: Save during pointer dispatch
- **WHEN** a Save action originates from a pointer event
- **THEN** heavy work SHALL be deferred until dispatch completes and the active widget SHALL remain valid for the rest of the event

#### Scenario: Save unfinished graph work
- **WHEN** a serializable graph has no root output and the user saves it
- **THEN** the document SHALL reopen with the unfinished edits preserved, cook SHALL report the missing output, and any old preview SHALL remain explicitly identified as old

### Requirement: Graph diagnostics and runtime inspection
The Graph editor SHALL display parameter values, active state, transition progress and diagnostics mapped to source node/pin identities. Temporary debug parameter overrides SHALL be distinct from persisted defaults. Unsupported nested graphs and root-motion extraction SHALL be diagnosed rather than silently approximated.

#### Scenario: Usable viewport and preview setup
- **WHEN** a Graph opens or its window is resized
- **THEN** the canvas and selected inspector SHALL fit the available viewport, with Graph, selected-node and preview settings accessible through scoped action tabs
- **AND** an unavailable preview SHALL show an in-viewport diagnostic and setup guidance rather than an unexplained empty rectangle

#### Scenario: Scale or frame a Graph
- **WHEN** DPI changes or the user frames all nodes
- **THEN** logical node positions and connections SHALL remain unchanged, drawing and hit-testing SHALL use the same scale, and framing SHALL update only the canvas view
- **AND** nodes SHALL prefer readable asset/state names over UUID-only titles

#### Scenario: Mesh becomes ready after its skeleton
- **WHEN** a configured preview Mesh finishes loading after the Skeleton
- **THEN** initial camera framing SHALL wait for Mesh bounds and fit them using the viewport aspect ratio and field of view
- **AND** loading diagnostics SHALL remain readable without bone or bounds overlays obscuring them

#### Scenario: Locate a compile failure
- **WHEN** graph compilation reports an invalid connection
- **THEN** the editor SHALL identify the owning node/pin and preserve the editable document

### Requirement: Honest skinned selection capabilities
Until deformation-aware triangle picking and highlighting are implemented, the editor SHALL use Outliner or explicitly identified conservative bounds selection for skinned objects. It SHALL NOT reuse bind-pose triangle picking or positions-preserved static highlighting as if accurate for the animated pose.

#### Scenario: A limb leaves the bind pose
- **WHEN** a skinned limb moves outside its original mesh bounds
- **THEN** animated bounds SHALL remain visible/selectable and the editor SHALL NOT display an incorrect bind-pose selection outline
