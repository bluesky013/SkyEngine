## 1. Core: preferences model (UI-free, headless)

- [x] 1.1 Add `editor/core/preferences/` types: `PreferenceValue` (bool/int/float/string/color variants), `PreferenceSection`, `PreferencePage`, stable string keys, and defaults.
- [x] 1.2 Add `PreferenceRegistry` so modules register pages/sections/entries (no UI types).
- [x] 1.3 Add `PreferenceStore`: current/working/default values, `Set/Get` by typed key, `MarkDirty`, `Commit`/`Revert`/`ResetToDefaults`, and an enumeration of changed entries for apply.
- [x] 1.4 Add JSON v1 serialization for the store (`ToJson`/`FromJson`) tolerant of unknown keys.
- [x] 1.5 Tests: registry ordering, typed defaults, commit/revert/reset, JSON round-trip + forward-compat load.

## 2. Shell: Preferences dialog

- [x] 2.1 Add a `PreferencesDialog` widget: dim backdrop + centered panel with a left category list and a right page host; show/hide from shell state.
- [x] 2.2 Modal input: while open, the dialog captures pointer/keyboard; Esc = Cancel, Enter = OK; restore focus on close.
- [x] 2.3 Render each page from its section descriptors (checkbox / slider / text / combo / color).
- [x] 2.4 OK/Apply/Cancel/Reset buttons wired to the store's commit/revert/reset; per-page reset.
- [x] 2.5 Headless `EditorShellTest` smoke for open/close and page switching.

## 3. File menu and retiring Config

- [x] 3.1 Add `File > Preferences…` to the `MenuBar` and open the dialog.
- [x] 3.2 Remove the docked `config` panel from the default layout and from the View menu.
- [ ] 3.3 Move any still-needed Config/refldemo content into the appropriate page. (Deferred.)

## 4. Built-in pages, wiring and persistence

- [x] 4.1 General page (language/theme/autosave); Editor page (grid/snap/undo depth).
- [x] 4.2 Rendering page (vsync, resolution scale).
- [ ] 4.3 Audio / Input / Plugins pages (contributed via the extension host; incremental).
- [x] 4.4 Persistence: load `GetUserConfigPath()/editor-preferences.json` at startup into the store; write on commit.
- [ ] 4.5 Apply committed values to the running subsystems where supported. (Deferred; commit persists.)

## 5. Documentation

- [ ] 5.1 Update `docs/editor/editor-framework-design.md` (Preferences section) and the status doc.
