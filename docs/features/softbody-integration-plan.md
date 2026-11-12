---
title: "Multi-Provider Deformable Simulation Integration Plan"
description: "SkyEngine integration contracts for coexisting physics rigs, cloth, strands, volumes and clusters, explicit coupling domains, assets, rendering, mobile budgets and acceptance."
module: "deformable"
updated: "2026-10-22"
---

## Scope and decision

The requested outcome is **multiple soft-body technologies coexisting, selected by scene requirements**.
The engine should host physics-driven skinned rigs, cloth, strands, volumetric elastic bodies and cluster-based secondary motion
without pretending they have one topology, material model or native solver. Character flesh/jiggle is
a use case of these families. Fluid, granular, MPM and IPC integration are later specialist extensions,
not implied first-release capabilities.

Adopt the heterogeneous host architecture S2 in the
[technology and architecture comparison](softbody-technology-comparison.md). Native Jolt/Bullet worlds
are one provider route; rigid-body/constraint rigs can reuse the current rigid world, and standalone CPU
XPBD or shape matching can serve other families. Implement only
the initially selected providers, while validating at least two distinct families together. Do not
implement a universal particle engine or automatic coupling between every provider as a prerequisite.

This is a proposal, inspected against HEAD `861a4eb6` and the working tree on 2026-10-22. All newly named
interfaces, targets, schemas and settings below are proposed, not current APIs or approved module changes.
Existing unrelated changes were present. This work changes documentation only; no runtime implementation,
native build, performance measurement, OpenSpec change, commit or archive is included.

The repository requires confirmation before implementing module/target/loading changes and before
repairing third-party paths that actually rely on exceptions. The decision list in section 13 makes those
choices reviewable without treating this document as that approval.

## 1. Repository evidence and consequences

| Existing source | Observed state | Required integration consequence |
|---|---|---|
| [IPhysicsBackend.h](../../engine/physics/include/physics/IPhysicsBackend.h) | Rigid bodies, constraints, characters, queries and snapshots; no deformable contract | Add explicit provider/capability interfaces; do not overload rigid transforms as vertex state |
| [PhysicsConstraint.h](../../engine/physics/include/physics/PhysicsConstraint.h), [JoltConstraints.cpp](../../plugins/jolt/adaptor/src/backend/JoltConstraints.cpp) | Jolt maps fixed/hinge/slider/distance/6DOF, limits and explicit motor modes; no spring/compliance descriptor is exposed | Add explicit compliant-joint settings before claiming compressible rigs; authored Distance now preserves its length |
| [AnimationPose.h](../../engine/animation/core/include/animation/core/AnimationPose.h) | AnimPose and ToSkinRenderData exist | Reuse pose concepts, but validate physics-to-bone conversion and the complete Aurora skinning path; the API alone does not prove render integration |
| [PhysicsBackendRegistry.cpp](../../engine/physics/src/PhysicsBackendRegistry.cpp) | One active backend; Register shuts down the previous backend | Reject conflicting native backend selection before registration; no live replacement |
| [PhysicsSystem.cpp](../../plugins/jolt/adaptor/src/PhysicsSystem.cpp) | Tick and SingleStep share completion and event publication | A future deformable coordinator still needs explicit drive ownership |
| [PhysicsBodyComponent.cpp](../../plugins/jolt/adaptor/src/components/PhysicsBodyComponent.cpp) | Resolves engine IPhysicsSystem; preserves serialized PhysicsBody identity | Reuse the engine interface and existing component contract |
| [JoltModule.cpp](../../plugins/jolt/adaptor/JoltModule.cpp) | One metadata owner, validated world config, owned SDK/world state and process-resident code | Keep deformable registration independent of rigid metadata |
| [Findjolt.cmake](../../cmake/thirdparty/Findjolt.cmake), [thirdparty.json](../../cmake/thirdparty.json) | Jolt v5.6.0 is pinned; Bullet plugin/dependency have been removed | This migration enables rigid physics only; native deformable capability still needs separate implementation and verification |
| [World.cpp](../../engine/framework/src/world/World.cpp), [World.h](../../engine/framework/include/framework/world/World.h) | Actors tick before subsystems; subsystem storage is unordered | Subsystem registration order cannot establish rigid/deformable/animation scheduling |
| [PhysicsContract.cpp](../../engine/physics/src/PhysicsContract.cpp) | Fixed-step catch-up clamps accumulated time | Keep one time policy, report dropped time, and distinguish catch-up from solver subdivisions |
| [PhysicsSnapshot.h](../../engine/physics/include/physics/PhysicsSnapshot.h) | Stores rigid transform/velocity/sleep state only | Visual vertex snapshots do not automatically provide restart, rollback or exact replay |
| [RenderSceneBridge.cpp](../../engine/aurora/adaptor/src/renderer/RenderSceneBridge.cpp) | Extracts known static mesh/light components | Add an engine-side extraction contribution interface; do not include provider-plugin components here |
| [RenderCommands.h](../../engine/aurora/core/include/aurora/renderer/RenderCommands.h) | Owned FIFO packets, exact resource keys, release commands and retained Feature instances | Deformation joins the existing host packet; no independent submitter/event consumer |
| [RenderResourceTypes.h](../../engine/aurora/core/include/aurora/renderer/RenderResourceTypes.h) | Mesh payloads and bounds are versioned and owned | First presentation path can publish bounded full mesh revisions; streaming optimization is separate |
| [AssetBuilder.h](../../engine/framework/include/framework/asset/AssetBuilder.h), [asset pipeline](asset-pipeline.md) | Product loading and reflected per-target cook settings exist | Package provider-specific products; use generic reflected forms for authoring |
| [platform support](../architecture/platform-support.md), [Aurora rules](../../engine/aurora/AGENTS.md) | Android Vulkan and iOS Metal; packaged GPU acceptance is separate work | Native compilation alone cannot accept mobile deformables |

