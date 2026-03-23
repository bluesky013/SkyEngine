# OpenCode Skills

This directory contains project-local OpenCode skills for SkyEngine.

## Layout

- `skyengine-project-reference/`
  - Repository structure, target map, dependency overview, and build flags
- `skyengine-build-reference/`
  - Recommended build order and command templates for third-party setup, CMake configure, build, and optional tests
- `skyengine-commit-format/`
  - Repository-aligned commit prefix guidance for `feat`, `fix`, `3rd`, `build`, and related commit styles
- `skyengine-coding-rules/`
  - General C++ coding conventions and prohibited patterns (e.g., no `dynamic_cast`)
- `docs-generate/`
  - Generate or update English technical documentation under `docs/`, with optional local PDF export to `docs/pdf/`

Each skill lives in its own folder and must expose `SKILL.md`.

## Loading

OpenCode discovers project-local skills from:

```text
.opencode/skills/<name>/SKILL.md
```

Agents can load them with the `skill` tool by name.

## Maintenance

- Keep project-local skills as the primary maintained agent-facing documentation.
- Update the skill version first when changing agent-facing instructions.
- Preserve exact paths, commands, and option names from the repository when editing skill content.
