## 1. 宏定义

- [x] 1.1 core 侧 config 头定义 `SKY_DEVELOP`（`#ifndef` 兜底 `0`）——`engine/core/include/core/platform/BuildConfig.h`
- [x] 1.2 确认代码统一用 `#if SKY_DEVELOP` 判读

## 2. CMake 接入

- [x] 2.1 `cmake/options.cmake` 加 `SKY_DEVELOP` 选项（editor/tool 构建默认开），`cmake/configuration.cmake` 开启时 `add_compile_definitions(SKY_DEVELOP=1)`

## 3. 迁移既有开关

- [x] 3.1 `aurora-resource-name` 的 `SKY_ENABLE_RESOURCE_NAME` 直接判 `SKY_DEVELOP`（`aurora-resource-name` 尚未实现，实现时直接 gate 于 `SKY_DEVELOP`，无需 `_DEBUG` 过渡）
- [x] 3.2 文档明确 `_DEBUG`（编译配置）与 `SKY_DEVELOP`（构建意图）分工——`BuildConfig.h` 顶部注释

## 4. 测试

- [ ] 4.1 验证 Release+editor 构建下 `SKY_DEVELOP == 1`、`SKY_ENABLE_RESOURCE_NAME == 1`
- [ ] 4.2 验证纯 Release 构建下 `SKY_DEVELOP == 0`、`SKY_ENABLE_RESOURCE_NAME == 0`

## 5. 构建与风格校验

- [ ] 5.1 确认 `SKY_DEVELOP` 在开发/发布两意图下正确生效
- [ ] 5.2 运行 clang-format / clang-tidy 校验新文件符合仓库规范
