---
title: "Soft-Body Simulation Technology and Integration Comparison"
description: "Multiple deformable simulation families, versioned library evidence, complete architecture alternatives, mobile constraints, and selection gates for SkyEngine."
module: "deformable"
updated: "2026-10-22"
---

## Decision and scope

Current repository decision (2026-10-27): rigid physics migrates to Jolt v5.6.0, with adaptor + render implementation targets. Bullet alternatives below are comparison evidence, not active plugins. Native deformables remain unimplemented.

The integration should support **multiple deformable simulation families and providers**, selected by
workload, platform and interaction requirements. It must not reduce the engine design to one SoftBody
component or one chosen SDK. For coarse mobile squeezing, evaluate a rigid-body/constraint rig with
skinning first; CPU XPBD is another baseline when finer deformation is required. Jolt and Bullet native
soft bodies are provider candidates rather than the definition of the entire system.

No option has been integrated or benchmarked by this work. Performance rankings below are engineering
judgments, not measured results. The companion [integration plan](softbody-integration-plan.md) defines
the contracts, alternative implementation paths, coexistence boundaries, and staged delivery.

Physics-driven rigs, cloth, strands, volumetric elastic bodies and character secondary motion share scheduling and lifecycle
needs, but have distinct topology, material and contact contracts. MPM, IPC and fluid/particle research
must remain explicit specialist extensions. A common host does not imply universal cross-solver contact,
cutting, physical muscle activation, exact rollback, or equivalent material parameters.

Four decisions must remain separate:

1. **Material and discretization:** constrained rigid elements, springs, tetrahedral elastic energies, shells, or material points.
2. **Solver:** XPBD, local/global projective dynamics, Newton methods, or vertex block descent.
3. **Integration:** one native rigid/soft world, or an independent deformable solver with explicit coupling.
4. **Presentation:** skeletal skinning, cage embedding, surface binding or strand followers. Skinning is
   not a material solver; it can display the output of a rigid rig as well as other simulations.

XPBD and FEM are compatible choices, not mutually exclusive products. A tetrahedral volume constraint
alone is also not a calibrated hyperelastic constitutive model.

## 1. Algorithm reference

| Family | Useful property | Limitation relevant to this project | Position in the plan |
|---|---|---|---|
| Mass-spring | Simple local elastic forces and authoring | Stiff explicit systems require small steps; mesh connectivity affects material response | Baseline and specialized effects |
| Rigid-body constraint rig + skinning | Reuses rigid contacts and joints; low-dimensional controls drive a smooth visible surface | Deformation is limited by rig DOFs; collider coverage and volume response are approximate | First comparison baseline for coarse squeezable mobile props |
| PBD / XPBD | Local constraint solves with controllable work; XPBD gives compliant constraints a physical energy interpretation | Finite solver budgets still leave error; collision detection is a separate subsystem | Preferred real-time solver family |
| Shape matching | Efficient restoration of particle-cluster shape | Traditional geometric variants are harder to calibrate to measured materials | Cosmetic deformation alternative |
| Tetrahedral FEM | Strain and material energies describe volumetric behavior | Meshing, inversion, near-incompressibility, and nonlinear convergence need attention | Solid-body representation; select a solver separately |
| Projective dynamics | Alternating local projections and a global solve; fixed structure permits reusable factorization | Material formulation and changing topology/weights constrain reuse | Candidate for fixed-topology specialist workloads |
| VBD | Vertex-local block optimization of an implicit dynamics objective | Needs independent contact, parallel scheduling, and engine integration work | Research candidate after the first production path |
| MPM / MLS-MPM | Particle/grid discretization handles large elastoplastic deformation | Grid work, transfers, surface reconstruction, and material-specific tuning | Separate snow/mud/plasticity project |
| FEM with IPC | Optimization-based treatment of demanding frictional contact | Robustness has nonlinear-solve and collision costs; real-time suitability is workload-dependent | Quality reference for hard contact cases |

