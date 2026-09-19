## ADDED Requirements

### Requirement: 三方包 checkout 与配置版本一致

`third_party.py` SHALL 保证每次构建前，包的 checkout 与 `thirdparty.json` 配置的 `tag`/`commit` 一致——无论 checkout 是本次新克隆还是已存在。对已有浅克隆且配置 `tag` 的包，构建脚本 SHALL `fetch --depth=1 origin refs/tags/<tag>` 并 `checkout --detach FETCH_HEAD`。

#### Scenario: 已有 checkout 的 tag 升级

- **WHEN** `thirdparty.json` 中某包 tag 从 A 升到 B，且 intermediate 已存在 tag A 的浅克隆
- **THEN** 下次构建该包时 checkout 切换到 B（`git describe --tags` 为 B）

#### Scenario: 已有 checkout 的 tag 不变

- **WHEN** checkout 已在配置 tag 上
- **THEN** 构建正常进行（FETCH_HEAD 检出同一提交，幂等）

#### Scenario: submodule 包路径不受影响

- **WHEN** 包配置了 `submodule`/`submodules`（`need_full_history`）
- **THEN** 仍走 `fetch --tags` + `Branch_<tag>` 分支路径

### Requirement: 引擎代码与钉住版本 API 一致

引擎中对三方库的调用 SHALL 与 `thirdparty.json` 钉住的版本签名一致；版本不一致导致的编译错误 SHALL 通过修正 checkout 解决，SHALL NOT 通过改引擎代码向下适配陈旧版本。

#### Scenario: astcenc 5.7.0 调用

- **WHEN** `thirdparty.json` 钉住 astc `5.7.0`
- **THEN** `ImageCompressor.cpp` 以 4 参签名（含 `parent_context`）调用 `astcenc_context_alloc`,`AuroraCookTest` 全部通过
