---
title: "SkyEngine Documentation"
description: "Index of SkyEngine technical documentation."
updated: "2026-10-07"
---

## SkyEngine Documentation

Technical documentation for engine contributors and technical users. All documents are written in English, in
Markdown, and grounded in the current repository state.

### Modules

- [Network Module](modules/network.md) - backend-swappable multiplayer transport, sessions, and data-oriented
  replication (`Network`, `NetworkReplication`, `NetworkEcs`).

### Plugins

- [ENet Network Backend](plugins/network-enet.md) - first real transport backend: reliable/unreliable UDP channels
  over ENet.
- [PVS Plugin](plugins/pvs.md) - precomputed visibility: render-independent core, runtime culling glue, streaming,
  visibility queries and serialization.

### Editor

- [Editor Framework Design](editor/editor-framework-design.md) - startup/project-manager flow and overall layout
  design for the sandbox (non-Qt) editor.
- [Editor Framework Status](editor/editor-framework-status.md) - current implementation state, build/run, gotchas,
  and next steps (session handoff).
- [Reflection Widget Framework](editor/reflection-widget-framework.md) - reflection-driven editor property UI.
- [Play-In-Editor (PIE)](editor/play-in-editor.md) - world duplication, the play session, and editor controls.

### Features

- [Asset Pipeline](features/asset-pipeline.md) - source identities, the mounted source namespace, product bundles,
  loading, on-demand cook, cook configuration and the dependency graph.
- [Cross-Platform Determinism](features/cross-platform-determinism.md) - constraints for bit-identical results
  across platforms: fixed-width integers, byte/char handling, endianness, floating point, ordering, and the
  `SKY_DETERMINISTIC_FP` build option.
- [World Subsystems](features/world-subsystems.md) - the declarative world subsystem registry, `WorldDesc`,
  JSON persistence, and the editor world-config surface.

### Adding documents

- One topic per file; group by `architecture/`, `modules/`, `plugins/`, `features/`, or `guides/`.
- Start each file with YAML frontmatter (`title`, `description`, optional `module`, `updated`).
- Use `##` as the top-level heading inside the body (the title lives in the frontmatter).
- Add a link here whenever a document is added.
