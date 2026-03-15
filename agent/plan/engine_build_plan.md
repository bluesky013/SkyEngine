# SkyEngine 构建计划（4 步）

本计划按仓库当前结构制定，重点约束：**三方库构建入口统一使用 `python/third_party.py`**。

## Step1: 三方库构建（必须先执行）

- 入口脚本：`python/third_party.py`
- 目的：生成 `3RD_PATH` 目录（后续 CMake 配置依赖此路径）
- 推荐命令（Win32）：

```bash
python python/third_party.py \
  -i <thirdparty_intermediate_dir> \
  -o <thirdparty_output_dir> \
  -e <engine_root> \
  -p Win32 \
  -j 8
```

## Step2: 构建 solution（CMake Configure/Generate）

- 说明：本仓库通常先用 CMake 生成工程（Visual Studio 生成器下会生成 `.sln`）
- 推荐命令：

```bash
cmake -S <engine_root> -B <build_dir> \
  -G "Visual Studio 17 2022" \
  -D3RD_PATH=<thirdparty_output_dir> \
  -DSKY_BUILD_TEST=OFF
```

> 若需要在本次流程执行测试，将 `-DSKY_BUILD_TEST=ON`。

## Step3: 构建引擎

- 推荐命令：

```bash
cmake --build <build_dir> --config Release --parallel 8
```

- 产物默认输出到仓库 `output/bin`（由根 CMake 配置控制）。

## Step4: 如有需要运行 test

- 前提：Step2 需开启 `SKY_BUILD_TEST=ON`，并已完成 Step3。
- 运行方式：

```bash
ctest --test-dir <build_dir> -C Release --output-on-failure
```

---

## 一键执行脚本

已配套脚本：`agent/scripts/build_engine.py`

示例：

```bash
python agent/scripts/build_engine.py --platform Win32 --config Release --jobs 8
```

启用测试：

```bash
python agent/scripts/build_engine.py --platform Win32 --config Release --jobs 8 --enable-tests --run-tests
```
