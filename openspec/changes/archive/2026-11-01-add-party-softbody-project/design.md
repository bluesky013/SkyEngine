# Ownership and execution

- Project `native/source`: module lifecycle, input, fixed-step gameplay, two characters, arena, reset and automatic demonstration.
- Project algorithm code: seven-particle cage, bounded distance/volume restoration, collision projection and six-axis displacement. Keep it independent of gameplay and rendering so a later deformable module can adopt it after its contract is validated.
- Existing Physics/Jolt: movement, rigid collisions, character contacts and grounded state. No second rigid world implementation or native Jolt pointer escapes.
- Existing Aurora adaptor: `IRuntimeSceneSource` supplies owned commands to the existing launcher packet. No new Aurora target, module entry or renderer.

The project registers its source after initialization and unregisters before shutdown destroys its state. Calls are synchronous on the main thread. Packets contain owned data only; GPU work does not call the source. Module configuration keeps physics and Aurora alive until project shutdown. BUSY retries the same packet without asking the source to append it again.

The presentation path evaluates the recovered six-direction displacement field on the imported original character mesh on the CPU. Updated positions and bounds travel with immutable mesh versions through the existing resource uploader; Lit derives geometric normals and previous versions are released when replaced. This uses ordinary material and shadow passes. A GPU deformation stream is a later optimization, not part of prototype acceptance.

The reconstructed topology and defaults are reference evidence, not directly interchangeable stiffness/compliance values. The prototype uses explicit fixed-step parameters and records deviations, including its controlled anchor, spring/volume approximation and one-way collision proxies. Landing enhancement scales cage vertical velocity only. Reset and posture transitions rebuild reference state.

Verification covers standalone solver stability and recovery, impact direction, repeated reset, bounded planar knockback and fixed stepping, runtime source ownership/backpressure/failure, the real Aurora runtime target, project launcher/module build and a bounded native rendering run.

## Collision coordinates and tuning

The project descriptor names `assets/worlds/party_arena.world`. Framework World loads and builds its six static box actors, two directional lights, and Physics subsystem. The project resolves IPhysicsSystem through PHYSICS_SYSTEM_NAME and creates character/probe handles in that subsystem's IPhysicsWorld. World owns the physics runtime and drives it through World::Tick; the project never steps or destroys that runtime directly. Gravity and fixed timestep are read from the authored World configuration. RenderSceneBridge consumes both authored actors and dynamically created character actors, allocating all render identities and tracking immutable mesh versions. Project-owned ISceneResourceSource supplies deformation meshes and colored materials.

## Original character import

Project tools statically decode the verified NeoX 5.0.4 skinned layout. Original native reader evidence establishes 20-bit signed fixed-point coordinates per axis in an 8-byte word, byte normals, delta-coded uint16 indices, half UVs and normalized uint16 skin weights. Geometry is checked against independent GIM bounds and face/normal orientation, then exported to standard OBJ. Raw bone records, four influences and both UV layers remain in reference JSON. Unsupported variants are rejected.

Runtime loads the converter's triangulated OBJ position/index subset into the existing project resource provider, shifting feet-origin vertices into MeshCenter space before the same cage deformation. It does not import cook code or create an engine format target. Missing original textures mean the existing tinted Lit material remains; original ACL animations and inverse-bind conversion are not claimed. Separate prototype eye actors are removed. The original mesh stays in its authored bind pose while the controller and visual cage drive motion.

NORMAL uses the evidence fallback capsule (radius .45, cylinder .95); ROLL uses radius .7506. Feet are derived from the main collision center. Cage reference centers are (0,1,.015)/(0,1.2,.015), and the separate .834-radius trigger sphere follows feet+(0,1.2,0) or feet+(0,.634,0). Its Jolt layer is excluded from main-body responses. Explicit overlap queries drive approximate character cage contact planes; static box planes derive from the same World transforms as their colliders. This does not claim to recover the native extra-test-body coupling.