## 2. Architecture and coexistence

The common host owns identity, selection, lifecycle, scheduling, budget accounting and immutable outputs.
Family interfaces own topology/material meaning. Providers own algorithms and native objects. A scene
contains several **simulation domains**: each domain has one provider, execution owner and interaction
policy. Two objects using the same provider need not share a solve domain.

```mermaid
flowchart TD
    Content[Scene assets and family configuration] --> Host[Deformable host and provider selection]
    Host --> Native[Native volume domain: Jolt (planned)]
    Host --> Rig[Physics rig: rigid bodies and compliant joints]
    Host --> Cloth[Standalone cloth domain]
    Host --> Strand[Standalone strand domain]
    Host --> Cluster[Cluster or local flesh domain]
    Physics[Selected rigid world] --> Native
    Physics --> Rig
    Physics --> Proxies[Owned rigid collision snapshots]
    Proxies --> Cloth
    Proxies --> Strand
    Proxies --> Cluster
    Native --> State[Completed immutable deformation samples]
    Rig --> State
    Cloth --> State
    Strand --> State
    Cluster --> State
    State --> Bridge[Main-thread deformation presentation bridge]
    Bridge --> Renderer[Existing Aurora host packet and resource lifetime]
```

This diagram shows data flow, not universal contact edges. Native volumes and independent cloth can be
visible in the same scene while remaining collision-isolated. A strand can collide against rigid proxies
without feeding forces back. Every required interaction must be validated before scene activation.

### Interaction modes

| Mode | Contract | Initial availability |
|---|---|---|
| None | Objects do not influence each other's simulation | Explicitly available |
| OneWayProxy | Consumer reads owned geometry/pose/velocity samples; producer receives no impulse | Implement for selected shape families; static/kinematic primitives first |
| NativeCoupled | Provider's rigid rig or native deformables participate in the same physics-world update | Only for matching native-world bindings and verified contact pairs; identify rigid-proxy versus deformable-surface contact |
| ExplicitCoupled | A dedicated joint solve or validated co-simulation exchanges impulses/state | Reserved; unsupported until separately implemented and tested |

Validate interaction capability per **pair**, including family, shape class, thickness, CCD and friction.
Never infer it from a global `supportsSoftBody` boolean. One-way domain graphs must be acyclic for
same-tick evaluation. Reject a cycle unless a separately specified delayed-sampling policy is chosen.
Changing a provider never silently changes an object's required interaction mode.

The global physics registry uses the Jolt rigid backend. Native-coupled deformable providers must use
the same world and selected native backend. A separate proxy domain is one-way unless an explicit
coupling contract is implemented. Bullet remains comparison evidence, not a supported build alternative.

### Scene assemblies and selection policy

| Scene requirement | Proposed combination | Required edges and exclusions | Selection consequence |
|---|---|---|---|
| Mobile character with a cape and hair guides | Rigid character colliders + cloth XPBD domain + rod/strand XPBD domain; optional local clusters | Rigid/animation targets feed cosmetic domains one way; cape-hair contact is absent unless separately supported | Select cloth and strand providers independently; animation is an authored cosmetic fallback |
| Coarse squeezable toy/cushion with rigid interaction | Several rigid bodies, compliant joints and weighted control bones in the selected rigid world | Native rigid-proxy contacts; permitted compression DOFs and bounded surface/collider mismatch | Evaluate this route before a volume solver; no native soft-body dependency is required |
| Interactive elastic props beside that character | Add a future native Jolt volume domain matching the rigid backend | Props and rigid objects interact bidirectionally; independent cape/hair domains do not automatically contact props | A native provider supplies the coupled slice without replacing all cosmetic providers |
| A pile requiring deformable-surface contact | A provider/domain with verified soft-soft contact, optionally beside the same cosmetic domains | Soft-surface contact is required, so an incapable provider cannot be selected; rigid-proxy contact is a distinct approximation | Jolt v5.6.0 soft bodies fail this requirement; a rigid-rig pile remains possible if its approximation is accepted |
| Character flesh or a visibly deforming tire | Animation-driven volume/cluster deformation plus the existing gameplay collision representation | Declare whether deformation is cosmetic; visual tire bulging does not establish deformable tire-road mechanics | Low-cost clusters/cages suit a visual requirement; measured material or contact behavior requires a different provider profile |
| High-end rubber/tissue or snow/mud effect | Dedicated constitutive volume solver or MPM specialist, respectively | Specialist material/contact contracts and independent budget; cross-provider exchange remains explicit | Optional platform/content tier with separately cooked alternatives, not an automatic replacement for cloth XPBD |

