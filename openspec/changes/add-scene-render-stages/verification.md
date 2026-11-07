# Verification - 2026-11-07

## Implemented corrections

FrameFeatureRegistry is the sole owner of scene-extension registrations. Packet validation projects capacity, identities and generations in command order. Extensions are sorted when registration changes; immutable frame snapshots route payloads to selected outputs. Scene retirement cascades to its extensions, while independent retirement frees the extension and sends its retirement ID after its last borrower.

FrameFeatureOwners retains ordinary and scene-extension leases together through both existing graph keep-alive and presentation ownership, avoiding a second Retain overwriting the first. Shared scene color/depth raster attachments must LOAD and STORE; invalid clears return a FrameError before appending the extension pass. Particle-only HDR work now selects the HDR path.

## Validation

- Actual Aurora.Core.Static, Aurora.Pipeline.Static, Aurora.Adaptor.Static, Aurora.Render, Aurora.Render.Editor and SandboxModule builds passed.
- Aurora.CoreTest: 127 passed, 1 distribution-fixture skip. Six SceneExtensionRegistryTest cases cover projected overflow (63+2), duplicate names, stale generations, payload validation, order, independent retirement, parent ownership, failed projection, and disable/re-enable snapshot invalidation.
- Aurora.PipelineTest: 45 passed. Stage capture covers output routing, color/depth identity, LOAD state, rejection of shared attachment CLEAR, and propagation of an extension failure.
- Renderer Coordinator/State/Resource/Prepare tests: 18/14/39/3 passed. Module tests passed when run in their declared working directory; distribution-only cases are tracked in the particle verification record.
- Strict OpenSpec validation and git diff --check passed.

## Remaining

The complete HDR/non-HDR attachment sharing matrix is still open (task 3.2). Native Windows GPU submission tests were not available on this macOS host. See ../add-particle-system/verification.md for environment and runtime caveats. No commit or archive was performed.
