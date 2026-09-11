## 1. 3rd：slang 包

- [ ] 1.1 `cmake/thirdparty.json` 新增 slang 包（预编译二进制分发，钉 release tag，`platforms: ["Win32"]`）
- [ ] 1.2 `cmake/thirdparty/FindSlang.cmake`（预编译包 include/lib 引入）
- [ ] 1.3 `python/third_party.py -p Win32 -t slang` 拉取验证；`build_3rd/thirdparty_cache.cmake` 刷新

## 2. ShaderCompilerSlang

- [ ] 2.1 `engine/shader/include/shader/ShaderCompilerSlang.h` + `src/ShaderCompilerSlang.cpp`：`ShaderCompilerBase` 实现；`IGlobalSession` 持有；`ISession` 按 `ShaderCompileOption` 建；`loadModuleFromSourceString` → compose+link → `getEntryPointCode`
- [ ] 2.2 `ShaderLanguage` 加 `SLANG`；`ShaderCompiler` 注册语言→编译器映射
- [ ] 2.3 目标支持：SPIRV（`SLANG_SPIRV`）+ MSL（`SLANG_METAL`）
- [ ] 2.4 Slang program layout → `ShaderReflection` 收敛（space→set；资源类型映射 `ShaderResourceType`）

## 3. spike 测试

- [ ] 3.1 mini fullscreen shader（3 个 ParameterBlock：Global/Pass/Batch 语义 + Texture2D 采样）
- [ ] 3.2 SPIRV 产物断言：非空 + SPIRV-Cross 反射 set/binding 正确
- [ ] 3.3 MSL 产物断言：文本含 `[[buffer(N)]]` argument buffer 形态；不经 SPIRV-Cross
- [ ] 3.4 反射收敛断言：`reflection.resources` 与源码声明一致

## 4. 验证与收尾

- [ ] 4.1 全量 `cmake --build` 通过
- [ ] 4.2 `ShaderCompilerTest` 全绿
- [ ] 4.3 待用户确认后 `openspec archive aurora-slang-spike` 归档本 change