Primary references: [deformable-model survey](https://matthias-research.github.io/pages/publications/egstar2005.pdf),
[XPBD](https://mmacklin.com/xpbd.pdf),
[shape matching](https://matthias-research.github.io/pages/publications/MeshlessDeformations_SIG05.pdf),
[projective dynamics](https://users.cs.utah.edu/~ladislav/bouaziz14projective/bouaziz14projective.html),
[VBD](https://about.roblox.com/publications/vertex-block-descent),
[MLS-MPM](https://yuanming.taichi.graphics/publication/2018-mlsmpm/), and [IPC](https://ipc-sim.github.io/).

For an undamped scalar XPBD equality constraint, the update is
`alphaTilde = compliance / h^2`,
`deltaLambda = (-C - alphaTilde * lambda) / (sum(invMass * gradientSquared) + alphaTilde)`.
Accumulate lambda within a substep; the basic algorithm resets it at the next substep. Compliance is not
a dimensionless universal stiffness slider. Geometry normalization, rest volume, units, and the constraint
definition belong to the material mapping. Increasing iterations improves convergence; it cannot provide
collision guarantees or make different constitutive models equivalent. See the [original derivation](https://mmacklin.com/xpbd.pdf).

Compare more small steps with more iterations at equal quality and cost. The
[Small Steps paper](https://mmacklin.com/smallsteps.pdf) motivates this experiment, but does not establish
the best collision update rate or parameter mapping for every library.

## 2. Versioned implementation evidence

Inspection date: 2026-10-22. Jolt **v5.6.0** is an evaluation candidate, not a new SkyEngine dependency;
the [release](https://github.com/jrouwe/JoltPhysics/releases/tag/v5.6.0) identifies commit prefix `e77f175`.
The implementation change must record its full resolved commit and build options. Bullet **3.25** is
already pinned in [thirdparty.json](../../cmake/thirdparty.json). Research repositories on moving branches
below are references until an evaluation records an immutable revision.

| Candidate | Verified evidence | Important boundary |
|---|---|---|
| Jolt v5.6.0 CPU soft bodies | XPBD execution and rigid-body response are documented in its [architecture](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Docs/Architecture.md); [settings](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Jolt/Physics/SoftBody/SoftBodySharedSettings.h) include edges, bend, volume and skinned constraints | Elastic constraints do not imply general FEM material calibration |
| Bullet 3.25 traditional SoftBody | [btSoftRigidDynamicsWorld](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletSoftBody/btSoftRigidDynamicsWorld.h) extends the rigid world with soft bodies | Requires world/configuration/solver changes, not only a new component |
| Bullet 3.25 Deformable | A separate [btDeformableMultiBodyDynamicsWorld](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletSoftBody/btDeformableMultiBodyDynamicsWorld.cpp) implements a different deformable pipeline | Do not mix its settings or guarantees with the traditional SoftBody route |
| PositionBasedDynamics reference library | [Project README](https://github.com/InteractiveComputerGraphics/PositionBasedDynamics) describes constraint-based simulation and dependencies | Useful algorithm/reference baseline; production mobile packaging and coupling remain unverified |
| PhysX 5.8 deformable volumes | [Documentation](https://nvidia-omniverse.github.io/PhysX/physx/5.8.0/docs/DeformableBodyOverview.html) specifies FEM and CUDA GPU execution | Excluded from the common Android/iOS baseline; this restriction is not a restriction on FEM itself |
| Chaos Flesh | [Epic documentation](https://dev.epicgames.com/documentation/unreal-engine/chaos-flesh-overview) describes tetrahedral authoring, low-resolution runtime simulation and offline high-resolution caches | Asset/workflow reference, not an assumed standalone redistributable SDK |

Jolt v5.6.0's [contact collector](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Jolt/Physics/SoftBody/SoftBodyMotionProperties.cpp)
filters for rigid bodies and explicitly leaves soft-versus-soft support as a TODO. Therefore the proposed
adapter reports soft-soft collision as unsupported and self-collision as unavailable in the accepted
baseline. A ray/query against a soft shape is not evidence of simulated soft-soft contact. Comprehensive
deformable CCD is also unverified and must remain disabled in the advertised capability set.

Bullet 3.25 exposes `VF_SS`, `CL_SS`, and `CL_SELF` in
[btSoftBody::fCollision](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletSoftBody/btSoftBody.h).
These are configuration-specific mechanisms, not proof of robust arbitrary triangle self-contact, CCD,
or acceptable mobile cost. Validate the selected mode with the actual topology and solver.

For Jolt, `mNumIterations` is named as an iteration count in
[creation settings](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Jolt/Physics/SoftBody/SoftBodyCreationSettings.h),
but its [implementation](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/Jolt/Physics/SoftBody/SoftBodyMotionProperties.cpp)
uses `inDeltaTime / mNumIterations` for the soft-body substep. Do not expose a supposedly equivalent
cross-backend "iteration count" or multiply it by another unexamined substep loop.

## 3. Native implementation building blocks

Relative effort includes engine work, not just calling the native soft-body constructor.

| Option | World/coupling model | Reuse and migration | Main risk | Recommended role |
|---|---|---|---|---|
| A. Full Jolt backend | Rigid and soft bodies use the same Jolt world | New third-party packaging and rigid-backend parity; backend-neutral component ownership must be resolved | Broader migration; required self/soft-soft contacts fail the stock baseline | Native-world provider candidate, gated |
| B. Extend Bullet SoftBody | Existing backend adopts a soft-rigid world | Reuses current dependency, rigid integration, IDs and asset consumers | World replacement can regress rigid behavior; material/contact quality needs measurement | Required lower-migration-cost benchmark; viable delivery fallback |
| C. Bullet Deformable/FEM | Deformable multibody world | Reuses vendor dependency, but not the current native world implementation | Larger solver/API change and material tuning burden | Evaluate only if volumetric material needs justify it |
| D. Independent CPU XPBD | Solver owns particles; current physics supplies collision proxies/queries | Retains Bullet rigid world; may reuse reference algorithms | Engine owns collision, coupling, lifecycle, debugging and maintenance | Specialized cloth/strand/volume provider; start with restricted cosmetic effects |
| E. Separate Jolt soft world beside Bullet | Duplicated rigid proxies across two SDK worlds | Avoids immediate rigid migration | Proxy synchronization and two-way impulse exchange become a custom co-simulation system | Reject as a shortcut for coupled gameplay |
| F. GPU solver | Compute simulation on Vulkan/Metal or vendor-specific APIs | Potential scaling for sufficiently large workloads | GPU contention, readback/coupling, scheduling and platform validation | Later optimization; no first-release requirement |
| G. Rigid-body rig + skinning | Several dynamic bodies and compliant joints in the existing rigid world drive bones | Reuses rigid collision/response; needs complete spring/limit mapping and physics-to-pose binding | Sparse collision coverage, limited deformation modes and high-stiffness joint cost | Preferred first experiment for coarse squeezing; does not require BulletSoftBody |

Option D can provide one-way interaction: sample the rigid world, move only cosmetic particles, and publish
deformation. Simply applying the opposite impulse to Bullet after a separate soft solve does not create
a converged coupled solve. A gameplay version needs explicit substep ordering, rigid velocity/inertia
access, contact identity/friction state and tests of energy, momentum and resting stability. Current
raycast/sweep APIs alone do not supply all these contracts.

Option A must not load Jolt and Bullet registration modules simultaneously: the current
[registry](../../engine/physics/src/PhysicsBackendRegistry.cpp) replaces and shuts down the previous
backend. Selection is process-startup policy today, not per-world selection or safe hot swapping.

### Rigid-body rigs are a first-class deformation route

A single rigid body rigidly driving a mesh cannot change its internal shape. Several bodies can move
relative to each other, and weighted skinning can turn those motions into a continuous-looking surface.
For compression along a given direction, provide the corresponding translational compliance or another
rig mechanism that actually permits shortening; a rotation-only joint chain mainly bends/folds. Rigid
bone binding alone does not provide elastic recovery. Springs or suitable pose drives establish the
restoring response, with damping and travel limits controlling oscillation and collapse.

The general rigid-body/constraint-to-skeleton pattern is documented by Epic's
[Physics Asset workflow](https://dev.epicgames.com/documentation/en-us/unreal-engine/physics-asset-editor-in-unreal-engine).
Its [constraint reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/physics-constraint-reference-in-unreal-engine)
distinguishes linear freedom, soft limits and drives. Bullet 3.25 exposes per-axis spring, damping and
equilibrium controls in
[btGeneric6DofSpring2Constraint](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletDynamics/ConstraintSolver/btGeneric6DofSpring2Constraint.h).
These sources establish available mechanisms, not a validated SkyEngine squeeze asset or mobile benchmark.

An initial prop can use a small spatial arrangement of convex bodies, compliant connections, and a
weighted control skeleton. The physics graph need not match the skeleton's parent tree. Recover global
bone transforms using authored body-to-bone offsets, then derive parent-local poses for skinning.
Selectively filter adjacent-body collisions so rest-shape overlap does not fight the joints, while
retaining necessary nonadjacent collision. Do not put a full-size rigid shell around the rig: it would
prevent the intended compression. A fixed central body also sets a hard compression limit.

This route can exert real forces on surrounding dynamic bodies through native rigid contacts. Its
limitation is the geometric/material approximation, not an inherent absence of physical response.
The visible surface between colliders can penetrate obstacles, small dents need more controls, and
ordinary skinning does not enforce volume conservation. Corrective shapes can improve appearance but
do not repair collision or create a calibrated elastic material. More joints/colliders can erase the
cost advantage, so compare at matched visible/contact quality rather than assuming rigs are always faster.

| Requirement | Rigid rig + skinning | Particle/cage XPBD | Constitutive volume model |
|---|---|---|---|
| Broad flattening, wobble and rebound | Good candidate with authored compression DOFs | Good candidate | Capable; potentially unnecessary complexity |
| Fine arbitrary indentation | Requires a denser rig and collider coverage | Needs sufficient spatial resolution and contact support | Needs sufficient mesh resolution and contact support |
| Response against existing rigid objects | Native rigid contacts in the same world | Native provider or explicit coupling required | Depends on selected integration/coupling |
| Volume/material calibration | Additional formulation/fitting; skinning alone is insufficient | Depends on actual constraints and material mapping | Natural place for a constitutive model; still needs validation |
| Mobile integration | Small rigs can reuse pose/skinning machinery; measure actual joint/contact cost | Bounded simulation plus embedding cost | Workload-specific convergence and rendering cost |

## 4. Capability and portability comparison

"Present" means upstream API/source evidence exists. "Validate" means the target workload has no accepted
test result. No column below claims a SkyEngine feature is implemented.

| Requirement | Jolt candidate | Bullet SoftBody | Independent XPBD |
|---|---|---|---|
| CPU fixed-topology elastic deformation | Present; validate materials | Present; validate materials | Must implement or adapt |
| Soft-rigid bidirectional response | Present in same native world; validate | Present in soft-rigid world; validate | Substantial explicit coupling work |
| Soft-soft contact | Unsupported in inspected path | Modes present; validate | Must implement |
| Self-contact | Not accepted for baseline | Cluster mechanism present; validate scope | Must implement |
| Deforming surface CCD | Not accepted for baseline | Not accepted without workload tests | Must implement |
| Bone/skinned targets | Skinned constraints present; binding work remains | Anchors exist; skeletal policy remains adapter work | Custom target constraints |
| Mobile production suitability | Platforms supported upstream; game/device evidence missing | Existing dependency is useful; soft path/device evidence missing | Full responsibility of engine team |
| Exact rollback with soft bodies | Not established | Not established | Not established |
| Mobile asset/render pipeline | Must build | Must build | Must build |

Jolt lists Android/iOS and ARM64 NEON in its
[versioned README](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/README.md). Its newer GPU facilities
do not establish that this CPU soft-body integration runs on the GPU. Likewise, SkyEngine having Vulkan
and Metal backends does not establish a shipped mobile renderer/soft-body combination; see
[platform status](../architecture/platform-support.md) and [Aurora rules](../../engine/aurora/AGENTS.md).

For mobile, optimize total feature cost: native simulation, collision, CPU embedding, normal/tangent
reconstruction, uploads, and all rendering passes. A cheap particle solve with an expensive high-resolution
surface update can still miss the frame budget. Use bounded active counts, authored LODs, sleeping and
cosmetic fallbacks. Measure sustained gameplay under thermal load, as recommended by the
[Android thermal guidance](https://developer.android.com/games/optimize/adpf/thermal).

## 5. Asset/tooling choices

| Choice | Benefit | Cost or prerequisite | Recommendation |
|---|---|---|---|
| Authored/imported tetrahedral cage | Deterministic initial scope; no automatic mesher dependency | Content authoring and validation | First solid-body milestone |
| Offline fTetWild | Candidate automatic tetrahedralization of surface inputs | Version, topology/quality tests, failure-path and dependency review | Optional builder integration after imported cages work |
| Runtime tetrahedralization | User-generated geometry can be processed on device | Unbounded latency/memory and harder failure handling | Exclude from first release |
| CPU render embedding | Simple ownership and debugging | Scales with rendered vertices and passes indirectly through upload cost | First presentation path |
| GPU render embedding with CPU simulation | Upload a small cage and deform larger meshes on GPU | Requires new rendering contracts, bounds and shader variants | Later measured optimization |

Record third-party notices with the exact packaged revision. Jolt uses
[MIT](https://github.com/jrouwe/JoltPhysics/blob/v5.6.0/LICENSE); Bullet's selected source carries its
[zlib-style notice](https://github.com/bulletphysics/bullet3/blob/3.25/src/BulletSoftBody/btSoftBody.h);
the PBD reference project declares [MIT](https://github.com/InteractiveComputerGraphics/PositionBasedDynamics);
fTetWild carries [MPL-2.0](https://github.com/wildmeshing/fTetWild/blob/master/LICENSE.MPL2).
These identifiers are an inventory, not approval to ship a particular dependency set. Keep meshing tools
in offline tooling, and review their actual bundled dependencies and explicit failure behavior.

## 6. Workload-driven combinations

| Workload | Representation and material | Candidate provider | Essential interaction contract | Mobile strategy |
|---|---|---|---|---|
| Elastic prop | Tetrahedral cage, edge/volume constraints or calibrated elastic energy | Native Jolt/Bullet; specialized XPBD/FEM | Static/kinematic contact; optionally native-world bidirectional rigid coupling | Low-resolution cage and bounded active count |
| Coarse squeezable toy, cushion or stylized body | Spatial rigid-body rig, compliant joints, skinned surface | Existing rigid backend plus physics-rig adapter | Native rigid contact, controlled internal collision and visible/collider error limits | Few authored controls and simple colliders; benchmark before choosing a volume solver |
| Garment/flag | Triangle surface, anisotropic stretch/shear and bending | Specialized cloth XPBD; native solver if its contact subset suffices | Character colliders, thickness, friction; self-contact only when implemented | Cloth LOD, reduced colliders and cosmetic fallback |
| Rope/cable/hair guides | Segments or rods with stretch, bend and possibly twist | Specialized strand XPBD; library rod implementation after verification | Segment/swept contact and attachment targets; particle contacts alone are insufficient | Simulated guides drive render strands; no implicit per-hair solve |
| Character flesh/jiggle | Local cage or clusters, animation targets and deformation residuals | Volume XPBD, shape matching, or specialized FEM | Final animation pose at simulation time; chosen one-way/two-way attachment semantics | Local regions, low-frequency cosmetic tiers, animation-only fallback |
| High-fidelity rubber/tissue | Tetrahedral constitutive model, validated material parameters | FEM with Newton/PD/VBD; constraint-energy XPBD | Contact accuracy, inversion and near-incompressibility requirements | Selective high-end tier; lower-cost representation on other devices |
| Snow/mud/plastic flow | Material particles plus background grid | MPM specialist | Grid boundary/contact and optional rigid coupling | Separate workload budget; baking or simplified fallback |
| Dense, penetration-sensitive contact | Surface/volume discretization and barrier contact | IPC specialist | Collision-free initialization, CCD and converged optimization conditions | Offline/reference tier unless device measurements justify runtime use |

These are separate capability profiles. In particular, a provider that simulates a flag against capsules
must not advertise a complete self-colliding garment solution. Character secondary motion is a use case
that can reuse several providers, not a new material model.

## 7. Complete architecture alternatives

| Architecture | Delivery shape | Strength | Cost and limitation | When to choose |
|---|---|---|---|---|
| S1. Backend-integrated deformables | One rigid SDK supplies supported cloth/solid/rod capabilities through engine contracts | Native rigid coupling, few synchronization boundaries | SDK capabilities bound the feature set; changing SDK requires rigid parity | Small feature set and strong bidirectional rigid interaction |
| S2. Heterogeneous deformable host | Common assets/lifecycle/scheduling; family-specific provider interfaces; native and standalone domains coexist | Fits different workloads without forcing one material or solver | Requires domain ownership, capability negotiation and explicit cross-domain interaction | Recommended general engine architecture |
| S3. Unified particle simulation | Rigid, cloth, soft and possibly fluid primitives share a custom particle/constraint solver | Potentially consistent coupled solve within the supported formulation | Reimplements major physics capabilities; contact and material accuracy remain substantial work | Simulation is a core differentiator and solver ownership is funded |
| S4. Specialist continuum stack | FEM/VBD, IPC and MPM providers with workload-specific assets and contact | Supports demanding materials and reference-quality results | Highest integration and tuning burden; common mobile baseline is unlikely | Specialized simulation products, offline work or selected high-end effects |

Recommended direction: **S2 as the integration architecture, S1 as one provider path**. Introduce only the
provider families required by the first accepted workloads; do not build every solver immediately.
S3 is an alternative architecture, not a prerequisite for S2. S4 can later contribute isolated specialists.

Within one scene, a native volume prop and a standalone cosmetic strand can coexist. They do not
automatically collide with each other. Every interaction edge must name its mode: none, one-way proxy,
same-native-domain coupled, or a separately implemented coupled solver. Changing a provider can change
which edges are available. Unsupported required edges reject scene activation rather than silently vanish.

## 8. Selection gates

For a squeezable prop, first compare option G against the volume routes. Accept G when its deformation
modes, collider coverage and response meet the task. Do not require a tetrahedral solver merely because
the visible object looks soft. A pile of rigged props uses rigid-proxy contacts; this is a different
capability from simulated soft-surface contact and is not ruled out by Jolt's soft-soft limitation.

1. **Define required interactions.** If dense self-contact or soft-soft piles are mandatory, stock Jolt
   does not pass the current capability gate. Either reduce scope explicitly or evaluate another route.
2. **Run A/B tests.** Compare Jolt and Bullet using the same cage, density, scale, initial conditions,
   colliders, loading history, and target shape/volume error. Native parameter values need not match.
3. **Accept each provider's whole integration.** Jolt is selected only after rigid-body compatibility, error handling,
   assets, lifecycle, packaged mobile rendering and sustained performance pass. A standalone demo is insufficient.
4. **Use B if migration dominates.** Bullet can supply the native-world slice if it meets the same required
   quality/performance gates. Preserve backend-neutral assets; do not describe Bullet coefficients as XPBD compliance.
5. **Use D with a stated scope.** Independent XPBD is a reasonable choice for one-way cosmetic
   deformation when maintaining the current rigid backend is a hard requirement. It is not the default
   shortcut to universal cross-solver coupling.
6. **Validate combinations.** Test at least two distinct workload families together, provider selection
   failure, shared budget pressure, and missing coupling edges. A single falling soft cube cannot accept S2.

Do not choose by an invented library score or a desktop demo's vertex count. The
[integration plan](softbody-integration-plan.md) supplies workload definitions, proposed budgets, failure
tests, and the decision record needed to turn this conditional recommendation into an implementation choice.
