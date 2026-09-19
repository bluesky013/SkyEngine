## Context

`third_party.py::process_package` 的版本控制逻辑：

```python
if not os.path.exists(clone_dir):
    # 浅克隆:depth=1 + branch=tag —— tag 生效
    repo = Repo.clone_from(url, clone_dir, depth=1, branch=tag)
else:
    repo = Repo(clone_dir)

if commit:
    fetch --depth=1 origin <commit>; checkout --detach FETCH_HEAD
elif tag and need_full_history:   # submodule 包才走这里
    fetch --tags; checkout Branch_<tag>
# elif tag: 缺失 —— 普通包已有 checkout 时不做任何切换
```

astc-encoder 的版本演进放大了这个洞：5.2.0 的 `astcenc_context_alloc` 是 3 参，5.7.0 改为 4 参（新增 `parent_context` 用于上下文间共享线程池）。`thirdparty.json` 声明 5.7.0、引擎代码按 5.7.0 写，实际编译的却是 5.2.0 的 checkout。

## Goals / Non-Goals

**Goals:**

- 任何已有 checkout 在下次构建时 SHALL 与 `thirdparty.json` 的 tag/commit 一致。
- astc 回到 5.7.0，引擎代码保持 4 参调用。

**Non-Goals:**

- 不改 `need_full_history`（submodule）路径的行为。
- 不做版本一致性自检命令（如 `third_party.py --verify`）。

## Decisions

### D1: shallow 路径镜像 commit 路径的 FETCH_HEAD 模式

新增 `elif tag:` 分支：`fetch --depth=1 origin refs/tags/<tag>` + `checkout --detach FETCH_HEAD`。

- 与 commit 分支同一模式，不引入分支管理（`Branch_<tag>` 仅 full-history 需要，因为 submodule 递归更新要分支上下文）。
- 显式 `refs/tags/<tag>` refspec 避免依赖本地 tag 存在性。
- 每次构建多一次 depth=1 fetch，开销可忽略；换来幂等。
- 首次克隆后也会走该分支（重复 fetch 同一 tag，无害）。

### D2: astc.patch 跟随 5.7.0 重新生成

5.7.0 上游重写了 `cmake_core.cmake` 的 native ISA 段，`-mcpu=native` 已不存在，旧的 AppleClang x86_64 修复 hunk 删除。patch 只保留引擎需要的两条 install 规则（`astcenc.h` 头文件、`astcenc-native-static` 静态库）。

### D3: 引擎代码不改

`ImageCompressor.cpp` 的 4 参调用与 5.7.0 头文件一致；此前的 3 参改动是向下适配陈旧 checkout，还原。
