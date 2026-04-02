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
  - General SkyEngine C++ conventions and prohibited patterns that are broader than clang tooling (e.g., no `dynamic_cast`, ASCII-only source, member naming)
- `skyengine-clang-style/`
  - Repository-aligned `.clang-format` and `.clang-tidy` policy translated into explicit authoring and review rules
- `skyengine-architecture-review/`
  - Review changes against SkyEngine's current module layering, plugin boundaries, runtime/editor split, and render-vs-logic separation
- `skyengine-pc-gpu-review/`
  - Review desktop rendering changes for PC GPU cost, backend fit, pass integration, and `tex_pc` asset-path correctness
- `skyengine-mobile-gpu-review/`
  - Review Android/iOS rendering changes for mobile GPU scalability, backend compatibility, fallback design, and `tex_mobile` asset-path correctness
- `skyengine-cpu-review/`
  - Review CPU cost, hot-path ownership, profiling/SIMD assumptions, and plugin/runtime gating in frame-critical code
- `skyengine-cpp-file-creation/`
  - New C/C++ file authoring rules, including ASCII-only content, file preamble style, and correct logger usage
- `docs-generate/`
  - Generate or update English technical documentation under `docs/`, with optional local PDF export to `docs/pdf/`
- `vulkan-spec-quick-index/`
  - Fast topic map for the Vulkan spec, with lookup keywords, common navigation paths, and official reference entry points

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
