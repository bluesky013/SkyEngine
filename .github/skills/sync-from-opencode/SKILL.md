---
name: sync-from-opencode
description: >
  Sync skills from .opencode/skills/ into .github/skills/ for GitHub Copilot.
  Use when the user wants to import, sync, update, or mirror OpenCode skills
  into the Copilot skill directory. Handles frontmatter adaptation between formats.
---

# Sync Skills from OpenCode

Import or update skills from `.opencode/skills/` into `.github/skills/`, adapting
frontmatter from OpenCode format to Copilot format.

## Source and Target

| Role   | Path                  | Format owner |
|--------|-----------------------|--------------|
| Source | `.opencode/skills/`   | OpenCode     |
| Target | `.github/skills/`     | Copilot      |

## Frontmatter Translation

OpenCode and Copilot skills both use `SKILL.md` with YAML frontmatter, but the
fields differ slightly. Apply these conversions when copying:

### OpenCode → Copilot field mapping

| OpenCode field          | Copilot field           | Notes                                    |
|-------------------------|-------------------------|------------------------------------------|
| `name`                  | `name`                  | Keep unchanged                           |
| `description`           | `description`           | Keep unchanged                           |
| `compatibility: opencode` | *(remove)*            | Not needed in Copilot skills             |
| `license`               | `license`               | Keep if present                          |
| `metadata.audience`     | `metadata.audience`     | Keep as-is                               |
| `metadata.source`       | `metadata.source`       | Keep as-is                               |
| `metadata.author`       | `metadata.author`       | Keep if present                          |
| `metadata.version`      | `metadata.version`      | Keep if present                          |
| `metadata.generatedBy`  | `metadata.generatedBy`  | Keep if present                          |

### Copilot-specific additions

When syncing a skill that has no `license` field, do NOT add one.
Do NOT invent or fabricate any frontmatter fields that are not present in the source.

### Example conversion

**OpenCode format:**
```yaml
---
name: skyengine-coding-rules
description: Enforce SkyEngine C++ coding rules...
compatibility: opencode
metadata:
  audience: contributors
  source: project conventions
---
```

**Copilot format:**
```yaml
---
name: skyengine-coding-rules
description: Enforce SkyEngine C++ coding rules...
metadata:
  audience: contributors
  source: project conventions
---
```

## Procedure

### Step 1 — Inventory both directories

List skills in both locations:

```
.opencode/skills/   → source skills
.github/skills/     → existing target skills
```

Classify each source skill into one of:
- **New**: exists in `.opencode/skills/` but NOT in `.github/skills/` → will be created
- **Existing**: exists in both → will be compared and updated if different
- **Skipped**: `README.md` and other non-skill files → ignore

Present the classification to the user for confirmation before proceeding.

### Step 2 — Sync each skill

For each skill to sync (new or updated):

1. Read the source `.opencode/skills/<name>/SKILL.md`
2. Transform the frontmatter:
   - Remove `compatibility: opencode` line
   - Keep all other fields intact
3. Body content: copy as-is (do NOT modify the instruction text)
4. Write to `.github/skills/<name>/SKILL.md`
   - If the skill is **new**, create the directory and file
   - If the skill **exists**, compare content:
     - If identical after frontmatter normalization → skip (report "already up to date")
     - If different → show a diff summary and ask the user whether to overwrite

### Step 3 — Copy auxiliary files

If the source skill directory contains files besides `SKILL.md` (scripts, references,
templates), copy them to the target directory as well. Preserve relative paths.

### Step 4 — Report results

Summarize:
```
Synced:     <list of skills created or updated>
Skipped:    <list of skills already up to date>
New:        <count> skills added
Updated:    <count> skills updated
```

## Edge Cases

- **Shared openspec skills**: Skills like `openspec-apply-change` may exist in both
  directories with slightly different content (e.g., different tool names). When the
  body content differs, show a brief diff summary and let the user decide.
- **Copilot-only skills**: Skills in `.github/skills/` that do NOT exist in
  `.opencode/skills/` are unaffected. Never delete them.
- **Large skills**: If a skill body exceeds 500 lines, note it but still sync. Copilot
  will use progressive loading.
