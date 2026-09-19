## Why

`python/third_party.py` 的浅克隆路径存在版本钉住漏洞：只有**首次克隆**时用 `branch=tag` 浅克隆目标 tag；目录已存在时，仅带 submodule 的包（`need_full_history`）会 fetch+checkout 标签，普通包永远停留在旧 checkout 上。

实际事故：`thirdparty.json` 把 astc 从 5.2.0 升到 5.7.0，但 intermediate 里的 checkout 停在 5.2.0。astcenc 5.7.0 给 `astcenc_context_alloc` 增加了第 4 个参数（`parent_context`），引擎按 5.7.0 编写的调用在 5.2.0 头文件下编译失败，一度被误改为 3 参调用（削足适履）。

## What Changes

- **修机制**:`third_party.py` 补 `elif tag:` 分支——已有 checkout 且配置 tag 时，`fetch --depth=1 origin refs/tags/<tag>` 后 `checkout --detach FETCH_HEAD`（与 commit 路径一致），保证 checkout 与 `thirdparty.json` 一致。
- **astc.patch 基于 5.7.0 重新生成**：只保留两条 install 规则（头文件 + 静态库）；此前为 5.2.0 加的 `-mcpu=native` AppleClang x86_64 修复 hunk 删除（5.7.0 上游已移除该代码）。
- **还原** `ImageCompressor.cpp` 的 3 参误改（恢复 5.7.0 的 4 参调用）。
- **归档 md5** 同步（`thirdparty.json`）。

## Capabilities

### New Capabilities

- `third-party-version-pinning`: 三方包 checkout 与配置版本的一致性保证。

### Modified Capabilities

（无。）

## Impact

- **修改文件**:`python/third_party.py`（+6 行）、`cmake/patches/astc.patch`（重生成）、`cmake/thirdparty.json`（md5）、`engine/aurora/cook/image/src/ImageCompressor.cpp`（还原）。
- **影响面**：所有无 submodule 的三方包在 tag 变更后会被正确切换；已有 checkout 每次构建多一次 `fetch --depth=1`（开销可忽略）。
- **验证**：astc 5.7.0 重建成功，`AuroraCookTest` 28/28 通过。
