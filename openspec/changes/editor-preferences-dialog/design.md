## Context

Editor settings are edited today through the docked **"Config"** reflected-form panel (`ReflectedConfigPanel`
deriving from `ReflectedFormView`), which permanently occupies a layout slot and cannot be contributed to
by modules. The editor already has: reflected-form widgets (`ReflectedFormView` + field widgets), an
engine-drawn shell (`MenuBar`/`UiSkin`, UE+Blender styling), a per-user config path
(`GetUserConfigPath()`, already used by `editor_layout.json`), and an extension host where modules register
content. Preferences should reuse these rather than add a new UI stack.

## Goals / Non-Goals

**Goals:** a modal **Preferences** dialog opened from **File > Preferences…**; settings organized into
**categories** contributed by modules (General / Editor / Rendering / Audio / Input / Plugins); per-user
persistence with **Apply/OK/Cancel + Reset to defaults**; retire the docked `Config` panel.

**Non-Goals:** per-project settings, settings search, import/export, live keybinding capture UI (a later
change), `--safe-mode`.

## Decisions

1. **Modal dialog owned by the shell.** A full-window `PreferencesDialog` element (dim backdrop + centered
   panel; left category list, right page host) shown/hidden by shell state. While open it captures input;
   **Esc = Cancel**, **Enter = OK**. The shell never blocks the whole app, only the editor content.
2. **Contribution model lives in `core` (UI-free, headless, tested).** `PreferencePage{ id, title }`,
   `PreferenceSection`, and a typed `PreferenceValue` (bool/int/float/string/color) with defaults;
   modules register pages into a `PreferenceRegistry`. No UI types in `core`.
3. **Persistence per user.** `GetUserConfigPath()/editor-preferences.json` (versioned, key→value). Loaded
   at startup into the store; **OK/Apply** writes the working copy, **Cancel** reverts it, **Reset**
   restores defaults. Unknown keys tolerated (forward-compatible).
4. **Pages render with the existing widgets.** Each page is a `sky::ui` view built from its section
   descriptors using `UiSkin` rows (checkbox / slider / text / combo / color), reusing the reflected-form
   look; the dialog chrome is a thin frame, not a docked panel.
5. **File menu command.** Add **"Preferences…"** to `File` (with a shortcut); remove the docked `Config`
   panel from the default layout and its View-menu toggle.
6. **Layering mirrors the existing editor.** `core` = model/registry/store; `shell` = dialog + page
   widgets + menu wiring; `module` = persistence + registering built-in pages + extension contributions.

## Risks / Trade-offs

- **[Modal input/focus]** → the dialog must reliably capture pointer/keyboard over the dock and restore
  focus on close; test open/cancel/apply paths headlessly where possible.
- **[Retiring `config`]** → update the default layout and View menu; keep the reflected demo/config content
  reachable via the Editor/General pages.
- **[Persistence drift]** → versioned JSON + tolerant loader, like the layout file.
- **[Scope]** → ship General/Editor/Rendering pages first; Audio/Input/Plugins can land incrementally
  behind the same contribution model.

## Migration Plan

Build/test-green increments: (1) `core` preferences model/registry/store + tests; (2) `PreferencesDialog`
chrome + a General page; (3) File > Preferences… wiring; (4) retire `config` from the default layout/menu;
(5) built-in module pages + persistence; (6) docs. Rollback: revert; the preferences file is ignored if
absent.
