## 1. Resource root

- [x] 1.1 Create `engine/sandbox/resources/` with `icons/` and `fonts/`, and move the editor font into it
- [x] 1.2 Keep it separate from the project-package `assets/`

## 2. Resolution

- [x] 2.1 Add `SandboxResources` (`Root()` / `Resolve()`) in `EditorCore`, based on the platform bundle path
- [x] 2.2 Load the editor font through `SandboxResources` and link `EditorCore` from `EditorRender`

## 3. Deployment

- [x] 3.1 Add `deploy_resources.cmake`: remove the previous deployment (symlink or tree) without touching the
      source, then symlink with an automatic copy fallback
- [x] 3.2 Hook the deploy script into the `SandboxEditor` post-build

## 4. Verify

- [x] 4.1 Build `SandboxEditor` and confirm the resources are deployed to `output/bin/<config>/resources`
- [x] 4.2 Run the editor from the output directory and confirm the font loads (no block-glyph fallback)