For example, one accepted scene could select a future Jolt native volume provider for props, a standalone
cloth provider for a cape, and a standalone rod provider for hair guides. They share identity, scheduling,
asset loading and presentation services while retaining three solver/material contracts. These are
proposed assemblies, not claims that these SkyEngine providers already exist.

Resolve selection at scene activation, in this order:

1. The asset declares family, material model, units, required interactions and gameplay/cosmetic role.
2. The project/platform profile supplies an ordered candidate list and a quality budget for each family.
3. Reject candidates without compatible cooked products, platform support, native-world ownership or
   required pairwise capabilities. Validate the complete scene graph, not just each object separately.
4. Record the chosen provider revision, product and quality profile in diagnostics. If no candidate
   qualifies, fail activation for required content; use only an explicitly authored cosmetic fallback.
5. Keep that choice stable while the domain is active. A thermal LOD transition uses a validated variant
   and transfer/reset policy; it is not permission to replace an XPBD material with FEM mid-solve.

Use the same policy in authoring preview, packaged runtime and benchmarks. Avoid different hidden
provider defaults in the editor and on device.

### Rigid-rig integration path

Treat a physics rig as its own family, with a body/constraint graph and bone bindings. It is not a
tetrahedral asset, and it is not the same algorithm as particle-cluster shape matching. Its provider
creates bodies/joints through engine physics contracts, retains their world-scoped identities, and
participates as NativeManaged in the existing rigid step. Never simulate those bodies a second time.

First complete and test constraint limit/drive semantics and an explicit spring/damping/rest-target
contract. Map only supported settings to Jolt; reject unsupported ones rather than leaving the
descriptor accepted but ineffective. Extending the current rigid world for this path does not require
native soft-body objects or a new rigid backend. Changes to existing constraint behavior
need their own compatibility tests. No such runtime changes are made by this document.

At fixed-tick start, animation may supply kinematic targets or bounded compliant drive targets. After
the rigid solve, extract owned transforms, apply authored body-to-bone offsets and convert to the
skeleton's local hierarchy. The constraint graph can differ from the bone hierarchy. Specify the
authority of each bone: animated, simulated, or a defined blend. Do not overwrite a simulated body's
transform from the final blended render pose every frame. Unanchored props need a consistent root/frame
policy so physics motion is not also applied through the actor twice.

Keep rig schema/selection in the deformable plugin's core/runtime and pose conversion in its adaptor;
do not put a skeleton dependency into Physics or Aurora core. Use an animation-consumer interface for
pose contributions and a separate renderer-consumer presentation contract. These interface additions
are proposed review items, not an already working physics-animation bridge.

## 3. Ownership, modules and dependencies

New names in this table are proposed ownership, not a request to create every target in the first phase.

| Proposed location | Responsibility | Allowed direction |
|---|---|---|
| `engine/deformable` | Domain/family interfaces and data used in their signatures; pure validation helpers | Core and necessary Framework/engine interfaces; no concrete solver or serialized asset implementation |
| `plugins/deformable/core` | Provider registry implementation, selection, budgets, asset schema and binding algorithms | Core plus required engine interfaces; no renderer/editor/builder dependency |
| `plugins/deformable/runtime` | World components, asset loading, coordinator integration and module registration | Own core, Framework and engine interfaces |
| `plugins/deformable/adaptor` | Converts completed samples into Aurora-facing presentation data | Own core, Framework and existing Aurora adaptor interfaces |
| `plugins/deformable/builder` | Source parsing, validation, offline binding and product generation | Own core, Framework asset-build interfaces and selected offline libraries |
| `plugins/deformable/editor` | Authoring/debug integration through Sandbox editor framework | Own core, engine contracts and editor framework; never a runtime dependency |
| `plugins/jolt/adaptor` and a future backend-owned deformable bridge | Existing Jolt rigid backend; optional native deformable provider | Physics/deformable interfaces plus pinned Jolt; keep the existing adaptor + render layout |
| `plugins/deformable_xpbd` | Optional standalone cloth/strand/volume kernels | Engine deformable contracts and Core; native physics access only through the coupling contract |
| Optional specialist provider plugins | Shape matching, FEM/VBD, MPM or IPC implementations | Same explicit domain contracts; family-specific products and budgets |

Illustrative target names are `Deformable.Static`, `Deformable.Runtime`, `Deformable.Adaptor`,
`Deformable.Builder`, `Deformable.Editor`, and `DeformableXPBD`. Final naming and loading are a review
decision. Do not create `plugins/aurora`, a second renderer, or new Aurora submodule ownership.

The renderer consumes deformation, so a proposed `ISceneDeformationSource` and its presentation data
belong to the existing **Aurora adaptor interface surface**. The deformable adaptor implements it.
Likewise, deformable solvers consume rigid collision information, so proposed `IRigidCouplingProvider`
belongs to **engine/deformable**, and backend-owned bridges implement it. No concrete Bullet/Jolt type
crosses either boundary.