Recovered gravity -23 and normal/roll movement tables are used with a simple project controller. Native jump stage semantics are unconfirmed: the active prototype uses a 1.5-unit ballistic jump, retaining the recovered 23/-90/-10 values only as reference. Full-pack inspection includes 45,197 base scripts and 120 patch overrides. PhysicMgr initializes normal bounciness .1 and mass 20; Fling2MoveStatus selects bounciness .65 and mass scale 2.5, restoring them on exit. The native extra-body arguments (20,10,0,0,0,4) are not coefficients.

The project applies a planar two-body restitution response at main-capsule contact, replacing incoming normal motion instead of adding an opposing push over retained input. A pair is rearmed only after separation; .08/.10 contact margins accommodate Jolt virtual-character padding and avoid resting-contact reinjection. Mass ratios conserve planar momentum and e<=1 prevents collision-only energy growth. Using the larger state restitution, the planar restriction and contact margins are documented project approximations; FIXED_BOUNCINESS combining rules and native lose-control behavior remain unimplemented. Resolved displacement drives cage inertia and diagnostics only.

## First control-state implementation

The first implementation separates drive, current planar velocity, the submitted pre-physics velocity snapshot and the post-physics punch vector. Jolt CharacterState exposes transforms only, so displacement estimates the resolved velocity. Pair restitution replaces the planar result before punch detection, avoiding double-counted impulse feedback. Grounded punches remove the vertical component; airborne snapshots account for gravity and one-shot jump input.

The native notification threshold is 2; normal business control loss is strictly above 9, rolling includes 9, and stronger hits exceed 10.5. Light hits retain control. Strong hits clear drive and ignore movement/jump input, preserving resolved motion. The project uses the recovered .3-second no-damping interval, .2 linear damping and friction parameter 2. Its friction interpretation (mu times gravity), minimum 1.5-second loss, grounded speed gate of 5, and .35-second recovery blend are explicit approximations. Angular motion, native recovery condition composition and original stand-up animation are not implemented; the two strong-hit classes share upright recovery.

On recovery, current velocity is handed to drive and the previous-velocity snapshot, applying the source's conditional rolling continuity idea consistently in this prototype. The old .25-second normal-velocity timer is removed. Active probe contact instead constrains inward motor acceleration while permitting escape and tangent steering; separation releases this constraint. This contact rule and the probe-triggered standard pair response remain adaptations, not the recovered native extra-body solver.

Keys 1/2 reset into normal light contact and rolling heavy impact respectively. Both hold inward input through contact, then release input without reversing it; each replay repeats after eight seconds. Smoke runs alternate these modes. The HUD exposes each character's control state, last punch strength and speed. Numeric tests cover thresholds, contact release, strong-hit input suppression, recovery continuity, ground projection, airborne gating and retrigger during recovery at 60/120 Hz.

## Shadow and runtime debug display

Lit computes a receiver depth gradient before sampling branches, compares at each shadow texel center and retains a small depth epsilon. A 3x3 PCF kernel smooths the hard nearest shadow edge. The sampler clamps at map boundaries. Native pixel-readback tests compare self-casting sloped receivers with noncasting baselines, plus the existing real occlusion/material-shadow checks.

Project lighting enables the existing HDR/tone-map path. Its HUD uses Aurora's existing UI feature with an owned Feature instance and process-resident PartySoftBodyModule code; owned font texture and mesh/line snapshots do not retain gameplay objects. The Windows prototype rasterizes a local system font via GDI; there is no editor dependency or settings form. F1 cycles cage/body/probe/all, F2 toggles shadows, P/F3 pause/single-step. The title contains only the project name.


## Close-up showcase and live tuning

The authored World contains a presentation floor, fixed wall and two lights. Keys 1/2/4 move dynamic participants and the camera between pair, wall/slide and landing stations. Rolling is omitted from this presentation. The inactive second character is parked outside the single-player view and excluded from gameplay updates. Pair geometry rotates the source +Z front to opposing +/-X before world-aligned cage deformation; original face textures remain unavailable.

Defaults are speed 3.2 and restitution .08. The automatic approach stops after the first pair contact, allowing response and restoration to remain visible. A softer cage uses recovery 30, spring multiplier .04, damping 4.5 and volume strength .10 independently of restitution. Landing scales cage Y velocity by 1.41 without giving the main body a bounce. Gravity/drop-height edits apply on replay; gravity initially comes from World and is supplied to recreated Jolt characters.

