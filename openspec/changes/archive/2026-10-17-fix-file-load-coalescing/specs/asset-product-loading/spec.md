## ADDED Requirements

### Requirement: Concurrent product loads coalesce

Concurrent `AssetManager::LoadAsset` calls for the same UUID SHALL perform at most one product-header read and SHALL return the same in-flight asset while its status is `LOADING` or once it is `LOADED`. Callers SHALL be able to block on the existing load handle instead of starting a second load.

#### Scenario: Same UUID from many threads
- **WHEN** multiple threads call `LoadAsset` for the same product UUID before it is loaded
- **THEN** only one thread SHALL read the header/schedule the load and every caller SHALL receive a non-null asset

#### Scenario: Load already in flight
- **WHEN** a caller observes an asset with status `LOADING`
- **THEN** it SHALL return that asset without re-opening or re-reading the product

#### Scenario: Recursive dependency load
- **WHEN** a product load recursively loads its dependencies
- **THEN** the load orchestration lock SHALL be recursive and SHALL NOT deadlock

#### Scenario: Concurrent stress
- **WHEN** the existing `AssetManagerTest.ConcurrentLoadStress` runs repeatedly
- **THEN** no thread SHALL receive a null asset or a handler-registration error
