## ADDED Requirements

### Requirement: Engine animation integration remains independently selectable

The build SHALL declare SKY_BUILD_ANIMATION as an engine option defaulting to ON independently of plugins/plugins.json. The Animation algorithm library SHALL contain both the existing algorithms and pure Graph definition/evaluation code under engine/animation/core, depend only on Core and optional ACL, and remain available when this option is OFF. Its tests SHALL be unified in AnimationTest. Separate Animation.Graph.Static and Animation.GraphTest targets SHALL be removed. When ON, engine/animation SHALL provide separate GraphAssets (under assets) and World targets; Builder SHALL additionally require SKY_DEVELOP and Editor SHALL additionally require SKY_DEVELOP and SKY_BUILD_SANDBOX. Integration targets SHALL be configured after their engine dependencies and before host dependency binding. Each production source SHALL retain exactly one compilation owner.

#### Scenario: Animation integration enabled
- **WHEN** animation integration, developer facilities and Sandbox are enabled
- **THEN** Animation.GraphAssets, Animation.World, Animation.Builder and Animation.Editor implementation/module/test targets SHALL be configured from engine/animation with their existing names and public includes

#### Scenario: Algorithm without integration
- **WHEN** SKY_BUILD_ANIMATION is OFF
- **THEN** Animation SHALL include pure Graph support and remain buildable with AnimationTest, while GraphAssets, World, Builder and Editor integration targets SHALL be absent

#### Scenario: Engine option survives plugin manifest changes
- **WHEN** a build is configured with SKY_BUILD_ANIMATION=OFF and the plugin manifest subsequently changes
- **THEN** the explicit engine option SHALL remain OFF and ModuleManager::GetBuildOption SHALL report false for SKY_BUILD_ANIMATION rather than an unknown option

#### Scenario: Module packaging remains stable
- **WHEN** an enabled animation module is packaged statically or dynamically
- **THEN** its module name, metadata owner and runtime dependencies SHALL remain unchanged without an animation plugin registration
