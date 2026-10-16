## ADDED Requirements

### Requirement: Generic nested scalar properties
The document loader SHALL flatten nested object members in props into dotted property paths and pass scalar leaves through the registered element's SetProperty. Boolean, int64-compatible integer, other numeric float, and string values SHALL preserve their corresponding UIPropertyValue types. The layout, visual, and button groups SHALL retain their specialized parsing and SHALL NOT be dispatched a second time through the generic path. Arrays, null, unsupported values, and unknown property paths SHALL remain silently ignored without failing document loading. Generic dispatch SHALL NOT require widget-specific casts.

#### Scenario: Registered setter receives nested scalar properties
- **WHEN** a registered element is loaded with nested boolean, signed-64-bit-compatible integer, float, and string props
- **THEN** its SetProperty SHALL receive the dotted leaf paths and corresponding typed scalar values

#### Scenario: Existing specialized groups are decoded once
- **WHEN** a document supplies layout arrays, a visual color or texture reference, or button interactability
- **THEN** the existing specialized parsing SHALL apply those values once without duplicate generic setter calls

#### Scenario: Unknown and unsupported properties are harmless
- **WHEN** a document contains unknown property paths, array leaves, or null leaves
- **THEN** loading SHALL continue without a property diagnostic or a failed document

### Requirement: Declarative text wrap property
The generic text property path SHALL accept boolean text.wrap and apply it through the same Text setter used by code-built widgets. JSON text properties and C++ construction SHALL produce the same wrap setting and constrained measure/paint behavior. An omitted wrap property SHALL remain false, preserving existing documents and no-wrap callers.

#### Scenario: JSON enables wrap
- **WHEN** a Text document supplies boolean wrap in its text properties
- **THEN** the loader SHALL apply the text.wrap property and finite-width measurement SHALL use soft wrapping

#### Scenario: C++ and document behavior match
- **WHEN** equivalent Text widgets are configured in C++ and JSON with the same content/font/wrap/width
- **THEN** their constrained desired size and painted line layout SHALL agree

#### Scenario: Existing document retains default
- **WHEN** a Text document omits text.wrap
- **THEN** its wrap flag SHALL remain false
