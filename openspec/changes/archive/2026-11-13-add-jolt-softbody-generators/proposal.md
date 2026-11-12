> 2026-11-13：按用户明确指令归档当前交付。任务完成 80/83；5.4（形变法线视觉验收）、7.5（三端完整应用及持续性能验收）、8.5（优化海豹交互 Generate/Save/reopen）仍未完成，保留原始未勾选状态。归档与规范同步不代表这些验收已经通过。

# Portable CPU XPBD soft bodies and mesh generators

## Why

用户要求制作精细小海豹，在现有 Jolt 上封装 XPBD 软体能力，并在 Mesh 编辑器中支持可注册 generator：保留指定控制点及其约束，自动生成形变骨骼和每顶点最多四个骨骼影响的绑定。2026-11-01 已明确确认：四个指每顶点影响数，控制点总数可以更多；示例动物为小海豹。

当前 Physics/Jolt 没有软体接口，Mesh 视图是只读统计视图，编辑器没有 generator 注册契约。已有四权重蒙皮、文档撤销、反射表单、action registry 和生成子资产身份可以复用。

## What Changes

- 在 SoftBody 插件内实现独立 CPU XPBD，消费 `engine/physics/softbody` 的碰撞接口；Jolt 提供完整固定步的接触数据，未来 PhysX 可实现同一接口。
- 新增 SoftBody 实现插件，分离纯绑定算法、资产、World 适配、Builder 和 Editor；接口和接口引用数据保留在消费领域的 engine 模块。
- 增加通用 AssetGeneratorRegistry；Mesh document 消费注册描述和反射设置，插件提供生成器，不硬编码软体算法。
- 生成配方保留原 mesh source、作者控制点稳定身份、固定/随根驱动约束、预算、算法版本和参数；一个 source owner 生成一致的 Mesh/Skin/Skeleton/SoftBody 产品集合。
- 自动生成合法四面体控制笼，使用包含显示顶点的四面体重心坐标生成最多四个影响；不使用全局最近四点、不静默裁权重、不偷偷替换指定控制点。
- 制作原创精细小海豹，先检查未绑定主体拓扑，再通过生产 generator 建立绑定和 CPU XPBD 演示。

## Capabilities

### New Capabilities

None. 本 change 扩展已有 canonical domains，不为单一 solver 新建重复的领域规范。

### Modified Capabilities

- `physics`: CPU softbody 生命周期、控制点约束、固定步碰撞数据、快照和能力声明。
- `editor-assets`: 可注册 generator、可编辑 Mesh document、控制点 authoring、撤销和版本守卫。
- `asset-cook`: 确定性四面体绑定、单 source 多产品、现有 mesh importer/cook 服务复用。
- `animation`: 软体姿态修改与普通动画的确定合成；不覆盖既有全局 pose source。
- `aurora-resources`: 四权重绑定、形变法线和动态 bounds 的一致资源契约。

## Impact

复用 `Physics`、`JoltPhysics.Adaptor.Static`/`JoltPhysicsModule`、`EditorCore`、`Aurora.Render.Editor`、`Animation.World` 和 Aurora 既有渲染/烘焙宿主。拟新增 target/装载入口见 design D1；文件保持原目录，不做无关搬迁。

## Status

用户已确认 D1 的接口归属为 `engine/physics/softbody`；generator、资产与编辑器链路已进入验证阶段，不能视为整体验收完成。

2026-11-02 用户明确 Android、iOS，并补充“oho”（按 OHOS 理解），同时要求评估未来 PhysX 后端。用户随后以“按照你的建议”确认独立 CPU XPBD + 后端碰撞适配方案。原生 Jolt 试验实现及该试验新增的根跟随补丁已撤回，当前为独立 CPU XPBD。原生 PhysX CUDA 体积软体不能作为三个移动平台的共同基础。具体边界见 design D2。

用户进一步要求 Release 真机测试及精确 30/25/20/15/10 点 rig，先双软体／单软体对静态和运动学障碍，再测十只同屏。D3 在既有 target 中增加事务式双向软软接触、World 协调和可选固定步统计；动态刚体双向反作用仍不支持。Android 完整 Release 已安装，80 项主矩阵及 20 项独立低冲击补测已完成并保留失败记录、几何误差和原始数据。15/20 点接地稳定性未通过；这不等于 iOS/OHOS 或十只持续 60 FPS 验收。

上述状态属于实施阶段记录；2026-11-13 用户已授权将实现与同步后的规范一起提交、rebase 并 push。
