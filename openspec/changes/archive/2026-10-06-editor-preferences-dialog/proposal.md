## Why

Editor/project settings are currently edited through the docked **"Config"** reflected-form panel, which
mixes a permanent layout slot with what is really a global, modal **Preferences** task. There is no single
place to discover settings, and each subsystem (editor, rendering, audio, input, plugins) has no way to
contribute its own settings page. This change replaces the Config panel with a proper **Preferences
dialog** opened from **File > Preferences…**, contributed to by modules.

## What Changes

- **Preferences dialog**: a modal, engine-drawn dialog (left category list + right page) that hosts
  settings from every contributing module, with **Apply / OK / Cancel** and reset-to-default.
- **File > Preferences…**: add the entry to the File menu; wire it to open the dialog.
- **Contribution model**: modules register preference **pages/sections** (General, Editor, Rendering,
  Audio, Input, Plugins, …) so no subsystem is hard-coded into the shell.
- **Persistence**: per-user settings file (`GetUserConfigPath()/editor-preferences.json`) with
  load-on-start, apply-on-OK, and revert-on-cancel.
- **Retire the docked Config panel**: remove `config` from the default layout (and from the View menu);
  its reflected demo/config content moves into the Editor/General pages.
- **BREAKING** for the current default layout (no `Config` tab) and for anything relying on the `config`
  panel id.

## Capabilities

### New Capabilities
- `editor-preferences`: modal preferences dialog, the module contribution model (pages/sections), and
  per-user settings persistence with apply/cancel semantics.

### Modified Capabilities
- `editor-ui-shell`: add the **File > Preferences…** entry; remove the docked `Config` panel from the
  default layout and its View-menu visibility toggle.

## Impact

- `engine/sandbox/core`: new `preferences/` model — `PreferencePage`/`PreferenceSection`,
  `PreferenceRegistry` (registration by modules), `PreferenceStore` (typed values + JSON v1 + defaults +
  dirty tracking). Headless, UI-free.
- `engine/sandbox/shell`: new `PreferencesDialog` widget (category list + page host) reusing
  `ReflectedFormView`/`UiSkin`; `File > Preferences…` menu item; remove `config` from `kIds`/default.
- `engine/sandbox/module`: register built-in pages (General/Editor/Rendering/Audio/Input/Plugins) and
  load/save the preferences file; `--safe-mode` later.
- Extensions: each module contributes its page via the extension host.
- Persistence: `GetUserConfigPath()/editor-preferences.json`.
- Tests: `PreferenceStore`/`PreferenceRegistry` (headless) + a shell dialog smoke test.
