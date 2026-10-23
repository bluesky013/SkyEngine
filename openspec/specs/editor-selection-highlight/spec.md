# editor-selection-highlight Specification

## Purpose
Define editor selection silhouettes, subdued occluded feedback, fallback visuals, and bounded generic rendering inputs without asset or material mutation.

## Requirements


### Requirement: Editor selection feedback
The editor SHALL display selection feedback for live selected Actors regardless of whether selection originated in the viewport or outliner. Feedback SHALL use the editor theme and SHALL NOT mutate world data, materials or undo history.

#### Scenario: Select and clear
- **WHEN** the selection changes to a supported static mesh and is subsequently cleared
- **THEN** the corresponding mesh receives a screen-space outline and that outline disappears after clearing

#### Scenario: Document and play transitions
- **WHEN** the document changes, the Actor is deleted, or the editor enters play mode
- **THEN** obsolete editor selection feedback is removed and reused renderer identities do not inherit it

### Requirement: Depth-aware silhouette
Selected supported geometry SHALL produce a union silhouette with stronger visible and subdued occluded feedback. The renderer SHALL reuse the existing scene depth, redraw only selected geometry for the mask, and skip all highlight passes for an empty selection.

#### Scenario: Occlusion
- **WHEN** an unselected opaque mesh partially obscures a selected mesh
- **THEN** visible edges use the normal selection color and occluded edges use the configured lower opacity

#### Scenario: No selection cost
- **WHEN** no eligible render entities are selected
- **THEN** no highlight mask, highlight depth target or outline composition pass is generated

### Requirement: Fallback and ownership
Unsupported and non-mesh selections SHALL use available projected bounds or a pivot marker; selected overlay proxies SHALL use owner-associated feedback. Generic rendering interfaces SHALL NOT depend on editor selection types. Geometry query support alone SHALL NOT be interpreted as a renderable silhouette contract.

#### Scenario: Custom query without render geometry
- **WHEN** a selected custom geometry entry supplies bounds but no renderable mesh
- **THEN** the editor displays bounds feedback without trying to decode or render its private BVH

### Requirement: Bounded validated rendering inputs
Highlight input identities, counts and finite style values SHALL be validated. Accepted frame data and shader objects SHALL remain owned until normal renderer retirement.

#### Scenario: Malformed input
- **WHEN** highlight inputs contain invalid identities, non-finite colors or an unsupported width
- **THEN** an explicit validation error is returned before recording highlight draws
