---
title: "SkyEngine Documentation"
description: "Index of SkyEngine technical documentation."
updated: "2026-09-22"
---

## SkyEngine Documentation

Technical documentation for engine contributors and technical users. All documents are written in English, in
Markdown, and grounded in the current repository state.

### Plugins

- [PVS Plugin](plugins/pvs.md) - precomputed visibility: render-independent core, runtime culling glue, streaming,
  visibility queries and serialization.

### Adding documents

- One topic per file; group by `architecture/`, `modules/`, `plugins/`, `features/`, or `guides/`.
- Start each file with YAML frontmatter (`title`, `description`, optional `module`, `updated`).
- Use `##` as the top-level heading inside the body (the title lives in the frontmatter).
- Add a link here whenever a document is added.
