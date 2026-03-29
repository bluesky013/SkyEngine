---
description: Generate or update engine technical documentation (architecture, modules, features)
---

Generate engine documentation — architecture overviews, module references, feature specs, and design guides.

Output Markdown to `docs/` and optionally PDF to `docs/pdf/`.

---

**Input**: Specify the documentation scope — e.g., "full", a module name like "render", a plugin name like "opendrive", or a feature like "shader system".

**Steps**:

1. Read the `docs-generate` skill for conventions and templates.
2. Research the relevant source code thoroughly before writing.
3. Generate documentation following the skill's writing rules and templates.
4. Update `docs/README.md` index.
5. If PDF requested, export via pandoc.