Register factories through a shared interface service. Its concrete cross-DLL registry uses
`sky::Singleton<T>`; shared reflection has a single metadata owner whose process definition registers it once. Consumers do not reflect again. Registration of live provider
instances must still be reversible. The loader keeps defining DLLs resident until process exit; no per-object
code token is required. Factories, callbacks and outstanding jobs retain their actual provider/data owners,
and shutdown withdraws services and drains work before destroying those objects.

The Jolt rigid migration separately established adaptor + render ownership, interface-based subsystem
lookup and preserved serialized component identity. Deformables reuse that boundary; they do not
create another rigid core target or keep a retired backend loaded to obtain components.

## 4. Proposed public contracts

The following are API sketches. Use explicit status/error values; the repository's existing
`core::Result` pair does not by itself define this richer error contract.

| Contract | Required semantics |
|---|---|
| `ProviderDescriptor` | Stable provider ID, revision, supported families/material models/product formats, platform support, execution mode and limits |
| `IDeformableProviderRegistry` | Enumerate/select factories; reject duplicate IDs; retain provider instances; no implicit backend switching |
| `IDeformableSystem` | Own domains per world, validate scene interaction graph, expose completed states and explicit failures |
| `IDeformableDomain` | Common lifecycle, step preparation, publication and statistics; distinguish `NativeManaged` from `Standalone` execution |
| `IPhysicsRigSimulation`, `IVolumeSimulation`, `IClothSimulation`, `IStrandSimulation`, `IClusterSimulation` | Separate creation and material/topology operations; query through explicit interface methods, never RTTI |
| `IRigidCouplingProvider` | Advertise shape/contact modes; produce immutable rigid samples or validated same-native-domain binding; no native pointer escape |
| `IAnimationTargetSource` | Sample targets at requested simulation time; provide previous/current transforms and teleport information |
| `ISceneDeformationSource` | Render-consumer contract for owned CPU geometry/bounds or a future approved deformation stream |

### Identity and capabilities

A `DeformableHandle` includes world identity, domain identity, index and generation. Attachments to rigid
bodies include a world token as well as `PhysicsObjectId`; the existing ID alone is only world-scoped.
Validate owner, generation, family and topology revision before access. Unknown ownership returns an
explicit error. No `dynamic_cast`, backend-enum-only cast, or stale pointer fallback.

Capability fields distinguish at least: rig/volume/cloth/strand/cluster models, material model IDs,
static/kinematic/dynamic-rigid contact, soft-soft pairs, self-contact, deformable CCD, query support,
one-way/two-way attachments, state restart, exact replay and native execution ownership. Keep unsupported
fields false. A rig advertises rigid-proxy contact, not continuous skinned-surface contact.
Jolt v5.6.0 soft-soft is unavailable in the inspected path; see the
[versioned comparison](softbody-technology-comparison.md). Reference-library support is not automatically
support of the SkyEngine adapter.

### Creation, commands and state

Creation validates the entire description and selected provider product before publishing a handle.
Immutable rest data may be shared; particle positions, velocities, multipliers and contacts are per
instance. Queue mutation commands for a fixed-step boundary: attach/detach, target changes, impulses,
sleep/wake, teleport/reset, quality changes and destruction. Commands have sequence/epoch identity and
exactly one completion outcome. Do not mutate a solver during its worker jobs.

A `DeformationSample` owns or retains: handle, family, topology/product revision, simulation tick/time,
coordinate-frame transform, previous/current positions, bounds, status and reset/teleport flags.
Strands additionally need orientation/twist data when the model uses it. Cluster output can contain
cluster transforms with a distinct schema; it need not masquerade as a tetrahedral mesh.
Rig samples instead retain body/control-bone transforms, binding identity and a pose/frame epoch;
particle arrays are not mandatory for this family.

Runtime queries, render snapshots and restart snapshots are different contracts. Borrowed SDK vertex
arrays never become frame payloads. A bounded reference-counted sample pool must account for all readers;
two buffers are insufficient when more than two generations remain borrowed. Pool exhaustion triggers
documented presentation backpressure, not mutation of a borrowed sample.

Useful error categories: `UnsupportedFamily`, `UnsupportedInteraction`, `ProviderUnavailable`,
`WrongWorld`, `StaleHandle`, `InvalidTopology`, `InvalidMaterial`, `VersionMismatch`, `BudgetExceeded`,
`Busy`, `Cancelled` and `NativeFailure`. Include provider/object context in diagnostics.

## 5. Fixed-step execution and ownership

Use one host-owned advancement point **after World::Tick and before render extraction**. Add a narrow,
explicit manual-drive ownership contract to the physics subsystem when the deformable coordinator is
enabled. Its ordinary Tick must not also advance physics. Deformable subsystem Tick collects commands;
it does not independently race the coordinator. Registration order in the unordered subsystem map is
not a scheduling solution.

The new call/drive-mode contracts are implementation work. Do not describe them as already available.
When deformables are disabled, the original physics driving path remains valid. Ownership acquisition,
PIE transitions and release are explicit; a second owner fails before advancing any simulation.

For each accepted fixed tick:

1. Validate and apply queued topology-preserving commands. Sample animation/kinematic targets at the
   fixed simulation time, including previous targets needed for velocity. Render-frame poses alone do
   not define multiple catch-up ticks correctly.
