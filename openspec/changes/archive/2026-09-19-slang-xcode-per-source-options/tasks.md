## 1. 修复 slang 源码并更新 patch

- [x] 1.1 `source/slang/CMakeLists.txt`：`RICH_DIAG_SIZE_OPT_FLAGS` 按 `MSVC` / `GNU` / 其他（Clang）三分支改写，非 MSVC 分支不含 `$<CONFIG>`
- [x] 1.2 checkout 内 `git diff` 再生成 `cmake/patches/slang.patch`，`git apply --check` 验证

## 2. 按引擎三方流程重建 slang

- [x] 2.1 `python3 python/third_party.py -p MacOS-x86 -t slang -f`：configure 不再报 Xcode per-config 错误，Debug + Release 编译、install、拷贝到 `build_3rd/MacOS-x86/slang` 全部成功
- [x] 2.2 产物归档更新为 `thirdparty_MacOS-x86_e97d2deae540.zip`，`thirdparty.json` md5 同步

## 3. 收尾

- [x] 3.1 归档 change，Xcode per-source COMPILE_OPTIONS 限制同步进 `aurora-slang-dxc` spec
