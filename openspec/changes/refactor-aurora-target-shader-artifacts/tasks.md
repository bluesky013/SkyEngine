状态：2026-10-23 基线为 38/43；2026-10-25 追加审计修复 6/7 完成，本轮 Metal 原生验收（6.7）仍待 Apple 宿主。原跨平台与材质端到端 gate 保持未勾选；构建/离线验证不替代实机结果。详情见 [implementation.md](implementation.md)。未 commit/archive。

## 1. M1 产物身份与严格校验

- [x] 1.1 为现有五类问题固定回归入口和支持矩阵：缺失 native mapping、零工具链指纹、只读 buffer class、辅助 SPIR-V 身份桥、CPU/目标 ABI 耦合；记录可复现事实与仅属契约缺口的项目
- [x] 1.2 在当前固定 Slang SDK 验证声明身份通道（user attribute 或同源清单的严格完整声明路径）；覆盖 owner/命名作用域、字段关联、资源类型、VS/FS/CS、优化/宏、三目标及 MAT4/FLOAT4 数组布局信息，选定单一正式通道并回填证据
- [x] 1.3 在现有接口中定义逻辑接口、CPU schema、TargetEnvironment、ProducerIdentity、RuntimeAbiVersion 和有版本的目标 ABI 记录，保持 target/目录/装载入口不变
- [x] 1.4 将 CompileResult/缓存 blob 收敛为不可变 stage artifact：显式 code format、精确字节数、entry/stage、内容摘要、接口与目标反射，失败不得发布部分结果
- [x] 1.5 升级现有 index/blob codec 与加载验证，CompileKey/发布请求映射到绑定全部规范化内容的 ArtifactID；覆盖旧格式、损坏、超限、错误 target/stage/entry/variant/interface、同 MSL 的 macOS/iOS 互换、有效长度损坏及代码/反射错配；合法零资源与缺失反射分开
- [x] 1.6 用实际编译器/adapter/反射规则和规范化目标选项生成生产指纹，替换 cook/runtime 的零占位；验证输入变化失效及机器路径变化不影响 key
- [x] 1.7 分离开发 CompileKey 与 shipping index 解析/ABI 兼容检查，验证无编译器或不同已安装编译器均可消费兼容产品

## 2. M2 同源逻辑身份与单目标反射

- [x] 2.1 从权威 `.h` 派生接口身份、逻辑 ResourceKey、字段身份与独立 CPU schema；更新持久 `.slangh` 导出及新鲜度检查，不增加 C++ 到 Slang 的构建依赖
- [x] 2.2 Slang adapter 接入 M1 选定通道，严格校验声明关联、资源形状、重复键和缺失身份，删除短名字 first-match 关联
- [x] 2.3 改为从所请求目标的 global/entry scope variable layout 遍历、累积原生地址，保留活跃 stage、类别、访问方式与 block ABI；不支持容器/形态明确拒绝
- [x] 2.4 为 block/member 补齐 size/alignment/offset、array count/stride、规范化 matrix major/stride 与必要存储类型，完成 codec 往返和边界验证
- [x] 2.5 检查 metadata/地址查询结果，要求活跃集合每项映射完整；删除 Metal 空 metadata 连续编号、缺失 stage 跳过和 count=0 降为 1 的路径，合法 producer/手写 fixtures 同批迁移
- [x] 2.6 删除 MSL 的辅助 SPIR-V target/layout，验证每次只读取请求目标；覆盖跨 set 同 binding、同名局部声明、稀疏 native index、storage image mixed category 与宏优化资源
- [x] 2.7 从同源声明构建 interface/set 作用域的逻辑 Name/字段表并保留在 shipping artifact；验证规范名/显式 alias、Name handle 非持久身份、inactive/unknown 区分与带 generation 句柄失效

## 3. M3 Shader 链接与原生 BindingPlan