2. Prepare all domains. Native-managed domains enqueue work into their matching physics world. They
   must not run a separate integration call.
3. Step the selected rigid world exactly once. This also advances its native-coupled deformables.
4. Publish owned rigid start/end samples for one-way consumers. Advance standalone domains in the
   interaction graph's order; independent domains may run concurrently. Substeps interpolate collider
   motion using the declared sampling policy.
5. Join accepted work, reduce events into owned queues, and publish completed domain samples with
   tick identity. A cosmetic domain failure may retain its last valid pose with a visible diagnostic;
   a required gameplay domain failure marks the session failed/paused under host policy.
6. Interpolate presentation from completed samples and append commands to the sole renderer packet.

An opaque native step is not transactional rollback. If one domain fails after another advanced, do not
claim all domains were restored. Failures publish their status and completed tick, and the host applies
the stated session/fallback policy. All jobs and completion notifications must still be collected.

Keep three controls separate: fixed tick duration, maximum catch-up ticks, and provider-specific internal
subdivisions/passes. Existing PhysicsStepper clamping can remain the local game policy if dropped time is
reported, but must not be marketed as lockstep-safe. Jolt's `mNumIterations` maps to soft subdivisions in
the inspected implementation; Bullet counts and custom XPBD counts are not interchangeable settings.
Expose authored quality profiles plus the measured effective work rather than a misleading universal slider.

Use bounded worker admission. Start with a single-worker correctness baseline, then integrate the native
job system without a second uncontrolled all-core pool. Avoid nested waits on the same exhausted pool.
Native callbacks copy allowed contact data; they do not read concurrently changing bodies or invoke ECS
mutation. Jolt's [contact listener contract](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Jolt/Physics/SoftBody/SoftBodyContactListener.h)
explicitly warns about locking and concurrent access.

## 6. Assets, materials and cooking

Common asset identity does not mean one serialized mesh or stiffness model. Keep source/product schemas
in the owning plugin. Public engine structs contain only the data required by interface signatures.

| Family | Simulation product | Presentation binding | Authoring parameters |
|---|---|---|---|
| Physics rig | Body shapes, mass/inertia policy, joint graph, limits/springs/drives, collision filter and rest frames | Body-to-bone offsets, skeleton identity, weights and optional corrective shapes | Compression travel, rest targets, damping, collision coverage and pose authority |
| Volume | Nodes, oriented tetrahedra, boundary faces, mass/density and material regions | Tetrahedron ID and barycentric weights per render vertex | Named constitutive model or calibrated edge/volume compliance |
| Cloth | Triangle mesh, rest surface metrics, adjacency, bend data and thickness | Surface embedding with authored normal offsets where needed | Warp/weft/shear/bend model, density per area, collision thickness |
| Strand | Segments/rods, rest lengths, frames and attachment topology | Guide/follower weights and strand frames | Stretch/bend/twist model, linear density and radius |
| Cluster | Particles/clusters and rest membership/shape data | Cluster-weighted deformation | Cluster model, overlap/weights, damping and animation targets |

Material source data identifies its model and units. Jolt compliance, Bullet native coefficients and a
FEM Young's modulus/Poisson ratio are not aliases. Cook an explicit provider variant or reject an
unsupported material. Any approximate conversion records the fitting method, reference tests and error;
never label an arbitrary spring network as calibrated rubber.

The proposed cook pipeline is:

1. Import authored topology; establish metres/kilograms/seconds and orientation. Reject NaN/Inf,
   invalid indices, degenerate elements and unsupported scales with useful diagnostics.
2. Validate family-specific requirements. Volume cages require valid oriented elements and a usable
   boundary; open cloth surfaces are legitimate. Check duplicate topology, near-zero rest volume,
   conditioning and binding coverage. Near-degeneracy thresholds scale with asset units.
3. Generate rest data, mass distribution, attachments and material regions. Preserve source-to-runtime
   IDs through any provider optimization; do not expose native constraint order as stable author IDs.
4. Build presentation bindings. Volume binding must report out-of-cage vertices; do not silently attach
   every failure to the nearest tetrahedron. Surface offsets and strand follower models have separate rules.
5. Produce independently authored simulation LODs and compatible presentation maps. Record which
   variants can transition while preserving mass, momentum, targets and interaction guarantees.
6. Emit versioned products and dependencies through AssetBuilder. Key variants by source revision,
   family, provider revision, material mapping, cook settings, topology/binding hash and platform ABI
   when native serialization requires it. Runtime loads products, not source OBJ/JSON or desktop paths.

