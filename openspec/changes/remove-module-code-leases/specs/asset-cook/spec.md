## MODIFIED Requirements

### Requirement: Cook data compression

Cook MAY compress product data. The product header SHALL record the compression codec, and the loader SHALL decompress via the Framework CompressionManager/ICompressor interface. Framework SHALL provide default LZ4 when SKY_BUILD_COMPRESSION is enabled, without a dynamically loaded compression module. Disabling the built-in SHALL preserve uncompressed loading and explicit unsupported-codec failures. Operations SHALL retain acquired provider handles until their calls finish. Hosts SHALL establish the shared manager before loading modules; acquired handles SHALL retain replacement-provider objects through use and destruction while defining DLL code remains process-resident.

#### Scenario: Compressed product round-trips

- **WHEN** a target is configured to compress and Framework built-in LZ4 is enabled
- **THEN** the loader SHALL decompress the product before deserialization without loading any codec module

#### Scenario: Uncompressed needs no codec

- **WHEN** a product is not compressed
- **THEN** loading SHALL NOT require a compression implementation or module

#### Scenario: Support disabled

- **WHEN** built-in compression is disabled and no explicit codec replacement is provided
- **THEN** requesting an LZ4 product operation SHALL fail explicitly without loading an optional module or changing the encoding

#### Scenario: Replacement removed during retained use

- **WHEN** an acquired provider remains referenced while its registry replacement is removed or disabled
- **THEN** the acquired provider object SHALL remain valid until that handle is released, with defining DLL code remaining process-resident, while subsequent acquisition observes the current selection
