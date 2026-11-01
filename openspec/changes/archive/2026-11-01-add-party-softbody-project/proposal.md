# PartySoftBody prototype

Create a separate SkyEngine game project at `F:/Investigate/PartySoftBody` to validate character contact and landing squash using a seven-particle visual cage. Gameplay, procedural assets, tuning and the prototype solver belong to that project. Reuse the existing Jolt character world and Aurora renderer.

The engine addition is a runtime scene-source interface in the existing Aurora adaptor target. The launcher host consumes project-owned scene data without exposing the concrete Aurora module or sharing frame submission ownership. The requested shadow fix adjusts built-in Lit receiver-plane bias, a bounded 3x3 PCF kernel and shadow sampler edge clamping in existing targets.

The project also provides an in-window HUD and keyboard-controlled cage/body/probe overlays. Recovered character tuning is exported with CRC and byte-offset evidence. A World asset owns the static arena, lights and Physics subsystem; dynamic character actors share its physics runtime and render through RenderSceneBridge. Main character collision and independently queryable trigger spheres are separate from the visual cage. A planar restitution adapter uses script-derived normal/roll parameters without creating vertical launch impulses.

This prototype approximates the recovered behavior. Exact native solver parity, proprietary assets, networking, generic deformable-provider architecture and mobile acceptance are outside this change.
