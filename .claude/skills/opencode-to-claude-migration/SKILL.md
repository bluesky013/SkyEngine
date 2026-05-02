---
name: opencode-to-claude-migration
description: Port OpenCode skills, slash commands, and agent assets from .opencode/ into Claude Code's .claude/ layout, with frontmatter, tool-name, and slash-command adaptations applied during the copy.
compatibility: claude
metadata:
  audience: contributors
  source: project-local migration playbook
---

# OpenCode → Claude Code Migration

Use this skill when the user wants to make existing OpenCode assets (skills under `.opencode/skills/`, commands under `.opencode/command/`) usable inside Claude Code, or when adding a new asset that should live in both worlds.

## Goal

Produce a working `.claude/` layout whose assets behave identically in Claude Code to how the OpenCode originals behave in OpenCode, fixing references that differ between the two harnesses during the copy step.

## Core decisions

- **Copy, do not symlink.** Symlinks save duplication but freeze the two copies to the same content; the harnesses use slightly different tool/slash names, so each copy needs its own edits.
- **Keep `.opencode/` as the OpenCode-side source.** Edits to Claude-side files do not flow back; if a rule must apply in both harnesses, update both.
- **Skip OpenSpec port.** OpenSpec ships native Claude integration (`openspec init --tools claude`), which writes `.claude/skills/openspec-*/` and `.claude/commands/opsx/*.md` for you. Do not hand-port `openspec-*` skills.

## Layout mapping

| Concept | OpenCode path | Claude Code path |
|---|---|---|
| Project skill | `.opencode/skills/<name>/SKILL.md` | `.claude/skills/<name>/SKILL.md` |
| Project slash command | `.opencode/command/<name>.md` | `.claude/commands/<name>.md` (or nested as `.claude/commands/<group>/<name>.md` → `/<group>:<name>`) |
| Agent definition | `.opencode/agent/<name>.md` | `.claude/agents/<name>.md` |
| Project memory | `AGENTS.md` (project root) | `CLAUDE.md` (project root) |

## Workflow

### Step 1 — Inventory

List the source assets and decide what is in scope:

```bash
ls .opencode/skills/
ls .opencode/command/ 2>/dev/null
ls .opencode/agent/   2>/dev/null
```

Pick out any `openspec-*` skills and exclude them from the manual port — they will be re-installed natively.

### Step 2 — Copy each asset

For each in-scope skill:

```bash
mkdir -p .claude/skills
cp -R .opencode/skills/<name> .claude/skills/<name>
```

Do the same for commands (mind the layout change — flat `.opencode/command/foo.md` becomes `.claude/commands/foo.md`; nested groups become `/group:command` slash invocations).

### Step 3 — Apply frontmatter adaptations

Claude Code's required frontmatter keys: `name`, `description`. Unknown keys are tolerated, but clean them up while you are here.

- Replace `compatibility: opencode` with `compatibility: claude` (or remove the line). Use a `$`-anchored regex without trailing `\s*` to avoid eating the newline:
  ```bash
  perl -i -pe 's/^compatibility: opencode$/compatibility: claude/' .claude/skills/<name>/SKILL.md
  ```
- Confirm `name` matches the directory name and is kebab-case.
- Keep `description` short (a single sentence; ≪ 1024 chars).

### Step 4 — Apply content adaptations

Search the body of each migrated file for OpenCode-specific identifiers and rewrite them:

| OpenCode wording | Claude Code wording |
|---|---|
| ``the `explore` subagent`` / `explore subagent` | ``the `Explore` agent (Agent tool with `subagent_type: "Explore"`)`` |
| `subagent_type: "general-purpose"` | unchanged — both harnesses accept this name |
| `Task tool` (with `subagent_type: ...`) | `Agent tool` (with `subagent_type: ...`) |
| `TodoWrite tool` | use the task tracking tools (`TaskCreate` / `TaskUpdate`); often the line can simply be removed |
| `AskUserQuestion tool` | unchanged — Claude Code has it (deferred-loaded) |
| `Skill tool` | unchanged — Claude Code has it |
| Slash refs like `/opsx-apply` | OpenSpec/Claude form is `/opsx:apply` (group:command); for non-OpenSpec commands, mirror your own `.claude/commands/` layout |
| Example commit messages or doc text mentioning "OpenCode" | rewrite to neutral or Claude-flavored examples to avoid factual drift |

Quick scan command:

```bash
grep -nEi "(opencode|todowrite|task tool|/opsx-|explore subagent|\bsubagent\b)" .claude/skills/<name>/SKILL.md
```

### Step 5 — OpenSpec native install (if applicable)

If the project uses OpenSpec, install the Claude-native version of those four skills + four slash commands instead of porting them:

```bash
openspec init --tools claude
```

This writes:

- `.claude/skills/openspec-{apply-change,archive-change,explore,propose}/SKILL.md`
- `.claude/commands/opsx/{apply,archive,explore,propose}.md` (invoked as `/opsx:apply`, etc.)

Do not symlink or hand-port the OpenCode `openspec-*` skills — let `openspec` own the Claude side.

### Step 6 — Verify

- `grep -RH '^name:' .claude/skills/*/SKILL.md` — every skill has frontmatter
- `grep -RH '^description:' .claude/skills/*/SKILL.md`
- Re-run the Step 4 scan; expect no matches
- Restart Claude Code so the harness picks up the new skills and slash commands

## Working rules

- Migrate one skill at a time when in doubt; verify each before moving on.
- Never edit OpenSpec-generated `.claude/skills/openspec-*/` by hand — re-run `openspec init --tools claude` (or `openspec update`) instead.
- Preserve example code, file paths, type names, and CLI flags exactly as they appear in the source skill — only adapt harness-specific identifiers.
- When a skill genuinely needs different content for OpenCode vs Claude (different examples, different available tools), keep the two copies divergent and accept duplication.
- If a copied skill ends up identical to its OpenCode source after adaptation, that is fine — duplication is the cost of harness independence.

## Common pitfalls

- **Greedy `\s*$` in `perl -i -pe`** ate the newline after `compatibility: opencode` and glued the next key to it. Use `^compatibility: opencode$` (no `\s*`) to keep the line break.
- **Symlink rot.** A `.claude/skills/foo` symlink to `.opencode/skills/foo` "works" until you tweak it for Claude — then your edits leak back into OpenCode. Copy, do not symlink.
- **Forgetting the OpenSpec native command.** Hand-porting OpenSpec skills duplicates work and locks you to a stale snapshot of OpenSpec's instructions.
- **Slash-command path layout.** Claude Code groups commands by subdirectory (`.claude/commands/group/cmd.md` → `/group:cmd`); OpenCode flattens them. If your skill body references slash commands, match the new path layout.
- **Frontmatter newline loss.** After bulk regex edits, `grep '^compatibility:'` on every file should print clean lines, not lines glued to the next key.

## Output convention

When this skill runs to completion, summarize:

- Number of skills/commands copied
- Number of OpenSpec skills installed natively
- Files edited beyond a straight copy (with one-line reason for each)
- Reminder to restart Claude Code for slash commands to register
