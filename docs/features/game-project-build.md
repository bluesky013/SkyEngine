---
title: "Game Project Build & Packaging"
description: "Creating a game project, building its launcher/module, generating platform IDE projects, and the product-bundle packaging model."
updated: "2026-10-12"
---

## Game Project Build & Packaging

A SkyEngine project is a directory holding the descriptor (`<name>.skyproj`), the source assets (`assets/`),
configuration (`configs/`), a cache (`cache/`), and a single native codebase under `native/`. The game logic ships
as a runtime **module**: a shared library exporting `StartModule`/`StopModule` (macro `REGISTER_MODULE`), loaded by
`ModuleManager`. The runtime host (`Launcher`) is **project-owned** — the engine no longer ships a launcher; it ships
**templates**.

### Creating a project

`python/package_project.py create` is the single implementation (the editor Project Manager shells out to it):

- `engine/templates/game/` — the game skeleton (`native/` CMake entry + module)
- `engine/templates/launcher/<platform>/` — the platform launcher entry, copied into `native/<platform>`

```
<Project>/
  <name>.skyproj  export_presets.json  .gitignore
  native/                       # the single native codebase
    CMakeLists.txt              # independent engine-consuming entry
    CMakePresets.json
    source/                     # game module <Name>Module
    android/  cpp/ java/<pkg>/ res/     # platform launcher entry (copied per platform)
    ios/  macos/  windows/      # platform launcher entry
  assets/ configs/ cache/
  build/<platform>/             # generated build/IDE projects + artifacts
```

The project consumes the engine as a library (its `native/CMakeLists.txt` includes the engine `cmake/*` helpers and
adds the engine + plugin targets) rather than adding the engine's root project — no symlinks, no copied engine
launcher. The target name stays `Launcher`, so engine/plugin `DEPENDENCIES Launcher` registrations keep working.

### Building the launcher and module

```bash
python3 python/package_project.py generate --project <p>.skyproj --preset <name>
cmake --build --preset <platform>      # in <Project>/native
```

Desktop output lands in `<Project>/build/<platform>/bin`: the launcher artifact is named after the **project
identifier** (`<Name>.exe` / `<Name>` / `<Name>.app`) and the game module is `<Name>Module` (`.dylib`/`.so`/`.dll`).
Point `SKY_ENGINE_ROOT` at the engine repository and bootstrap third-party with `python/third_party.py -p <platform>`.
On Win32 the preset is provided but build/run verification is expected on a Windows host.

### Application identity, export presets, and signing

The descriptor's `settings` holds the **default identity** only (nothing secret):

```json
"settings": { "rhi": "vulkan", "applicationId": "com.example.game", "versionName": "1.0", "versionCode": 1 }
```

`applicationId` maps to the Android `applicationId`/`namespace` and the Apple `CFBundleIdentifier`; when unset it
defaults to `com.skyengine.<sanitized-name>`. The Android **namespace / Java package** is fixed at create time (it
is baked into `native/android/java`); a preset `bundleId` only overrides the install `applicationId`.

Per-platform options live in **`export_presets.json`** (committed); secrets live in **`export_credentials.json`**
(never committed), like Godot's `export_presets.cfg` / `export_credentials.cfg`. Credentials resolve **env > file**.

```json
// export_presets.json (committed)
[ { "name": "Game-android", "platform": "android", "variant": "release", "bundleId": "com.acme.game",
    "abis": ["arm64-v8a"], "keystore": "release", "compileSdk": 34, "minSdk": 29, "targetSdk": 33 },
  { "name": "Game-ios",     "platform": "ios",   "variant": "release", "signing": "auto" },
  { "name": "Game-macos",   "platform": "macos", "variant": "release", "bundle": true, "signing": "off" } ]

// export_credentials.json (not committed)
{ "android": { "release": { "keystorePath": "~/.keys/acme.jks", "storePassword": "...",
                            "keyAlias": "release", "keyPassword": "..." } },
  "apple":   { "teamId": "...", "signingIdentity": "...", "provisioningProfile": "..." } }
```

Preset fields: `bundleId` (defaults to the project app id), `versionName`/`versionCode`, `variant`; **Apple**
`signing` (`auto` uses the credentials/team, `off` builds unsigned), `bundle` (macOS `.app`),
`team`/`signingIdentity`/`provisioningProfile` (override credentials); **Android** `abis`/`keystore`/`compileSdk`/
`minSdk`/`targetSdk`/`ndkVersion`; plus `cacheVariables`, `compileDefinitions`, `compileFlags`,
`extraConfigureArgs`, `extraBuildArgs`. These are editable from the editor Project Manager via the reflection panel.

Env overrides: `SKY_ANDROID_KEYSTORE`, `SKY_ANDROID_STORE_PASSWORD`, `SKY_ANDROID_KEY_ALIAS`,
`SKY_ANDROID_KEY_PASSWORD`, `SKY_APPLE_TEAM_ID`, `SKY_APPLE_SIGNING_IDENTITY`, `SKY_APPLE_PROVISIONING_PROFILE`.

### Generating platform projects and packaging

```bash
python3 python/package_project.py generate --project <p>.skyproj --preset <name>
python3 python/package_project.py package  --project <p>.skyproj --preset <name>
```

