## ADDED Requirements

### Requirement: Navigation codec validates allocation bounds

The existing navigation format SHALL validate mode/version, tile count, distinct coordinate/layer keys, declared payload sizes and remaining encoded bytes before allocating corresponding containers. The codec SHALL support at most 65,536 tile records and 256 MiB aggregate tile/full payload. Invalid decode SHALL invalidate the archive and clear output; invalid save SHALL fail explicitly rather than narrow oversized counts or payload sizes.

#### Scenario: Tiny payload declares huge tile count

- **WHEN** a small otherwise framed payload declares UINT32_MAX tile records
- **THEN** decoding SHALL fail before allocating the declared tile container

#### Scenario: Tile payload consumes following records

- **WHEN** a declared tile payload exceeds remaining bytes after reserving the following tile headers and final full-data size field
- **THEN** decoding SHALL fail before resizing that payload

#### Scenario: Unsupported schema or duplicate layer

- **WHEN** mode/version is unsupported or a coordinate/layer key repeats
- **THEN** decoding SHALL fail explicitly without leaving partially loaded output
