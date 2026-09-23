## 1. Editor module config

- [x] 1.1 `EditorApplication::LoadConfigs` reads `configs/modules_editor.json` (bundle path) and registers its
      modules; falls back to `SandboxModule` when the file is missing

## 2. Launcher editor mode

- [x] 2.1 `Win32Launcher` accepts `--app editor` and runs the editor host (guarded by `SKY_EDITOR_HOST`)
- [x] 2.2 Launcher CMake compiles the editor host sources when `SKY_BUILD_SANDBOX` and defines `SKY_EDITOR_HOST`
- [x] 2.3 Deploy the editor module + `resources/` next to the launcher when the sandbox editor is built

## 3. Game mode boot fix (found while verifying)

- [x] 3.1 Set the work file system to the bundle path in `GameApplication::Init` outside editor mode (the Win32
      `#else` branch left it null, so `LoadConfigs` dereferenced null and the launcher segfaulted) and guard the
      config open against a null work FS

## 4. Verify

- [x] 4.1 `Launcher --app editor` runs the editor (Vulkan and DX12; loads modules from `configs/modules_editor.json`)
- [x] 4.2 `Launcher` game mode boots (loads `AuroraRender`, creates the device, no crash) and `SandboxEditor` still
      runs