- **Android**: instantiates `engine/templates/ide/android/` into `<Project>/build/android`, referencing `native/`
  (`externalNativeBuild` → `../../../native/CMakeLists.txt`; `java/res.srcDirs` → `../../../native/android/…`); the
  native library is `lib<Name>.so` (`loadLibrary("<Name>")` + manifest `lib_name` are tokenized). `package` runs the
  bundled gradle wrapper (`:app:assemble<variant>`) and copies the APK to `<Project>/build/android/<Name>-<variant>.apk`.
  It opens directly in Android Studio.
- **iOS / macOS**: `cmake --preset <platform>` in `native/` produces the `.xcodeproj` under `<Project>/build/<platform>`;
  the project launcher consumes `SKY_BUNDLE_ID` / `SKY_APPLE_TEAM_ID` / `SKY_APPLE_SIGNING_IDENTITY` /
  `SKY_APPLE_PROVISIONING_PROFILE`.

Android build prerequisites (from the environment):

- `ANDROID_HOME` (or `ANDROID_SDK_ROOT`) with platform/build-tools and `ANDROID_NDK_HOME` (NDK `28.1.13356709`).
- `SKY_3RD_PATH` pointing at the Android third-party output (`python/third_party.py -p Android`).
- `JAVA_HOME` with a Gradle-compatible JDK (Gradle 8.10.2 supports up to Java 22; JDK 17/21 recommended). Some
  IDE-bundled runtimes are too new (e.g. a Java 25 JBR fails with `Unsupported class file major version 69`).

### Module packaging (dynamic / static)

Runtime modules are built and loaded two ways, selected by an option / export preset:

- **dynamic** (default): each module is a shared library (`REGISTER_MODULE`), loaded by `ModuleManager` via dlopen.
- **static** (`SKY_MODULE_PACKAGING=static`, preset `packing`): modules are compiled into the host. Each uses
  `REGISTER_MODULE_TAGGED(tag, name)` producing a unique `sky_module_create_<tag>()` factory (tag = target name with
  `.`/`-` -> `_`). At configure time `sky_generate_static_modules()` emits `RegisterStaticModules.cpp` from the
  modules declared via `sky_add_module()`; the project launcher links those static modules plus the generated cpp,
  and `Application` calls `SkyEngineRegisterStaticModules(ModuleManager&)` before `LoadModules`. Static bundling also
  applies on platforms where dlopen of module code is restricted (e.g. iOS).

Modules opt in by declaring their target with `sky_add_module(...)` (instead of `sky_add_library(... SHARED)`) and
using `REGISTER_MODULE_TAGGED`.

### Packaging ships cooked products

A shipped project packages the target platform's **cooked product bundles** (`product.index` plus `<uuid>.bin`
payloads; see [Asset Pipeline](asset-pipeline.md)) together with the game module and engine bundles. The raw
`assets/` sources are **not** shipped. The runtime loads product bundles only (engine `common`/`tex_*` plus the
project's cooked bundle) and requires no source catalog.

The project launcher's `--project <dir>` is a **development** mode: it mounts the project tree (including `assets/`)
and relies on on-demand cook in the editor. It is a convenience, not the shipping layout.

### Asset cook paths

Assets are authored under `assets/` and shipped as **cooked product bundles** (see [Asset Pipeline](asset-pipeline.md)).
Cook is performed by `AssetBuilderManager` (builders registered by modules such as `Aurora.Cook`); three entry points exist:

- **Editor on-demand**: the editor cooks a asset when it is first loaded (source catalog resolves missing products).
- **Out-of-process worker**: `tools/asset_tool` (target `AssetTool`) is a headless worker that mounts the project +
  engine filesystems, loads the builder modules, and serves cook requests over the length-prefixed frame protocol
  (`asset-cook-ipc`) when the host selects the out-of-process runner.
- **Headless batch (`--cook-all`)**: the same tool with `--project <dir> --engine <bundle> --cook-all --platform <p>`
  cooks every source into `<project>/products/` and exits (used by `python/package_project.py` before packaging, and
  the `--migrate` one-shot uses the same bootstrap).

### Product bundles in a package

A package ships only the bundles a **distribution** needs (an export preset), so a platform can ship several packages
without cross-tier UUID conflicts (quality tiers use distinct bundle names, e.g. `tex_low` vs `tex_hd`):

- Bundles are staged under `products/<bundle>/`:
  - **Android** → the APK's `assets/products/<bundle>/` (read at runtime via the APK asset filesystem).
  - **desktop / iOS** → `products/<bundle>/` next to the launcher (`.app` → `Contents/Resources/products/`).
- `package`/`generate` first run the headless cook worker (`tools/asset_tool` -> `AssetTool
  --project <dir> --engine <bundle> --cook-all --platform <p>`) to cook `<Project>/assets/` into `<Project>/products/`.
- `package_project.py` copies `<Project>/products/<bundle>` for each selected bundle and writes
  **`products/bundles.json`** (`{ platform, distribution, bundles: [...] }`). Selection comes from the preset's
  `bundles` list, else the cook config's distribution.
- `GameApplication` reads `products/bundles.json` and mounts **exactly** those bundles (no filesystem enumeration);
  when the manifest is absent it falls back to `common`. `--project` dev mode skips product bundles.
