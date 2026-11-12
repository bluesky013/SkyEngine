## ADDED Requirements

### Requirement: Deterministic constrained tetrahedral binding
The authoring algorithm SHALL retain mandatory control-point identities, positions and modes, construct validated positive-volume tetrahedra, and assign every binding position to a containing tetrahedron with at most four nonnegative normalized bone weights. Automatic fitting SHALL use the display position as its binding position. Explicit supplied-cage displacement binding MAY use a persisted positive binding-position scale without modifying the display geometry or physical cage. The algorithm SHALL reject invalid coverage or insufficient budgets without silently moving constraints or using nearest-four binding.

#### Scenario: Generate a four-influence seal binding
- **WHEN** a supported closed seal shell and valid author constraints are supplied
- **THEN** generation reproduces the rest mesh, preserves the constraints and produces no vertex with more than four positive influences

#### Scenario: Mandatory point would create invalid cells
- **WHEN** a required insertion produces unacceptable tetrahedron quality
- **THEN** generation returns a control-specific diagnostic and leaves the previous valid output unchanged

### Requirement: Configurable exact control budget
The recipe SHALL persist an optional exact-budget setting whose absent value retains maximum-budget behavior. Small budgets SHALL use a valid six-point shell seed when the larger seed cannot fit. Exact generation SHALL reach the requested count through conforming boundary-edge subdivision before complete stored-precision validation, without adding unused controls or relaxing production quality defaults. An unattainable count SHALL fail and preserve the previous output.

#### Scenario: Generate the performance rig matrix
- **WHEN** exact budgets 30, 25, 20, 15 and 10 are requested for the same supported source and mandatory points
- **THEN** each successful rig contains exactly its requested total, retains the mandatory identities and four-weight binding, and reports its own measured proxy quality

### Requirement: Bounded competing cage candidates
The generator SHALL evaluate the existing radial star candidate and a deterministic bounded support-plane candidate without changing the source geometry, verified center, mandatory controls or requested control budget. The support candidate SHALL begin with six axis-aligned source support planes, use scale-aware safety margins and incremental convex clipping, and accept only cuts within the available boundary-vertex budget. Both candidates SHALL pass the same stored-precision geometry, mandatory insertion, full triangle coverage, surface-distance, tetrahedron-quality, binding and descriptor checks before selection. The generator SHALL publish the valid candidate with the smaller cage volume, retain the star candidate on an equal-volume tie, and preserve the previous output if neither is valid or the request is cancelled.

#### Scenario: Support planes improve a constrained seal cage
- **WHEN** the support-plane candidate passes all shared checks and has lower volume than the valid star candidate
- **THEN** it is selected without increasing the control budget or changing mandatory IDs, positions, modes, the source mesh or the four-influence binding limit

#### Scenario: Support candidate fails a quality gate
- **WHEN** the support-plane candidate fails coverage, distance, tetrahedron quality or binding validation and the star candidate remains valid
- **THEN** the generator publishes the complete valid star candidate rather than an incomplete or invalid support result

### Requirement: Distinguish measured cage quality from acceptance limits
The generator SHALL report volume excess, sampled cage-to-source surface distance, a certified upper bound over the complete cage boundary, and rest-geometry support offsets for -X, +X, -Y, +Y, -Z and +Z. The distance certificate SHALL be one-way nearest distance from cage boundary to source surface; it SHALL NOT be presented as a bidirectional Hausdorff bound. Full source-triangle coverage SHALL be validated separately. Axis support offsets SHALL compare boundary and source extrema and exclude collision radius; they SHALL NOT be presented as dynamic settling or contact validation. General defaults SHALL remain 0.16 m distance and 85% excess volume, while the seal demonstration recipe SHALL persist tighter limits of 0.12 m and 50% with its 30-control maximum and unchanged mandatory points.

#### Scenario: Compare proxy quality under the same budget
- **WHEN** cage candidates are compared or an acceptance threshold changes
- **THEN** measured distances, the certified bound, volume excess and support offsets are reported separately from thresholds, and changing a threshold alone is not reported as a geometry improvement

### Requirement: One recipe owns mesh and softbody products
A versioned authoring recipe SHALL own stable Mesh, Skin, Skeleton and SoftBody child identities. Editor and cook SHALL use the same algorithm owner. Mesh import and product construction SHALL reuse the existing mesh cook provider through interfaces, without linking the softbody builder to runtime, adaptor or editor implementations.

#### Scenario: Save and recook a generated binding
- **WHEN** a saved recipe is reopened and recooked with unchanged inputs and algorithm version
- **THEN** it reproduces the same binding layout and stable child identities, and publishes one complete validated candidate

### Requirement: Validated supplied center-fan binding
The versioned recipe SHALL support an explicit supplied center-fan mode containing ordered controls, a center index, fixed tetrahedra and a positive binding-position scale. It SHALL preserve the supplied physical positions, identities, modes and topology without automatic insertion, fitting, budget filling or collision-cage expansion. It SHALL validate positive finite tetrahedra, index uniqueness, opposite shared-face orientation, a closed connected convex degree-one boundary, use of every control and minimum quality. Complete embedded-triangle coverage SHALL be established independently of the source display shell's star-shapedness. The default automatic mode SHALL remain unchanged.

#### Scenario: Preserve a saved fifteen-point displacement rig
- **WHEN** a valid saved fifteen-control center fan with center index fourteen and binding-position scale 0.7 is supplied
- **THEN** the original physical control order and topology remain unchanged, each display vertex receives at most four normalized nonnegative weights, and the original display rest positions are reproduced by reference/inverse-bind skinning

#### Scenario: Reject an invalid supplied cage atomically
- **WHEN** supplied topology has an invalid index, duplicate or inverted cell, open or non-convex boundary, exterior center, insufficient quality or uncovered binding position
- **THEN** generation fails explicitly without replacing the previous complete output or silently fitting a different cage

### Requirement: Constant-anchor attachments and explicit skin normal policy
An attachment MAY request a model-space binding anchor whose single normalized tetrahedral weight tuple applies to its entire primitive. Version-two recipes and products SHALL persist an explicit normal policy. Standard skinning SHALL publish no custom normal palette and SHALL retain the existing render skin-normal path; automatic assets SHALL continue to default to tetrahedral-gradient normals. Version-one input SHALL preserve its previous defaults. New enums SHALL reject exclusive END and out-of-range raw values before conversion.

#### Scenario: Eye and pupil retain their relative shape
- **WHEN** an eye and its pupil use the same constant binding anchor under a translating control cage
- **THEN** both use the same at-most-four-weight displacement, retain their size and relative offset, and use the selected standard-skinning normal behavior

### Requirement: Distinguish embedding diagnostics and preserve material slots
Supplied-cage diagnostics SHALL distinguish scaled binding-space coverage and binding-position reconstruction from rest skinning and unscaled physical/display geometry quality. Recipes MAY persist a material-product UUID slot table that the existing mesh compiler SHALL preserve together with primitive material indices and product dependencies. Empty tables SHALL retain the prior behavior; material preservation SHALL NOT rewrite geometry, UVs or vertex colors.

#### Scenario: Cook a multi-material supplied animal
- **WHEN** a supplied body and explicit attachments use several saved material slots
- **THEN** their Mesh, Skin, Skeleton and SoftBody products preserve the slot references, original display attributes, normal policy and recipe ownership without introducing another importer or build target