LabSettings reflection has one owner in PartySoftBodyModule::RegisterMeta. A separate project tool module PartySoftBodyTools consumes IPartyLabPanel and binds ReflectedFormView with its own CommandService, PropertyEditorRegistry and notifier. Runtime/algorithm targets do not depend on editor widgets. The tool links existing EditorWidgets/EditorRender/UI.FreeType targets; no production source has a second owner, and no Aurora target/ownership is introduced. The project enables existing Sandbox widget targets and bundles the existing OpenSans font. Settings are session-local.

The user requested 2048x2048 directional shadow maps. The existing D32/3x3 PCF path retains its depth comparison and changes image/raster extent together. Scene-bounds projection and wide coverage remain limitations; no CSM implementation is claimed.


### Tuning observability and demonstration braking

Native edit logs established that restitution .08/.33/0/.5 produced outgoing speeds .256/1.056/0/1.6 at an approach speed of 3.2. The original normal motor deceleration of 40 quickly erased this difference after contact release. Automatic demonstrations now expose their own braking value (default 4), while manual motor tuning remains unchanged. This is a presentation choice, not a recovered native parameter. Last-impact restitution and actual outgoing speed remain visible in the HUD. The panel labels the collision timing, and gravity/drop height retain replay labels. No fixed immunity/protection timer is reintroduced.


### Static-wall compression follow-up

The initial wall replay kept pushing normally into the wall. The .45 primary capsule stopped about .47 from the surface, but the side cage particle rests at radius 1. Hard contact projection therefore caused about .53 of persistent side compression, overwhelming stiffness edits. Pair restitution did not apply to walls.

A project-owned extra-probe support approximation now clips planar wall approach at `probeRadius - wallCompression` (.834-.12=.714 by default), using authored box transforms. It preserves tangential displacement, vertical movement and outward escape. Current contact normals also constrain motor acceleration, preventing repeated inward drive/punch accumulation. Floors/sloped contacts remain under the character backend; pair collision logic and the original primary capsule dimensions are unchanged. Wall squeeze is replay-applied. This is not asserted to reproduce the unresolved native extra-body solver.

For World2 birds, the cage's canonical unit dimensions are mapped through each abdomen probe center/radius. Contact offsets and acceleration are converted into cage units; cage displacement returns to mesh space before driving rig targets. Using the old unit cage directly with the goose's .608065 abdomen radius caused disproportionate wall compression. Static wall support also considers the existing head/neck spheres at their rest-frame centers, capped to `min(replayWallCompression, radius * .45)` so simulated neck bending cannot permit indefinite motor creep into a wall. Tangential motion, outward escape and vertical backend motion remain available.

Rig contact projection does not synthesize an outward launch velocity from depenetration. At an active plane it preserves the integrated outward normal speed, removes inward normal speed, and retains tangential constraint response. Goose default neck stiffness is 70/62/56/52/52/56/60, inertia .12/.16/.20/.24/.28/.30/.32 and damping scale 1.35; head values are 62/.32/1.35. This replaces the exaggerated jelly profile while retaining the same two upper probes and live inertia scaling. The wall replay approaches, slides, retreats and settles; its camera is centered on contact and closer to the model.

### Window resize and scene resource subscriptions

The runtime previously refreshed resource subscriptions only for scenes with available viewports. During swapchain recreation, moving entities advanced their binding revisions while their viewport was temporarily unavailable. Catalog refresh invalidated their old subscribers and pruned their unchanged materials. The project bridge had already published those material payloads, so the moving meshes continued uploading without a drawable material. Static geometry and the independently rendered debug overlay remained visible.

Resource subscription refresh now includes changed live scenes, independent of output availability. Preparation and GPU output scheduling still skip unavailable viewports. Camera aspect and viewport resize retain their existing update path; no scene reset, repeated asset publication or projection workaround is introduced.

### World2 imported model spring prototype

World2 is a separate authored World selected with `--world2` / `RunWorld2.ps1`. Its project-owned `partyLab.models` metadata lists penguin, goose and peacock rig assets in that order. The existing runtime creates characters dynamically; 5/6/7 select an asset and reset the current pair, wall or landing showcase. Existing defaultScene behavior remains available.

