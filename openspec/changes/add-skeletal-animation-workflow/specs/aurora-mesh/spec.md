## MODIFIED Requirements

### Requirement: 蒙皮输入槽与独立实例绑定
VertexSemantic SHALL 保留 JOINTS / WEIGHTS 及名称解析。JOINTS SHALL 表示四个 Skin 输入槽索引，WEIGHTS SHALL 表示对应权重，数据留在 RenderGeometry 顶点流。Mesh SHALL 不定义或引用 Aurora Skeleton，不持有选定 Skin，不暴露 SetSkeleton/GetSkeleton/HasSkin 或 SetSkin/GetSkin。Skin 与实例绑定、palette 和 GPU 执行 SHALL 遵守 aurora-skinning capability；本类型不直接管理 GPU 蒙皮任务。

#### Scenario: 蒙皮顶点语义
- **WHEN** 读取含 JOINTS / WEIGHTS 的顶点布局
- **THEN** 语义名称 SHALL 能解析，JOINTS SHALL 按绑定槽解释而非直接索引 Skeleton

#### Scenario: 共享 mesh 的实例绑定隔离
- **WHEN** 两个实例对同一 Mesh 选择不同 Skin
- **THEN** 几何资源 SHALL 保持不变，所选绑定 SHALL 由各实例保存
