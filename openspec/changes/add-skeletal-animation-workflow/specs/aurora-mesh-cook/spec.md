## MODIFIED Requirements

### Requirement: mesh 源导入
Cook SHALL 通过现有 Assimp importer 读取受支持的 gltf/glb/fbx/obj 数据，保留原始节点身份、每 mesh-binding 的 offset matrices、rig 关联和 influence。蒙皮导入 SHALL 在限制权重或优化节点/mesh 之前验证原始输入，SHALL NOT 用 LimitBoneWeights 静默截断或以全局骨名表吞掉不同绑定。转换空间与共享判定 SHALL 遵守 aurora-cook；不能可靠恢复的绑定关联 SHALL 返回 Unsupported。静态三角化、法线/UV/切线生成仍使用现有管线。

#### Scenario: OBJ quad 导入
- **WHEN** 导入有效的带 pos/uv/normal 的 OBJ quad
- **THEN** SHALL 产出三角形 primitive、有效属性与材质槽

#### Scenario: 无效源拒绝
- **WHEN** 导入无法解析的字节或无法区分的骨架绑定
- **THEN** SHALL 返回具体失败，不发布错误或不完整产品

#### Scenario: 原始权重超限
- **WHEN** 原始顶点包含超过四个不同的正权重骨骼
- **THEN** SHALL 在任何裁剪前报错，不把 Assimp 已裁剪结果当作合法原始输入

### Requirement: 交错顶点流组装
Cook SHALL 按 POSITION/NORMAL/TANGENT/UV1/COLOR/JOINTS/WEIGHTS 的既有顺序与格式组装每个可合并几何组，JOINTS 使用 U_RGBA8 槽索引。位置 SHALL 烘焙 mesh 节点变换，法线 SHALL 使用 inverse-transpose，切线 SHALL 使用线性变换再相对变换后的法线正交化；镜像变换 SHALL 同步修正切线 handedness 与三角形绕序。index 类型 SHALL 按顶点数选择 U16/U32，几何 bounds 与 mesh-space 槽 bounds SHALL 从最终顶点流构造。

#### Scenario: 属性表与 stride
- **WHEN** 组装含 pos/normal/tangent/uv 的源
- **THEN** 属性 SHALL 紧凑排列且 stride 为 48，vertex/index 尺寸与计数一致

#### Scenario: tangent 可关
- **WHEN** 配置 tangents=false
- **THEN** 属性表 SHALL 不含 TANGENT

#### Scenario: 非均匀与镜像变换
- **WHEN** mesh 节点有可逆非均匀缩放或镜像
- **THEN** 烘焙后的法线/切线 SHALL 正交且绕序/handedness 与变换一致

### Requirement: Mesh 几何产品与独立 Skin 产品
新版 MeshAssetData SHALL 保存版本、属性/计数/stride/indexType、材质引用、几何 bulk、输入槽布局/模型空间契约与槽 bounds；SHALL 移除 v5 skin bulk 和 Skeleton 引用，不恢复旧 v2 skin UUID 或已删除 meshlet 字段。Skin SHALL 作为版本化 AuroraSkin 子产品，组件组合与稳定身份 SHALL 遵守 aurora-cook 和 asset-source-identity。旧产品 SHALL 返回 recook 诊断。

#### Scenario: 新几何产品往返
- **WHEN** 新版 Mesh 产品完成 Save/Load
- **THEN** 几何、材质与输入槽契约 SHALL 保持一致，SHALL 不包含选定 Skin/Skeleton

#### Scenario: 旧产品拒绝
- **WHEN** 加载旧 v2 或 v5 Mesh 产品
- **THEN** SHALL 返回版本/recook 诊断，不按新布局解析

#### Scenario: 蒙皮产品组合
- **WHEN** cook 有效蒙皮模型且 skinning=true
- **THEN** SHALL 发布独立 Mesh/Skin/Skeleton 产品，默认组合存入导入元数据而非 Mesh 依赖表