The offline GLB importer validates inverse-bind reconstruction, normalizes visual height to 2.3, and retains original four-weight influences. Controls follow weighted influence centroids, with bone origins for unweighted nodes; this separates the peacock fan regions even when their joint origins coincide. Parent links supply structural springs, grandparent links restrict bending, and bilateral/fan cross-links support width. The model profile stores estimated torso capsule/probe dimensions. Source GLBs and SHA-256 identifiers remain in the project.

`RigSolver` extends the existing project Algorithm target without renderer, editor or backend dependencies. Runtime `RigAsset` parses validated profiles. The seven-point torso cage supplies compression targets; each rig integrates restoring forces/inertia/damping and projects distance, offset and contact constraints. Original weights blend control displacements into the visible mesh. This is translation-based surface deformation, not a complete rotational SoftBone/animation implementation or limb rigid-body simulation. The existing reflected settings drive both systems; UI only binds the shared model.

Space and WASD now stop automatic replay when gameplay receives them, making jump/movement accessible without a prior F toggle. Focused property editors still consume keyboard input; paused simulation remains paused. Jump requires ground support and controlled state.

Peacock adds an optional second probe following its head-volume control. Native inspection and influence-region statistics show that this asset weights the head volume to `neck` (node 5), while `head` predominantly influences the crest base. The importer therefore fits the head sphere to node 5's influence region. Its independent trigger follows simulated control displacement; static-box and other-character sphere contacts become node-specific RigContact planes. Head/head penetration is shared, and all queries use body-transform snapshots from before either rig step. No head contact generates an additional character impulse. Model reset destroys/recreates optional probes; debug renders abdomen purple and head pink. This approximates local soft contact and does not provide a second rigid character collider or rotational SoftBone simulation.

World2 now also references the original Party OBJ through `partyLab.partyModel`. Key 8 selects that mesh and reuses the original seven-point deformation path, body dimensions and abdomen probe; 5/6/7 retain bird selection and penguin remains the startup default. Reset clears obsolete rig state and recreates optional probes. All four models share showcases and settings. Smoke cycling covers four models and returns to penguin.

Penguin and goose also receive fitted head probes. Their visible head regions span neck/head and neck_07/head respectively, so the fit combines those skin influences and stores a center offset relative to the head control. The runtime rotates that offset with model facing and subtracts it when converting sphere contact planes into node constraints. Peacock's corrected fit is unchanged. This extends existing asset data and solver integration without new targets or an alternate collision solver.

The goose profile is subsequently calibrated to its abdomen rather than its full silhouette: a combined hips/spine_01/spine_02 influence centroid defines the horizontal asset origin and body/probe height. The importer translates mesh and all controls together, preserving weights, links and head-follow offsets. The capsule radius remains .428065; lowering its center from 1.013443 to .799225 shortens its cylinder to .742320, keeping the floor at zero. The head sphere is reduced by 15% to .249735. Penguin, peacock and Party profiles are unchanged.

Goose uses one gold local sphere on neck_03 plus its existing pink head sphere; the abdomen sphere is separate. The initial six-neck-sphere trial was reduced at the user's request because its dense contacts produced an undesirable appearance. The existing node-contact path handles head and neck probes against authored boxes and other characters' abdomen/head/neck probes. It ignores the same character, uses pre-rig trigger snapshots, shares penetration between flexible probes, and retains a .003 contact margin. Reset destroys every old neck trigger before creating the selected profile. Main character impulses still originate only from the main/abdomen contact path.

Weak movement sway was a response-tuning limitation: all neck nodes had stiffness 75, inertia .20 and full damping. Goose now has per-node restoring stiffness 24..38, inertia .45..1.10 along the neck, and damping scale .65, with matching head response. Existing distance/offset constraints remain active. LabSettings.rigInertia is a reflected, undoable live multiplier with default 1; other models retain their previous per-node defaults. No scripted sinusoid is added: acceleration excites sway, then steady movement allows it to decay. This remains weighted control displacement, not full rotating-bone SoftBone.