- [x] 3.1 ShaderFunction 保留 artifact 身份/所有权，Shader 从匹配 stage artifacts 链接；拒绝无关 code/reflection、错误 stage/entry/target environment 与资源语义冲突
- [x] 3.2 在现有 Shader/ResourceGroup 构建并复用不可变 BindingPlan，保存逐 stage 原生地址；验证容量、类别内冲突、孤立记录与 vertex buffer 槽位边界
- [x] 3.3 Metal graphics/compute encoder 消费明确 DirectArguments 计划；保留分 stage 动态 offset 和稀疏槽位，拒绝未实现的 descriptor array/argument-buffer model
- [x] 3.4 Vulkan layout/descriptor 继续由 Shader 同源派生，迁移为受检计划；保留现有 update-template/显式更新和已支持数组路径
- [x] 3.5 贯通 structured/raw buffer READ/READ_WRITE、raw/structured 形态和 element stride：Slang 语义、codec/merge、DX12 root/table 类别、descriptor 创建/编码及 RDG 访问校验；未支持 typed/counter 形态明确失败
- [x] 3.6 迁移 DescriptorEncoder 的本次受影响错误路径，类型/owner/范围冲突明确返回并保持先前有效绑定，不用断言、UB 或静默忽略
- [x] 3.7 增加原生 binding/访问回归：VS buffer(2)/FS buffer(5)、两 draw 不同 dynamic offset、只读输入加 RW 输出、冲突/缺失拒绝以及同 logical UBO 跨 stage 不兼容布局拒绝
- [x] 3.8 将 ResourceGroup/既有 DescriptorEncoder 收敛为 Name-first 事务，提供 typed WriteData/WriteBlock/WriteImage/WriteSampler/WriteBuffer；显式注入 RHI 中性上传上下文，禁止资产/图反向依赖、后端名字推导与 End 隐式 GPU 提交

## 4. M4 UniformUploadPlan 与生产调用迁移

- [x] 4.1 实现 CPU schema/语义接口/目标 ABI 的计划构建与缓存，key 包含链接布局或 artifact 身份、CPU fingerprint 和 packing 协议；每 draw 不重复扫描反射
- [x] 4.2 编码本期 scalar/vector/MAT4/FLOAT4 数组，覆盖目标 offset、数组 stride、矩阵 major/stride、清零 padding、源范围/目标范围/重叠与整数溢出校验
- [x] 4.3 实现逐字段兼容且复制范围无 padding 的整块复制快路；验证与通用路径目标字节相等，有 padding 时分段复制并清零，拒绝仅总大小相等或源 padding 已初始化就整块复制
- [x] 4.4 迁移 typed Pass 写入与 PassResourceBindings：先保留 CPU 值/选择实际计划，再分配目标大小、编码及原子发布；区分逻辑 range 与 native view 对齐 backing，移除原始 as_bytes<T> 上传后要求 CPU=GPU 的假设
- [x] 4.5 将 PackShaderUniform/材质 property 映射与显式 Batch assembler 接入同一计划规则，保留资源值提供方、可选资源与不活跃资源的现有语义
- [x] 4.6 CPU 验证合成不同布局、非对称矩阵、数组 stride、padding/哨兵、全部字段负例及失败不改变 draw；使用实际三目标反射验证支持集合，不强求各目标恰好不同
- [x] 4.7 验证两个 View、多 draw、连续在途帧及新旧显式 shader/计划版本并存；计划、artifact、字节和模块引用沿现有 lease 退休
- [x] 4.8 迁移 Material/MaterialInstance 的语义类型校验、Name alias 与 typed 参数快照，以及 SceneAssetSource/RenderMaterialTechnique/PreparedDraw 的数字 binding/目标预打包路径；材质与 Feature 使用同一命名事务且保持显式 provider 归属
- [x] 4.9 打通普通 2D 材质贴图：资产引用/typed sampler -> 精确图片版本及预算/发布依赖 -> Resident/InGraph 资格 -> 图 view/读访问 -> 命名 RHI 绑定；补齐 sRGB 颜色/线性数据语义，不复用环境辐射专用限制
- [x] 4.10 验证命名事务的 unknown/ambiguous/inactive、同大小异类型、错误资源形状/owner、缺字段、重复 alias/字段与 block 重叠、取消和失败原子性；部分更新/实例覆盖/贴图替换保留旧在途快照

## 5. M5 产品迁移、跨平台验收与收口

