## Why

The Engine asset migration exposed source/product boundary defects: overlay paths can replace Engine identities, stale manifest caches can overwrite cook-worker commits, and generic mesh/material consumers select source files implicitly. Editor deletion also leaves sources discoverable after restart.

## What Changes

- Preserve mount-qualified source identity and open sources from their owning filesystem while retaining explicit overlay lookup for project overrides.
- Serialize manifest mutations against freshly read state and make editor source deletion durable with explicit failure handling.
- Pass resolved fallback materials into mesh compilation; propagate effective material dependencies through normal and SoftBody builders.
- Resolve material resources independently of meshes, eliminating the navigation overlay's Plane dependency and runtime source-path fallback.
- Resolve Engine defaults at editor/bootstrap boundaries, retaining UUID component serialization and readable Inspector paths/hints.
- Use a writable editor DDC location and remove obsolete source-path assumptions. Target ownership and broad editor integration restructuring remain separate review units.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `assets`: mount-qualified identity, authoritative file access, concurrent manifest mutations and durable deletion.
- `asset-cook`: explicit fallback inputs and complete effective source dependencies.
- `aurora-renderer`: independent material resolution without dummy geometry or source-path policy.
- `editor-assets`: Engine defaults, preserved asset labels/hints and writable derived-data caching.

## Impact

Framework asset catalog and editor mutations; Aurora mesh cook, authoring compiler and scene resource source; Recast render bridge; SoftBody builder; editor creation/preview bootstrap. Existing source manifests, UUIDs and AuroraMesh product serialization remain compatible. This change owns the new review remediation; prior project-object-creation-defaults and tooltip work retain their own implementation and validation evidence.
