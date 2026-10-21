## Context

The approved order is pass resource declarations, World bridge, editing, Shadow, then IBL/HDR. The first two implementation changes preserve a single renderer producer and consume owned World snapshots. This change extends the scene pipeline after those gates pass.

## Goals / Non-Goals

Goals: shared light/shadow/environment declarations, explicit per-frame resource bindings, one directional shadow map, bounded direct lights, linear HDR scene output and final display conversion, and reproducible native pixels.

Non-goals: cascaded/point/spot shadows, dynamic environment capture, skinning, clustered lighting, automatic material conversion to a full PBR graph, module restructuring, and claims of Metal/mobile native validation from Windows results.

## Decisions

### Pass ownership

Lighting and shadow uniforms/textures/samplers are declared in their consuming Pass shared headers. Field/resource lists expand native C++ types and typed slots as well as shader declarations. Constant arrays/nested light structures are enabled only with verified size, alignment and stride; hand-written duplicate CPU ABI structs are not a fallback. Optional View remains a Pass choice.

L02 enables bounded float4 arrays with reflected element count and 16-byte stride. Other array element types and nested structures remain rejected. Shader cache format 3 stores this metadata and rejects older blobs. SceneLightingPass owns eight light slots and one shadow image/sampler; ShadowPass owns its light projection, while SceneSurfaceBatch carries per-draw receive-shadow state. Existing SceneBatch and unlit SceneForward declarations remain compatible.

### Depth and HDR graph capabilities

The existing raster pass accepts a depth-only attachment set. All attachment validation, content-definition rules, backend pipelines and encoder paths handle zero color attachments. D32 sampling uses a depth aspect and a float scalar shader contract; comparison is initially explicit in the scene shader with an ordinary sampler. Depth MSAA sampling remains unsupported. HDR uses RGBA16_FLOAT for render/sample intermediates; CPU copy/upload support must be extended independently and checked against actual texel sizes.

### Shadow visibility and resources

A directional shadow pass consumes an owned caster snapshot that is not limited by camera frustum culling. Exact frozen mesh versions, submesh ranges and transforms remain resolved by the common geometry path. Shadow policy, light-space projection and bindings belong to Pipeline. Shadow maps are frame-owned graph resources, and producer/consumer dependencies follow declared attachment writes and sampled reads. No native GPU pointer enters component data or worker snapshots.

The first shadow-enabled directional light in stable entity order uses a 512-square D32 map fitted to visible receiver and caster bounds. An absent shadow uses a cleared 1-square D32 input. Unbounded geometry is explicitly unsupported for fitting. Caster extraction still honors visibility masks and draw tags; it retains exact mesh/material versions and does not bypass resource readiness or retirement.

### Direct lighting and environment

Direct lights are bounded and validated before shader upload, with deterministic ordering. Point/spot attenuation and directional light direction use the existing World-derived values. Environment inputs are explicit logical resource versions with neutral defaults; diffuse irradiance, roughness-prefiltered specular input and a BRDF integration input have separate declarations. The first implementation consumes authored/precomputed inputs rather than adding dynamic capture. The exact supported image/view shapes are recorded with their native tests.

### Display output and compatibility

Scene color stays linear until a final pass applies exposure and tone mapping. Existing unlit/custom passes retain their declared behavior; lighting is not inferred from a universal material ABI. Editor controls use reflected data and CommandService. In-flight versions and resize use existing frame leases and output revisions.

HDR is an explicit per-output SceneViewInput option. The scene owns an RGBA16_FLOAT intermediate, followed by a shared ToneMapPass using exposure in EV ([-16, 16]), Reinhard mapping, and sRGB display conversion. UNORM outputs receive shader conversion; sRGB output formats use hardware conversion. An already initialized scene output rejects HDR composition instead of applying exposure twice. UI/composite features run after scene display conversion. Without a scene input, existing outputs keep their previous behavior.

### Environment payload and asset shape

SceneEnvironmentPass declares separate diffuse irradiance, specular prefilter and BRDF images plus their samplers. Initial authored inputs are linear RGBA8 or RGBA16_FLOAT 2D images: diffuse/specular use latitude-longitude direction mapping; BRDF uses NdotV/roughness coordinates. Specular mip levels contain prefiltered roughness, sampled at roughness times (mipCount - 1). Cube, array, 3D, compressed and sRGB assets are rejected explicitly. This phase does not generate irradiance, prefilter or BRDF assets. The initial surface model uses flat derivative normals, dielectric F0=0.04 and roughness=0.5; it is not a general material graph.

Missing, pending or unavailable image versions bind frame-owned initialized defaults: diffuse 0.03, specular black, BRDF (1,0). No native descriptor may remain missing. Intensity is finite in [0,64], with zero disabling environment contribution. Packet extensions carry owned settings and logical exact image versions; catalog/frame leases retain selected native images through submission retirement. Upload admission, continuation prefixes and readback pitches use 8 bytes per RGBA16_FLOAT texel, including partial rows/mips/layers. Native format limits remain backend checks.

EnvironmentComponent stores only asset UUIDs, intensity, exposure and enabled/HDR flags, registered through existing reflection controls. A World accepts at most one environment component; duplicates or invalid settings produce bridge issues and omit the ambiguous settings. Missing/failed assets produce issues and use the documented defaults. The editor appends these inputs to its existing host packet and scene viewport. Changes release superseded logical keys after frozen frame retention. Asset-source caching freezes each loaded asset for its lifetime (revision 1); replacement by another UUID is supported, while live file hot reload is not claimed.

New editor Cube/Plane objects explicitly select the reserved built-in SceneLit material. Existing empty/default material references continue using SceneForward. The existing Create action registry includes Environment; no per-field settings UI or new module target is introduced.

## Risks / Trade-offs

- Depth-only raster touches all three backend compilers → first isolate graph and native pixel tests before scene integration.
- Off-camera casters increase preparation/storage → retain bounded snapshots and deterministic extraction; report budget failures explicitly.
- Native shader array/packing differences → validate native C++ offsets and actual backend shader compilation.
- Environment assets may exceed the current uploader subset → validate format/mip/layer byte accounting before advertising that input shape.
- Windows cannot execute Metal/mobile → leave their native gates unchecked with concrete follow-up coverage.

## Migration Plan

Implement L01 depth attachment/sampling, L02 lights/shadow, L03 HDR/display, then L04 IBL/resource/editor integration. Keep each stage's evidence in tasks. Do not archive or commit until the user reviews the concrete state and confirms.