- [x] 5.1 正常项目 cook/package 传递完整目标环境和 producer identity，按请求发布新格式产物；重新 cook 既有 scene/material/fullscreen/compute 与多 owner 产品
- [x] 5.2 验证 relocated source-free 产品：移除作者源码、运行时 Slang/DXC，覆盖 scene/compute/fullscreen；旧格式/错误设备能力/缺少产品明确失败而不回退源码
- [x] 5.3 macOS 原生 Metal：scene/compute/fullscreen、stage 稀疏地址、动态 offset、目标 uniform 编码和产品读回；必须以 native pipeline reflection 核对可观测 stage/category/index、活跃参数和 block ABI，并验证保守集合不遗漏实际活跃资源
- [ ] 5.4 iOS arm64：交叉构建与独立目标环境产物校验；真机完成 native binding、uniform/compute 与无编译器产品消费，分开记录构建和实机结果
- [ ] 5.5 Android arm64-v8a：交叉构建；Vulkan 真机只读/RW buffer 与参数读回，验证 APK 部署产品且无作者源码/运行时编译器依赖
- [ ] 5.6 Windows：DXIL/SPIR-V 实编译、D3D12 debug layer 下只读 SRV/RW UAV 与 graphics/compute 原生读回、Vulkan 两条既有 raster 路径及无编译器产品回归
- [x] 5.7 清理被替代的辅助 target、短名关联、顺序槽位和固定 CPU=GPU 兼容分支；更新 shader-system、相关技能/设计说明及已有 change 的接续记录，不重写历史验收
- [x] 5.8 运行受影响构建/测试、源码规则与 diff 检查及 OpenSpec strict；回填每项证据、设备/工具链和未完成项，未执行的实机验证保持未勾选
- [ ] 5.9 Linux/Vulkan 消费方：基于已有 preset 核查公共 shader/Vulkan 构建及产品加载；在可用 Vulkan 环境执行 offscreen compute/参数读回，缺失的现有宿主基础记录为前置且保持未验收，不混入额外平台移植
- [ ] 5.10 以真实材质资产及实例在三后端通过同一 Name 入口同时更新常量、纹理与 sampler，检查实际采样读回、异步未就绪/缺失错误、device epoch 和两帧旧纹理保活；shipping 剥离源码/Slang/debug 信息后仍按 Name 工作
- [x] 5.11 迁移材质命名映射/资源与 sampler 的资产 schema，收集 cook/package 图片依赖并 recook；覆盖不同 technique 的显式 alias、sRGB 与线性输入以及保留的非本期资源 Unsupported 边界

实施结束后的 commit/archive 仍需按 AGENTS.md 分别展示拟操作内容并取得用户明确同意；这两项不作为当前已授权实施任务。

## Proposal validation

本节只记录方案文件自身的校验，不代表以上实现或原生验收已完成。

2026-10-22：proposal、design、两份 capability delta 与 tasks 已齐；35 项实施任务均未开始。`openspec validate refactor-aurora-target-shader-artifacts --strict`、接续基线 `refactor-aurora-pipeline-resource-ownership` 的 strict 校验及 `git diff --check` 通过；新文件额外检查无行尾空格。引用的 active changes 均存在。方案经声明/CPU ABI、Metal native mapping、artifact/shipping 三个方向交叉复核，未运行或宣称任何本次重构的构建/GPU 验收。

2026-10-23：跨平台影响复核补入架构图、平台迁移表及已有 Linux preset 对应 Vulkan 消费方的验收项，现共 36 项待实施任务；不将已有 Linux 宿主基础是否完整视为已验证事实。本次仅更新方案说明与验收范围。

2026-10-23（Name 参数入口补充）：按用户要求新增 Name-first ResourceGroup 事务、shipping 逻辑名字表、Material/MaterialInstance 受检入口和普通 2D 材质贴图依赖闭环，新增 aurora-material delta，现共 43 项待实施任务且全部未开始。三份 capability delta 的 OpenSpec strict、diff 及新增文档空白检查通过；本次仍仅修改方案，没有实现命名 API 或运行原生材质采样验收。

## Implementation checkpoint — 2026-10-23

