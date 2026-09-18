## 1. 追加 slang.patch hunk

- [x] 1.1 `cmake/patches/slang.patch` 追加 hunk：把 `if(SLANG_ENABLE_DXIL) include(FetchDXC) endif()` 替换为从 `${3RD_PATH}/dxcompiler/bin` 拷贝 `dxcompiler.dll` + `dxil.dll`（保留 `copy-dxcompiler`/`copy-dxil` target 名）

## 2. 重建 slang + 验证

- [x] 2.1 `python third_party.py -p Win32 -t slang -f` 重建（确认 configure 无 `Downloading DXC` 日志）
- [x] 2.2 重编 `AuroraShaderTest` 并跑 `CompileDxilForDx12` + `AuroraD3D12Test.*`，确认 DXIL 仍绿

## 3. aurora shader 声明 dxcompiler 运行时依赖

- [x] 3.1 `Finddxcompiler.cmake`：`INTERFACE_DYN_LIBS` 暴露 `dxcompiler.dll` + `dxil.dll`（原先只暴露 dxcompiler.dll）
- [x] 3.2 `engine/aurora/shader/CMakeLists.txt`：Windows 下追加 `3rdParty::dxcompiler` 依赖（`NOT TARGET` 守卫避免与 engine/shader 重复 find）
- [x] 3.3 验证：重编后 `dxcompiler.dll` + `dxil.dll` 自动拷贝到 `output/bin/Debug`，`AuroraShaderTest` 全绿

## 4. 收尾

- [ ] 4.1 `openspec archive aurora-slang-reuse-dxc` 归档（需用户确认）
