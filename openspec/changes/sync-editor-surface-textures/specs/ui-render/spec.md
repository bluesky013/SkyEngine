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