Begin with imported cages/surfaces/curves. Automatic tetrahedralization is an optional offline builder
extension; [fTetWild](https://github.com/wildmeshing/fTetWild) is a candidate, not a runtime dependency.
Validate dependency versions, failure behavior and notices before packaging. Do not introduce catch
blocks to accommodate an exception-dependent parser/mesher without the repository-required review.

Public coordinates use asset-local rest data plus an explicit frame transform. A provider may internally
recenter a body; export positions and the matching frame together. Avoid applying both a native body
translation and the same actor translation twice. For initial implementation, bake nonuniform scale
into a cooked variant and reject unsupported runtime scaling. Teleport resets target velocity history
and interpolation; it is not a very large velocity command.

## 7. Aurora presentation integration

Simulation outputs remain CPU-owned and renderer-independent. Extend the existing scene-bridge consumer
interface to collect per-instance deformation contributions. The bridge invokes providers by interface,
never by including plugin components. Establish one owner for each render entity, so a deformed mesh is
not also extracted and drawn as a duplicate StaticMeshComponent.

### Rig presentation

Prefer publishing the completed control pose into the skeletal presentation path when its full resource,
shader, bounds and lifetime contracts are verified. This can avoid regenerating a whole render mesh for
each simulation update. Existing AnimPose::ToSkinRenderData is a useful input, not proof that the current
scene bridge already supports the path. If unavailable, use CPU skinning into the bounded full-revision
path below as a correctness baseline and include its cost in the comparison. Retain palette/pose data
until the consuming frame retires. Interpolate consistent body samples and apply bind offsets once;
update bounds for the resulting pose. Corrective shape/scale changes affect rendering and must not be
mistaken for changes to native collider geometry or volume conservation.

### First mesh path: CPU deformation with bounded full revisions

1. Acquire completed samples and a matching presentation binding. Interpolate only within matching
   topology/frame epochs; reset on teleport or variant change.
2. Evaluate the render surface on CPU. Recompute or correctly transform normals/tangents for the chosen
   binding method. Preserve material sections/UV seams and update deformed bounds.
3. Publish owned RenderResourcePayload bytes and metadata under a unique per-instance resource identity
   and increasing revision, then update the entity binding. Shared rest assets must not share mutable
   deformed resource identity. Bounds and geometry must select the same version.
4. Append through the existing host packet. Respect exact device epochs, upload limits, BUSY retry of
   the same sealed packet, and release/retirement rules. Do not overwrite an in-flight GPU buffer.
5. Retain the previous valid mesh when an optional update is not admitted. Coalesce only unsealed
   cosmetic samples; never drop accepted lifecycle commands or replace the payload of a sealed packet.

The current mesh payload can include indices and static attributes on every revision. That makes this
a correctness baseline, not a claim of cheap high-resolution streaming. Count CPU copies, uploaded bytes,
pending backing storage and all views/shadow passes. Maintain bounded live revisions and a stable
fallback pairing of geometry, metadata and binding.

### Later path: dedicated dynamic streams or GPU embedding

After profiling, consider reusing immutable topology/UVs while updating positions/normals, or uploading
the low-resolution cage and evaluating bindings on GPU. The current scene interfaces do not already
provide this complete path. Specify frozen prepare inputs, per-instance streams, bounds, deformation
ordering relative to skinning, shadow/depth consistency, and retirement before implementing it.

CPU simulation with GPU embedding is distinct from GPU simulation. The latter also changes collision
and gameplay-readback costs. Keep physics on CPU initially; no synchronous GPU readback in the baseline.
Use existing FrameContext leases and RDG access declarations for any approved GPU feature. No new renderer,
queue owner, upload ring or RHI-side business plugin is introduced.

For character deformation, define whether the cage is driven by skeletal targets and whether its output
is an absolute deformed surface or a residual applied after skinning. Do not skin the same displacement
twice. Animation dependencies stay in the adaptor boundary; Aurora core does not acquire a Skeleton type.

## 8. Lifecycle, errors and editor behavior

Provider activation is staged: select -> validate all required capabilities/products/interaction edges ->
retain code/world ownership -> create native resources -> publish handles. Partial failure cleans up
created objects and leaves no registered half-domain. Admission limits should prevent recoverable user
loads from reaching native fatal-capacity paths. A native path that cannot return an explicit recoverable
failure is an integration blocker to resolve, not permission to terminate or silently skip content.

Stop sequence: stop accepting commands; cancel cancellable work; join accepted jobs; drain outcomes;
remove scene contributions; retire queued render resources/features; release samples and native objects;
destroy domains/worlds; unregister and release live providers. DLL code remains resident. Cancellation request is
not completion. Failed initialization follows the same ownership rules for objects already created.

Mobile background/resume must reset presentation/time history according to platform frame epochs, avoid
catching up the entire background interval, and rebuild GPU resources for a new device epoch. CPU rest
assets and valid simulation state may survive renderer recreation when the selected policy allows it.

Editor controls use reflected config objects and generic ReflectedFormView/inspector binding, with
CommandService undo and dirty tracking. Expose family/material/provider choice and invalid interaction
diagnostics. Shared debug views show cage topology, contacts, bounds, attachment targets and budget
usage; chrome uses EditorActionRegistry. There is no hard-coded per-provider settings form.

PIE creates independent solver state while sharing immutable products. Preview never writes dynamic
particle state into the source asset. Pause and single-step execute the same coordinator path; leaving
PIE drains callbacks and removes leases before destroying the world.

## 9. Mobile profiles and measurement

These are **proposed benchmark fixtures and budgets**, not supported maximums, product defaults or
measurements. Minimum Android/iPhone models and the shipping frame target are not specified yet.
Freeze those choices before treating numbers as acceptance gates.

| Fixture | Initial workload | Interaction profile |
|---|---|---|
| B1 | The same squeezable prop authored with 8, 16 and 32 rigid controls, a bounded joint graph and identical render mesh | Two opposing press surfaces, dynamic-object response, local indentation and rebound; counts are experiment points, not recommended limits |
| V1 | One 512-node cage, at most 2,500 tetrahedra | Floor, kinematic capsule; optional one dynamic rigid object in native domain |
| C1 | One 32x32-node cloth grid | Static/animated primitive colliders; self-contact disabled and declared unsupported |
| R1 | Eight 64-node strands | Moving attachment and explicit segment-contact test if supported |
| K1 | One 256-control-point cluster asset | Animation targets and primitive collision where implemented |
| Mixed | V1 + C1 + R1; compare an alternate B1 + C1 + R1 assembly | Native volume or rigid rig with independent surface/strand domains; only declared interaction edges |

For a provisional 60 Hz target, start with **1.5 ms P95 total deformable CPU work**, **0.5 ms P95 CPU
presentation preparation**, and **0.5 ms P95 incremental GPU work** for the Mixed fixture. These are
allocation hypotheses to revise with target-device evidence. Native integration may combine rigid and
soft costs; report full-world time plus a matched rigid-only baseline, not a misleading isolated number.
For B1, count the added bodies, joints and contacts as feature cost even though they execute in the rigid
world. Compare B1/V1 at matched compression/rebound quality and accepted collider/surface error.

Measure fixed-tick wall time and total worker CPU time separately. Report P50/P95/P99, worst contact burst,
active/sleeping counts, nodes/elements/constraints, effective substeps/passes, candidate/contact counts,
sample-pool pressure, CPU/GPU memory peaks, bytes uploaded and total frame time. Include all rendering
passes and the device's real game load. No portable vertices-per-millisecond claim follows from one test.

Use release builds on low/mid/high target tiers, recording device, OS, GPU, build flags, provider commit,
asset hash and thermal state. Run a 20-minute sustained gameplay scenario plus lifecycle transitions;
record cold and warm results rather than averaging away throttling. Android's
[thermal guidance](https://developer.android.com/games/optimize/adpf/thermal) supports adaptive workloads.

Degrade cosmetic work by limiting active regions, using prevalidated LOD variants, reducing nonessential
contacts and falling back to animation. Provider/LOD changes occur at declared safe boundaries with an
explicit reset/transfer policy; do not hot-swap arbitrary solver state. Gameplay-critical coupled objects
retain their agreed physics policy. Never silently disable required collision under thermal pressure.

## 10. State, networking and determinism

Separate cosmetic-only domains from gameplay-affecting domains. Cosmetic one-way domains do not feed
impulses back to authoritative rigid bodies. A native provider can implement a one-way response policy
only after verifying the corresponding contact settings; attachments also need the same direction rule.

Do not advertise exact rollback merely because a library has a deterministic build option. Restart
requires provider-specific state, topology/version identity, particles and orientations where applicable,
sleep/target histories, and any necessary internal caches. Render samples are insufficient. Rig restart
additionally needs joint/drive targets, bindings and pose authority; rigid body transforms alone do not
establish complete rig restart. The current
PhysicsWorldState cannot silently claim full-world capture while omitting active gameplay deformables:
reject unsupported full capture or extend the schema explicitly in a separate state-contract change.

Initial cosmetic networking replicates driving poses/events and tolerates local visual variation. Local
or server-authoritative gameplay deformables need an explicit state synchronization strategy. Lockstep
world creation rejects unsupported gameplay domains. A deformable determinism claim requires separate
trace tests across the actual device/compiler/provider combinations.

## 11. Validation and delivery packages

| Package | Deliverable | Exit gate |
|---|---|---|
| P0. Scope and comparative spikes | Required family/interaction matrix; rigid-rig baseline, pinned Jolt/Bullet and optional XPBD reference harnesses | Reproducible quality/cost results and chosen initial providers; coarse squeezing does not automatically require a volume solver |
| P1. Contracts and host | Family interfaces, registry, domain identity, interaction validation, drive ownership and diagnostics | Duplicate registration, wrong-world handles, unsupported edges, pause/single-step and cancellation tests |
| P1a. Native backend prerequisite | Jolt rigid migration is a separate change with adaptor + render ownership | Reuse its engine interfaces and regression coverage; do not reintroduce Bullet |
| P2. First two families | A rigid-rig or volume provider plus an independently selected strand or cloth provider | Both coexist in one scene; supported interaction graph and failure behavior demonstrated |
| P3. Assets and presentation | Cooked products, CPU embedding, exact revisions, bounds, reflected editor workflow | Packaged assets load without source access; no double extraction, stale bounds or in-flight overwrite |
| P4. Mobile acceptance | Android Vulkan and iOS Metal packages with sustained Mixed workload | Agreed device budgets, quality and background/resume tests pass |
| P5. Selective expansion | Additional family/provider, self-contact, GPU embedding, specialist material or stronger coupling | Separate capability/quality/performance gates; existing providers continue working |

P1/P2 can initially use a headless harness. P1a is conditional on the native provider choice and keeps
structural movement separate from feature review. P3 may proceed with authored topology before automatic
meshing. Do not build all specialist plugins during P1.

### Required scenario matrix

| Scenario | What it verifies |
|---|---|
| Rest shape, gravity drop and energy decay | Material mapping, scale, damping and basic stability |
| Moderate compression, shear and twisting | Volume response, element quality and reference-shape error |
| Rig squeezed between two press surfaces, then released | Actual compression DOFs, restoration/damping, effective limits/drives, no blocking rigid shell, and collider/skin mismatch |
| Rig driven by animation and then released to physics | Bone/body offsets, hierarchy conversion, authority transitions, root motion and no double stepping |
| Animated cloth and attached strand | Sampling time, thickness, bend/twist behavior and moving colliders |
| Native soft prop against dynamic rigid body | Bidirectional response, mass ratio and resting contact |
| Two families with no coupling edge | Coexistence without accidental interaction or duplicate stepping |
| One-way dependency and forbidden cycle | Correct graph ordering; explicit rejection of unsupported coupling |
| Required soft-soft/self-contact on incapable provider | Activation fails with a named capability error |
| High-speed thin obstacle | Exposes tunneling; no CCD claim without a passing dedicated implementation |
| Same asset instantiated twice | Shared immutable data but independent simulation and render revisions |
| Invalid/degenerate mesh and incompatible cooked variant | Explicit failure, no assertion/abort-based content validation |
| Queue saturation, pending destroy and unload | Bounded resources, exactly-once outcomes and correct lease retirement |
| PIE restart, suspend/resume and renderer epoch change | Clean state ownership and GPU reconstruction |
| Soft gameplay body with unsupported full snapshot | Capture rejects rather than silently saving rigid-only state |

For each quality fixture record units, density, target material curve, collision thickness, contact speed,
and accepted error before comparing providers. Measure volume ratio and minimum element determinant for
near-incompressible volume cases, strain/shape error for cloth and strands, penetration relative to the
authored thickness, and joint/attachment drift. Moderate accepted tests should not contain inverted
volume elements; extreme invalid states must trigger the documented failure policy rather than a false
claim of unconditional non-inversion. Equal quality at a budget and equal cost at a quality target are
both useful comparisons; equal iteration numbers are not.

Existing relevant targets include PhysicsTest and JoltPhysicsTest. New provider/coordinator/cook tests
are proposed. Future Windows validation follows the repository's close-editor -> configure when sources
change -> targeted Debug build order; native mobile and release performance validation are separate.
This documentation task runs link/diff checks only and does not report those future tests as passed.

## 12. Review findings and risk controls

| Review | Status | Evidence and boundary | Required treatment |
|---|---|---|---|
| Architecture | Risk | PhysicsBackendRegistry is single-backend | Multi-provider deformables do not imply multiple active rigid backends; validate binding before activation |
| Architecture | Addressed by rigid migration | Jolt adaptor owns component/system implementation; lookup uses IPhysicsSystem | Preserve consumer interfaces when adding deformables |
| Architecture | Risk | World subsystem order and current automatic physics Tick | Explicit host drive owner and one shared advancement path |
| Architecture | Risk | Scene extraction knows static components | Add consumer-owned deformation contribution interface without plugin includes |
| Architecture | OK in proposal | New engine module contains contracts; implementations are plugins | Keep module dependency direction, process-resident code and explicit object ownership |
| Mobile GPU | Risk | Full mesh revisions and existing upload budgets | Profile complete cost; bound pending versions; optimize streams only after evidence |
| Mobile GPU | Risk | Vulkan/Metal backend presence is not packaged runtime acceptance | Device packages, warm-load tests and lifecycle verification are P4 gates |
| Mobile GPU | OK in proposal | CPU simulation and capability-gated presentation | Preserve animation/CPU fallbacks and existing renderer ownership |

These are design assessments, not runtime validation results. Repository AGENTS.md takes precedence over
older skill examples: Aurora remains foundation, editor extensions may use the approved editor framework,
and GLES is not a supported fallback.

## 13. Decisions before implementation

The user's multi-technology/coexistence scope is established. The following concrete choices remain for
implementation review rather than another general research round:

1. Initial two families and interaction edges: compare rigged versus volume props, plus cosmetic strands/cloth;
   decide whether self-contact or cross-deformable coupling is required from day one.
2. Prop provider: existing Jolt rigid world plus a rig adapter, or a future Jolt v5.6.0 native volume provider
   route after P0; independently choose the second family provider. No single SDK is selected for every use case.
3. Proposed `engine/deformable` and plugin ownership, target names, startup manifests and the explicit
   host/manual-drive seam, including conditional physics ownership extraction.
4. Minimum Android/iPhone hardware, frame target, accepted material/contact errors and provisional budgets.
5. Cosmetic versus gameplay authority, fallback policy, and whether restart/network state is in scope.
6. Any actually encountered exception-dependent third-party paths, with call sites, explicit-error
   replacement and cleanup/completion plan documented before a repair is approved.

Implement accepted packages as focused OpenSpec changes. Keep each change's implementation and specs
together when eventually submitting; directory restructuring remains a separate review. Commit and
OpenSpec archive still require the explicit confirmations specified by AGENTS.md.
