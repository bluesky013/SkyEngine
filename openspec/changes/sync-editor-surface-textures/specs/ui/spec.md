## ADDED Requirements

### Requirement: Shared image updates preserve GPU generations

UIRenderer registration/update SHALL queue CPU pixel data without mapping staging buffers or releasing GPU resources referenced by submitted work. Upload SHALL create a fresh image/staging/resource-group generation from the latest queued pixels and preserve replaced generations. ReleaseTexture and RegisterImage replacement SHALL retain old GPU generations even after removing/replacing their lookup. The host SHALL collect retired generations only after all submissions referencing them have completed. The current editor integration SHALL use its inflight=1 BeginFrame wait; this contract SHALL NOT imply safety for increased inflight counts without a generation/fence design.

#### Scenario: Shared update arrives while a surface has outstanding work
- **WHEN** shared pixel data changes while a renderer's earlier submission may still read its image or staging
- **THEN** UpdateTexture SHALL only queue CPU pixels and its later upload SHALL use a different GPU generation

#### Scenario: Image is released during frame recording
- **WHEN** ReleaseTexture or image replacement occurs after an earlier draw was recorded with that generation
- **THEN** its GPU resources SHALL remain retained until a completed-submission collection point

#### Scenario: Minimized main viewport still reaches collection
- **WHEN** the frame waits for all preceding single-inflight submissions and then exits because viewport acquire/begin is unavailable
- **THEN** eligible main and floating renderer generations SHALL already have been collected at that safe boundary

#### Scenario: Upload allocation fails
- **WHEN** a new image/staging/group cannot be prepared
- **THEN** the existing visible generation and pending CPU pixels SHALL remain available for a later retry

### Requirement: Shared CPU images across text-system surfaces

UITextSystem SHALL expose a shared-image IUITextureRegistry entry point separate from GetRegistry's borrowed default backend. A backend-independent UI runtime catalog SHALL own current CPU image pixels, replay current images when a backend attaches, mirror subsequent registrations/updates/releases only to live attached backends, and detach without invoking or releasing the borrowed backend. Existing text-system AddRegistry/RemoveRegistry SHALL pair both atlas and shared-image attachments. Viewport/GPU images SHALL remain local to their owning renderer.

#### Scenario: Floating surface attaches after icons exist
- **WHEN** shared editor icons are registered before a floating backend attaches
- **THEN** the new backend SHALL receive the current pixels under the same shared ids

#### Scenario: Wheel or image is registered later
- **WHEN** a shared image is registered or resized/updated after several surfaces attach
- **THEN** all attached backends SHALL receive the same current id and pixel data

#### Scenario: Detached backend is destroyed
- **WHEN** a backend detaches and is destroyed before shared updates, removal, or later glyph/page growth
- **THEN** that backend SHALL receive no later calls and other live backends SHALL continue receiving their own resources

#### Scenario: Current pixels replay on reattachment
- **WHEN** a backend is detached while a catalog image changes and is later reattached
- **THEN** the backend SHALL receive current pixels even if an older image remains under the same id

#### Scenario: Default facade remains explicit
- **WHEN** the default backend detaches while shared CPU images remain
- **THEN** GetRegistry SHALL return null until explicit backend attachment and the shared entry point SHALL remain usable for CPU image storage

### Requirement: Shared texture id ownership

Catalog-generated shared ids SHALL use 0x20000000 through 0x3FFFFFFF, separate from low surface-local ids and glyph pages starting at 0x40000000. Explicit shared RegisterTextureAs SHALL accept only ids at or above 0x80000000 and preserve pixels for an existing id. Duplicate explicit registration SHALL re-ensure the stored image on attached backends so a reset backend can recover. Updates SHALL require an owned id and valid complete RGBA8 pixels; releases of unknown/local/glyph ids SHALL not propagate. Each backend SHALL attach to one shared catalog group.

#### Scenario: Local and glyph resources remain isolated
- **WHEN** a shared caller attempts to register or release a viewport or glyph id
- **THEN** explicit registration SHALL fail and removal SHALL leave those backend-owned resources untouched

#### Scenario: Reset backend recovers immutable wheel
- **WHEN** a backend texture table is reset and the same explicit wheel id is registered again
- **THEN** the stored immutable wheel SHALL be ensured again under that id on attached backends

#### Scenario: Owned image is removed and recreated
- **WHEN** an owned shared image is released and later explicitly recreated
- **THEN** live backends SHALL remove its old lookup and receive the new pixels while detached/unrelated backends receive no calls
