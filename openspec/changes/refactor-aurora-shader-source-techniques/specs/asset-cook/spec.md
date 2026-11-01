## ADDED Requirements

### Requirement: Explicit normalized variant demand

Cook SHALL retain JSON companions and explicit stage entry programs, collect requested Vertex/Pipeline/Shader-local choices through the existing resource graph and normalize them using the runtime selector. Material demand, explicit project/module requests and optional usage capture SHALL share this contract. Cook SHALL not automatically enumerate all possible combinations. Values, source-symbol mappings and native metadata SHALL be validated before products are published.

#### Scenario: Equivalent requests
- **WHEN** two demand sources express the same choice with different ordering or omitted defaults
- **THEN** they yield one canonical selection and reusable stage products

### Requirement: Complete compiler-free distributions

Products SHALL retain generated logical Material/variant metadata, exact allowed choices, code/reflection and stage-native mappings for Scene, Fullscreen and Compute. A relocated host with sources and compiler libraries absent SHALL execute published choices and reject missing choices. Product versions and codecs SHALL migrate atomically with affected assets and tests; C++ shared exports and existing tools/targets SHALL be retained.

#### Scenario: Stripped nondefault request
- **WHEN** a relocated compiler-free host requests a published nondefault selection
- **THEN** it obtains the correct target artifact, native values and bindings from products

## MODIFIED Requirements

### Requirement: MaterialInstance builder（.mati）

AuroraMaterialInstanceBuilder SHALL read a typed MaterialInstanceSource, retain the parent UUID and persist value/texture/sampler overrides with semantic validation. Parent and texture UUIDs SHALL register product and applicable source dependencies. Complete key ownership and static-option restrictions SHALL be checked against the resolved parent schema during preparation; the builder SHALL NOT invent keys or shader layouts from JSON scalar shapes.

#### Scenario: Instance product
- **WHEN** a valid typed instance source is cooked
- **THEN** the product contains the parent and sparse overrides with their asset dependencies

#### Scenario: Wrong value type
- **WHEN** an override's semantic type disagrees with its stored value
- **THEN** cook returns failure without treating the mismatched payload as valid bytes

### Requirement: Material builder（.mat）

AuroraMaterialBuilder SHALL read the reflected typed .mat schema, validate MaterialAssetData and publish a versioned product with explicit technique stage/source/entry records and typed properties. New source values SHALL use canonical Material keys; legacy alias migration SHALL be checked against complete declaration metadata during preparation. Shader paths SHALL remain logical paths rather than UUID assets. Texture UUIDs SHALL register product and applicable source dependencies. A generic number/string JSON inference table SHALL NOT replace the typed source schema or shader-key validation.

#### Scenario: Typed material product
- **WHEN** a valid .mat with tagged stage programs and typed texture/value properties is cooked
- **THEN** the product preserves those values and records texture dependencies

#### Scenario: Invalid semantic value
- **WHEN** a property's stored value does not match its declared semantic type
- **THEN** cook fails before publishing a successful product
