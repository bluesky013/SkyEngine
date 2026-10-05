## ADDED Requirements

### Requirement: Registered derived-data builders

The framework SHALL allow derived-data builders to be registered by a stable string id, and each
builder SHALL declare a version so that changing its build algorithm invalidates previously cached
results.

#### Scenario: Builder resolved by id

- **WHEN** a builder is registered under an id
- **THEN** the derived data cache SHALL resolve that builder by id for subsequent fetches

### Requirement: Content-addressed caching

The derived data cache SHALL store a builder's output under a key derived from the source bytes,
the builder id, the builder version, the settings string, and the platform. A fetch SHALL return the
stored output when the key is unchanged and SHALL run the builder and store the output on a miss;
changing any key input SHALL produce a miss.

#### Scenario: Cache hit avoids rebuild

- **WHEN** the same source, builder, version, settings, and platform are fetched twice
- **THEN** the builder SHALL run only once and both fetches SHALL return identical output

#### Scenario: Settings and platform are part of the key

- **WHEN** the same source is fetched with a different settings string or a different platform
- **THEN** the cache SHALL run the builder and store a separate entry

#### Scenario: Builder version invalidates

- **WHEN** the registered builder's version changes for otherwise identical inputs
- **THEN** the cache SHALL miss and run the builder again

### Requirement: Failure is reported without caching

The derived data cache SHALL report failure when no builder is registered for the requested id or
when the builder fails to build, and SHALL NOT write a cache entry in that case.

#### Scenario: Unknown builder

- **WHEN** a fetch is requested for an unregistered builder id
- **THEN** the cache SHALL report failure without writing an entry
