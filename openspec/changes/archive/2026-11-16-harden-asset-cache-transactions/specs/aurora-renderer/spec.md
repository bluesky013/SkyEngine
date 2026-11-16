## ADDED Requirements

### Requirement: Immutable shared CPU payload ownership
Scene resource caches SHALL share immutable payloads rather than copying material/image bytes into each mesh entry. Budget accounting SHALL measure unique cache-owned CPU payload identities including backend, device epoch, variant and content revision; it is not process RSS. Refresh clears the full cache and resets its accounting. External snapshots surviving refresh are outside the cache-owned budget and SHALL be documented as such.

#### Scenario: Multiple meshes share one material
- **WHEN** meshes resolve the same material variant in one device epoch
- **THEN** they share its payload storage and do not consume the material byte budget repeatedly

### Requirement: Late preview defaults
Preview providers and ground defaults SHALL permit late registration. Updating defaults affects future sessions and SHALL NOT fail merely because another session already exists.

#### Scenario: Provider appears after a preview opens
- **WHEN** ground assets are registered while an existing session is open
- **THEN** newly created sessions use the new defaults
