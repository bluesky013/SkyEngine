---
name: skyengine-commit-format
description: Write SkyEngine commit messages using the repository's common prefix style, especially feat, fix, 3rd, build, refactor, doc, tool, and platform.
metadata:
  audience: contributors
  source: git-history-derived
---

# SkyEngine Commit Format

Use this skill whenever you need to draft, review, or suggest a git commit message for this repository.

## Goal

Match the repository's existing commit style instead of defaulting to generic Conventional Commits.

## Core rules

- Prefer a short prefix that reflects the change scope.
- Use the repository's existing common prefixes first: `feat`, `fix`, `3rd`, `build`.
- Other observed prefixes that are acceptable when they fit better: `refactor`, `doc`, `tool`, `platform`, `sample`, `assets`, `bug`, `editor`.
- Keep the subject concise and action-oriented.
- English commit subjects are the norm in this repository.
- Do not invent a new prefix if an existing one already fits.

## Prefix guide

### `feat`
Use for new user-facing or developer-facing functionality.

Examples:

- `[feat]: Update PBR Shader.`
- `[feat]: update 3rdparty`
- `[feat]: ReverseZ`

### `fix`
Use for bug fixes, regressions, broken builds, or corrections to existing behavior.

Examples:

- `fix: mac os build.`
- `[fix]: fix release compile issue.`
- `[fix]: DescriptorSet update.`

### `3rd`
Use for third-party dependency changes, patches, package updates, submodule/vendor changes, or external library build logic.

Examples:

- `[3rd]: update 3rd cmake`
- `[3rd]: rewrite 3rd build script. add readme.`
- `[3rd]: add node editor`

### `build`
Use for build system logic, build configuration, CI/build pipeline behavior, or generator/config changes that are primarily build-related.

Examples:

- `[build]: plugin system driven by JSON config`
- `fix: Add DS_Store` *(if the practical effect is to fix repo/build hygiene rather than feature behavior)*

## Secondary prefixes

- `refactor` — internal structure cleanup without intended behavior change
- `doc` — documentation-only updates
- `tool` — developer tooling or scripts
- `platform` — platform-specific support or fixes
- `sample` — sample/demo changes
- `assets` — asset-only updates
- `editor` — editor-specific work when that scope is clearer than `feat`
- `bug` — older history uses this; prefer `fix` for new commits unless matching nearby history is important

## Formatting preference

This repository has mixed historical formatting, but these patterns are all seen:

- `fix: message`
- `[fix]: message`
- `[feat]: message`
- `Plain sentence without prefix`

For new commits, prefer one of these two styles for consistency and clarity:

1. `fix: short message`
2. `[feat]: short message`

If you are matching nearby history in a touched area, mirror the local style.

## Selection checklist

Before finalizing a commit message, ask:

1. Is this a new capability? → `feat`
2. Is this repairing behavior or build breakage? → `fix`
3. Is this changing vendored or external dependencies? → `3rd`
4. Is this primarily build/config/pipeline logic? → `build`
5. Is this only restructuring? → `refactor`
6. Is this only documentation? → `doc`

## Good examples

- `feat: add OpenCode skills for SkyEngine docs`
- `fix: restore macOS build flags`
- `3rd: update glslang patch handling`
- `build: simplify third-party configure flow`
- `doc: add OpenCode skills README`

## Avoid

- Vague subjects like `update`, `fix issue`, or `change code`
- Prefixes not already used in the repository when a common one fits
- Overly long explanations in the subject line
- Mixing multiple unrelated concerns into one commit message

## Default recommendation

When unsure, choose from this order:

1. `fix`
2. `feat`
3. `3rd`
4. `build`

Then write a short, specific subject describing the actual change.
