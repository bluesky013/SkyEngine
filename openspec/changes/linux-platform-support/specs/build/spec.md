## ADDED Requirements

### Requirement: slang 三方包支持 Linux 构建

`cmake/thirdparty.json` 的 slang 条目 SHALL 在 Linux 下不携带 Win32 专属构建参数，并通过 `options_Linux` 设置 `SLANG_ENABLE_DXIL=OFF`（Linux 走 SPIR-V，不触发 dxc 获取链）。

`python/third_party.py` SHALL 提供 `options_<platform>` 条件合并：包条目可携带 `options_<platform>`，引导时合并覆盖在 `options` 之上；无该字段时行为不变。

`python3 python/third_party.py -p Linux -o <out>` SHALL 为 slang 产出 `<out>/Linux/slang/lib/Release/libslang.so`。

#### Scenario: Linux 引导产出 libslang.so
- **WHEN** 运行 `python3 python/third_party.py -p Linux -o build_3rd -t slang`
- **THEN** 构建成功；`build_3rd/Linux/slang/lib/Release/libslang.so` 存在

#### Scenario: aurora shader 模块在 Linux 通过 configure
- **WHEN** `3RD_PATH` 指向 `build_3rd/Linux` 配置引擎 CMake
- **THEN** `sky_find_3rd(slang)` 找到目标且 aurora shader 模块编译链接通过

### Requirement: 静态三方库在 Linux 使用 PIC

被链接进共享模块（插件/引擎 DLL）的静态三方库 SHALL 以 `CMAKE_POSITION_INDEPENDENT_CODE=ON` 构建（Linux），至少包括 jolt、recast、enet。

Jolt（上游替换 Bullet）SHALL 在 Linux 同时启用 `CPP_RTTI_ENABLED=ON`：插件子类 `PairFilter : JPH::GroupFilter` 需要基类 typeinfo，否则模块 dlopen 时报告 `undefined symbol: typeinfo for JPH::GroupFilter`。

#### Scenario: 共享插件链接/加载成功
- **WHEN** 在 Linux 构建并加载 `JoltPhysicsModule` / `RecastNavigation` / `EnetNetwork`
- **THEN** 链接与 dlopen 成功，无 `recompile with -fPIC` 或 undefined typeinfo

### Requirement: 三方 Find 模块的大小写

`FindSlang.cmake` SHALL 改名为 `Findslang.cmake`（`find_package(slang)` 在大小写敏感文件系统上的模块名）；`Findsfmt.cmake` SHALL 使用上游产物名 `SFMT`。

#### Scenario: Linux configure 找到 slang/sfmt
- **WHEN** 在 Linux 配置引擎
- **THEN** `3rdParty::slang` 与 `3rdParty::sfmt` 正确解析，无 “not found”

### Requirement: 宿主机前置依赖可文档化复现

Linux 引导的前置条件 SHALL 在仓库文档中列明：Python 依赖（经 `venv`）、ninja、X11/xcb 头与库、`-D3RD_PATH` / `third_party_cache_Linux.cmake` 工作流。

#### Scenario: 干净 Ubuntu 24.04 按文档可完成引导
- **WHEN** 按文档安装前置依赖后运行引导并配置引擎
- **THEN** Linux 引擎目标可 configure/build