- M1–M4 的 32 项实现与当前支持集合回归已完成；ResourceGroup 和生产图桥直接委托同一 `ShaderParameterTransaction`，原生/图 materializer 保留各自的资源与 lease 边界。
- M5.1/5.2/5.3/5.7/5.8/5.11 已完成当前宿主验收；cache v6、usage/distribution v2、Material v3、MaterialInstance v2 均需按新格式重新 cook，不保留旧产品静默兼容。
- 最终原生/CPU：RHI 184/184、Core 91/91、Shader 51/51、Pipeline 40/40、Adaptor 44/44、Cook 44/44；Module 10/10、StaticModule 11/11。Core 新增目标相关 downstream producer 失效回归；UI SPIR-V/MSL 6/6、DXIL 3项非Windows跳过。材质产品冷加载新增加强后，动态/静态各 2/2 通过；重新 cook 后剥离宿主的 Core 1/1 与 Metal scene/compute/fullscreen/material 3/3 通过。Python 61/61、ExportPreset C++ 6/6 通过。编辑器构建及8帧运行、UI Metal/MoltenVK初始化/关闭和材质/实例sampler通用面板编辑/Undo/Redo/保存探针通过；Windows fixture源SPIR-V/MSL各17个stage通过，不能代替Windows原生执行。
- 独立 native graphics reflection 对照：两宏 × O0/O3，四个实际 Metal VS/FS pipeline 均通过，逐 stage 活跃 category/index 与 Slang metadata 相同；block size 192、字段 offset 0/8/16/80/144、矩阵及 FLOAT4 数组 stride 16 均匹配。结构体 alignment 16 与 Metal buffer binding alignment 32 分开处理。
- Android arm64-v8a/API31：最终112+3步；iOS arm64/SDK18.5/deployment13：最终105+3步；包含UIRender的指定目标均成功且无 API availability 警告。离线分别 cook iOS/Metal 与 Android/Vulkan 产品成功，正确平台 staging 接受、错误平台拒绝。MSL 2.3 的原生运行下限仍为 macOS 11 / iOS 14。
- 当前 adb 设备列表为空，xcdevice 只列出本机 Mac。Windows/D3D12、Windows/Vulkan、Linux/Vulkan 和移动实机尚未执行，五项原生验收保持未勾选。Linux preset 已存在，但 Core/Framework 的当前平台源选择没有 Linux 分支；宿主/异步原语基础为既有前置，不在本 change 扩大移植。
- `git diff --check`、新增/修改错误路径禁用 throw/try/catch/dynamic_cast 检查及相关 OpenSpec strict 校验完成；具体复现方式、日志和限制见 implementation.md。
- Origin复验：已rebase至`b5247705`并保留全部原未提交改动，修复冲突及Develop/Clang/日志格式兼容。Release1494项、Develop632项、Python67项通过；shipping重新cook后1+3项、编辑器8帧运行及移动交叉构建通过。UIShader的3项DXIL在非Windows跳过；原五项跨平台原生验收仍待对应环境，进度保持38/43。详见implementation.md的rebase复验记录。


## 6. 2026-10-25 审计修复

- [x] 6.1 恢复全部 RendererEvent 交付；补完整 set 2 Material、set 3 SceneSurface/UI/legacy Batch 和生产 storage 事务的失败原子性。
- [x] 6.2 发布 v9 三域选择元数据；使用真实 Vertex 属性/instance 输入和逐 stage schema，保留 specialization 有效值；补无源非默认组合回归。
- [x] 6.3 对所有执行 variants 校验接口与唯一选择键，并计入 artifact/反射/选择数据预算。
- [x] 6.4 异步 compiler/module/root/writer 显式保活及单次执行；补真实 DLL 最终释放回归并消除控制块自卸载。
- [x] 6.5 Metal compute 四组与 raster storage 实现接通；同步当前文档、规则、持久导出和过期测试夹具。
- [x] 6.6 完成 Windows 最终构建/原生回归、发布包和规范检查，回填最终结果。
- [ ] 6.7 在 Apple 宿主验证本轮四组 compute 与 raster palette 修改；本机 MSL 生成不替代该项。

6.x 为本轮追加项；不改写 2026-10-23 的 38/43 历史进度，也不自动关闭原跨平台/材质端到端 gate。当前结论见 implementation.md 与 docs/aurora/rendering-design-audit-2026-10-25.md。


## 2026-10-26 规范接续复核

已逐项修正相关主 spec 与 active delta 的现行契约，并将已存在的 ADDED/RENAMED/REMOVED 操作重设为可再次合并的 delta。只读合并检查与 strict 校验分开记录；未执行 archive。历史平台结果不覆盖后续四组/typed selection/metadata owner 修改。完整范围、证据和剩余项见 `docs/aurora/consistency-review-2026-10-26.md`。
