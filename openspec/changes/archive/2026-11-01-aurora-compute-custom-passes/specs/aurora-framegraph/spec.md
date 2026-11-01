## ADDED Requirements

### Requirement: Compute/Custom pass 的声明访问

`FrameComputePass` 和 `FrameCustomPass` SHALL 通过显式 buffer/image/sampler 声明表达其资源使用。每个 buffer/image 声明 SHALL 携带真实 set/binding、资源 handle、range 或 mip/layer view，以及独立的资源类型和 READ、WRITE、READ_WRITE 之一；sampler 声明 SHALL 携带 set/binding 和 sampler。声明顺序 SHALL 在 Compile 与 Custom 回调资源数组之间保持一致；原生 descriptor SHALL 通过 set/binding 对应，dynamic offsets SHALL 按同一 set 的 binding 升序传递。

Compute SHALL 按 shader reflection 校验声明：sampler 只匹配 SAMPLER，uniform buffer 只能 READ，storage buffer 可为 READ/WRITE/READ_WRITE，sampled image 只能 READ，storage image 可为 READ/WRITE/READ_WRITE；缺失 shader、重复 set/binding、声明了 shader 未使用的资源或 access 与资源类型冲突 SHALL 失败。Compute 可默认 REFLECTED，显式 kind SHALL 匹配 reflection。Custom SHALL 显式选择 buffer 的 UNIFORM/STORAGE 或 image 的 SAMPLED/STORAGE，UNIFORM/SAMPLED SHALL 只允许 READ。Custom SHALL 以声明为唯一契约，encoder 为空、声明为空且 `sideEffect=false`、或 range/view/access 非法 SHALL 失败。

buffer 的有效访问范围 SHALL 包含 dynamicOffset 并在加法前验证溢出与上界；compute storage image view SHALL 归一化为恰好一个 mip、单 sample。声明的读取 SHALL 进入内容依赖和存活区间；WRITE SHALL 承诺完整覆盖声明范围；READ_WRITE 还 SHALL 要求已有内容，两者的 range/view SHALL 定义对应内容。STORAGE + READ SHALL 仅产生读依赖，不定义内容，也不因此改用 sampled/SRV 绑定。读取未定义内容 SHALL 在 Compile 被拒绝。Custom 回调 SHALL NOT 调用 `PipelineBarrier`，也不得访问声明之外的 graph 资源；回调返回前 SHALL 结束其创建的 encoders，不得保留 command/context 引用。

#### Scenario: Compute 声明驱动内容与依赖
- **WHEN** Compute pass 声明写 storage buffer 范围并读 sampled image view
- **THEN** 图保留 buffer producer、将写范围记为已定义、为 image 暴露的 mip/layer 建立读取依赖，并在 Compile 校验 handle/range/view 与 shader reflection

#### Scenario: Storage image 的写后采样
- **WHEN** Compute 以 READ_WRITE 声明 storage image，后续 raster 以 SRV 采样同一 image
- **THEN** image 在 compute 后进入可写状态、在 raster 前转为 shader read，写内容视为已定义；未初始化或被丢弃的子资源读取失败

#### Scenario: Custom 回调使用声明资源
- **WHEN** Custom pass 声明一个 storage buffer 和一个 sampled image，并提供非空 encoder
- **THEN** backend 在调用 encoder 前完成声明的 barrier，回调按声明顺序取得已解析的 `Buffer*`/`Image*` 和同一 command buffer；回调不插入 barrier，写入范围按声明定义内容

#### Scenario: 非法 Compute/Custom 声明
- **WHEN** Compute 对 sampled image 声明 WRITE，或 Custom 没有 encoder、声明为空且 `sideEffect=false`，或 range/view/access 越界
- **THEN** Compile 返回明确错误，不分配 transient、不执行回调，也不把图当作空图成功

### Requirement: Compute/Custom 的后端编码

每个后端 SHALL 在自己的 Compile 中为 Compute/Custom 生成 native 计划：按声明和 descriptor 类型把 buffer/image 从当前状态转到 uniform/shader read 或 unordered access，编码实际 dispatch 或 Custom 回调，并在后续 pass 前恢复所需状态。Compute/Custom SHALL 在 coordinator 串行编码；Custom SHALL NOT 声明并行。没有 native 能力或声明超出当前支持范围时 SHALL 返回 Unsupported，不得空分支伪成功。

Vulkan RenderPass 和 Dynamic Rendering 两套 compiler SHALL 各自生成对应计划；RenderPass 模式 SHALL 在 Compute/Custom 前后结束并重新建立 native scope，不得跨该 pass 合并。DX12 SHALL 在 DIRECT list 上完成 state transition 与 dispatch/自定义命令；同状态 UAV 访问 SHALL 保留必要的 UAV 依赖，uniform buffer SHALL 使用 constant-buffer 读取状态，dynamic-only set SHALL NOT 绑定不存在的 descriptor table。Vulkan storage 同步 SHALL 覆盖 Custom 允许的 graphics/compute 阶段及其前后依赖。Metal SHALL 使用 compute encoder 或当前命令缓冲，并以 encoder 边界或显式 memory barrier 满足跨 encoder hazard。

#### Scenario: Compute 写 buffer 后 copy 读回
- **WHEN** 图先执行写 storage buffer 的 Compute，再用 copy 读到 host-visible buffer
- **THEN** Vulkan 两种模式和 DX12 都产生 dispatch、正确 barrier 和 copy，读回内容与参考一致；Metal 在真实设备上按同一图得到相同内容

#### Scenario: Compute 与 raster 的内容顺序
- **WHEN** Compute 写 storage image 后由 raster 采样，或 raster 写入后由 Compute 读取
- **THEN** barrier 的 src/dst stage 与 access 反映实际生产/消费关系，读回结果不受 pass 声明顺序以外的偶然执行顺序影响

#### Scenario: Custom 串行编码
- **WHEN** 图包含 Custom pass，且其 encoder 使用 RHI 命令写入声明的 buffer
- **THEN** 该 pass 在 coordinator 路径执行，前后 barrier 由声明推导，worker 不并发调用同一 encoder；读回内容正确

#### Scenario: 动态偏移与声明顺序
- **WHEN** Compute 以非 binding 升序声明动态 UBO，且实际内容仅存在于 dynamicOffset 指向的区间
- **THEN** 内容分析与裁剪使用有效偏移保留 producer；Vulkan/Metal 按 binding 升序传递 offsets，DX12 绑定有效 root CBV，GPU 读回一致；越界与溢出声明失败

#### Scenario: compute storage view 不扩大内容定义
- **WHEN** Compute 为多 mip image 选择一个 storage mip，或用默认 view 暴露多个 mip
- **THEN** 单 mip 声明只定义对应内容，其他 mip 仍不可读取或完整 export；多 mip storage view 在分配前被拒绝

#### Scenario: Custom storage image 的 sample 与 mip 契约
- **WHEN** Custom 以 STORAGE 声明 image，不论 access 为 READ、WRITE 或 READ_WRITE
- **THEN** image SHALL 为 X1；X2/X4/X8 STORAGE image SHALL 在分析阶段拒绝，包括同时声明 RENDER_TARGET usage 的情况；X1 的合法访问范围允许跨多个 mip

#### Scenario: Compute 资源在 CPU 图释放后保活
- **WHEN** Compute 使用 shader/sampler，提交 Accepted 后 CPU 图和外部引用被释放
- **THEN** GPU bindings 保持 shader/sampler 到完成或安全 teardown，不能随 CPU 图提前销毁；完成后允许退休

#### Scenario: Custom 失败与设备丢失
- **WHEN** Custom 回调返回错误，命令结束操作同时失败
- **THEN** 后续回调不再执行，已接受的编码任务仍收齐，失败不提交；任一 DeviceLost 优先于可恢复错误

#### Scenario: 只读 storage 声明
- **WHEN** Custom 以 STORAGE + READ 读取 storage-only image 或 buffer
- **THEN** 不要求 sampled/uniform usage，保留生产者和 storage/UAV 依赖，不定义新内容；未初始化读取失败

#### Scenario: 同 pass 的不兼容资源类型
- **WHEN** 同一物理 buffer 同时用于 UNIFORM/STORAGE，或同 image 重叠 mip/layer 同时用于 SAMPLED/STORAGE
- **THEN** Compute/Custom 声明失败且不依赖 binding 顺序；相同类型重复绑定与非重叠 image views 允许

#### Scenario: 非零 dispatch 契约
- **WHEN** Compute 的 groupsX/Y/Z 任一为零
- **THEN** 返回 INVALID_ARGUMENT，不执行 dispatch、不定义 WRITE 内容；no-op 支持须另行定义内容、依赖和裁剪规则

#### Scenario: 无资源的副作用 pass
- **WHEN** Custom encoder 非空、sideEffect=true，且没有 buffer/image 声明
- **THEN** pass 仍被编码并调用一次；真正无工作图仍返回 NoWork，仅有资源边界转换的图仍执行必要转换

#### Scenario: Metal 原生校验
- **WHEN** Metal 编译 Compute 的 shader、buffer range/offset 或 image view
- **THEN** 验证 compute stage、实际资源 usage/形态、有效范围、引擎公布的 uniform 对齐和原生反射的 active buffer 对齐；组内线程各轴和总数分别遵循 device/pipeline 限制，不以其限制 dispatch 组数；非法参数返回 INVALID_ARGUMENT，必要元数据缺失返回 UNSUPPORTED

#### Scenario: Metal buffer memory 与反射缺失
- **WHEN** Metal Compute 使用合法 memory 枚举的 buffer，或 pipeline 未返回 buffer reflection
- **THEN** memory 枚举本身 SHALL NOT 排除 CPU_ONLY/CPU_TO_GPU/GPU_TO_CPU，仍须通过 usage/范围/对齐校验；整份反射缺失时带 buffer 的 Compute SHALL 返回 UNSUPPORTED，不得把未知元数据当作 binding 已优化移除；只有成功反射中的 inactive binding 可跳过类型对齐检查

#### Scenario: Custom 失败后安全复用
- **WHEN** Custom 创建 encoder 后返回错误，调用方收齐失败并 DiscardUnsubmitted
- **THEN** 后续 callback 不执行，统一完成命令收尾，同 slot 可再次正常编码/提交；Metal End 没有错误返回，不虚构 End 失败

#### Scenario: Custom 提交保活
- **WHEN** Custom 提交 Accepted 后 CPU 图和外部 callback 引用被释放
- **THEN** callback 及其拥有的 GPU 对象保留到完成或安全 teardown，随后允许释放

#### Scenario: 超出支持范围
- **WHEN** 图请求 async compute、跨队列同步、后端专属 custom 契约，或某后端缺少所需能力
- **THEN** 对应后端在 Compile 明确返回 Unsupported，不静默降级为空 pass 或忽略声明访问

## MODIFIED Requirements

### Requirement: Raster pass execute 由冻结 draw 驱动

FrameRasterPass SHALL 通过 RasterDraw 声明 shader、几何 streams、index type/offset、draw args 与 set/binding 资源。后端 SHALL 在 Compile 生成匹配的 pipeline 和不可变 bindings；编码时先绑定 Pipeline，再绑定实际活跃的 set 0/1/2/3（Global/Pass/Material/Batch）。切换 layout 后 SHALL 重新绑定所需 sets。动态偏移 SHALL 来自对应声明；几何绑定 SHALL 保留实际 vertex streams、offsets 和 U16/U32 index type，不得硬编码单 stream 或 U32。

#### Scenario: Raster draws 驱动绘制
- **WHEN** 冻结 draw 具有有效 shader 与 Batch bindings
- **THEN** Pipeline 先于所需 ResourceGroups 绑定，应用 dynamic offset 后发出该 item 的绘制

#### Scenario: 多 stream 与 U16 index
- **WHEN** 几何使用多个 vertex streams 与 U16 索引
- **THEN** executor 按其实际布局绑定所有 streams 和索引类型并 DrawIndexed

#### Scenario: 几何与常量属于图读取依赖
- **WHEN** indexed item 使用图中刚上传的 vertex/index 范围与带 dynamic offset 的 UBO 范围
- **THEN** 内容分析保留相应 producer，验证最终读取范围并生成 copy 到 raster 的同步；缺失 producer、越界、错误 usage 或不兼容 reflection 显式失败

#### Scenario: 完整 mip 链纹理采样
- **WHEN** raster draw 声明 RGBA8 Texture2D image 和独立 sampler，纹理由本图上传或先前 Resident 版本提供
- **THEN** 采样读取纳入内容依赖与存活区间，全部暴露 mip 必须已定义；保留 producer 并生成 VS/FS 采样所需同步，拒绝同 pass attachment feedback

#### Scenario: 显式 mip/layer view 与部分内容读取
- **WHEN** 2D/2D-array sampled view 只暴露 image 的一部分 mip/layer，而未暴露子资源没有内容
- **THEN** 图只为暴露范围建立读取依赖并保留其 producer，缺失任一所选内容或越界范围失败；全图 export 仍要求全部内容有效，view 不扩大 initialized 承诺

#### Scenario: shader 资源形态前置校验
- **WHEN** shader 声明 descriptor 数组、comparison/MSAA/storage 或当前 raster 路径不支持的 texture 类型，或者 view 形态与 shader 不匹配
- **THEN** adaptor 在创建 native shader 前拒绝不支持声明，Compile 复核绑定与 view；长度一数组和未知反射信息不得作为受支持 scalar 资源，cache 缺失类型元数据时拒绝加载

#### Scenario: array view 的相对坐标与片段一致性
- **WHEN** 多个编码片段采样不同 baseMip/baseLayer 的 array views，或只暴露一个 mip/layer
- **THEN** LOD/layer 相对各自 view，native SRV/view 范围一致，串行与并行读回一致；array image 的单层切片仍要求 array view 与对应 shader 类型

#### Scenario: sampled view 创建失败与提交退休
- **WHEN** Compile 在部分 native view 创建后失败，或已冻结 view 经取消、不确定提交或完成查询错误退出
- **THEN** 失败清理已创建的未提交对象并允许安全重试；正常编码不创建/修改 view，所有在途 view 与 backing 保活至成功完成或安全 teardown，取消通知和错误回执不提前退休

#### Scenario: 采样 descriptor 冻结与预算失败
- **WHEN** 多个 worker 编码不同 image/sampler/layout 的 draws，或者某次 Compile 的 descriptor 容量不足
- **THEN** 每 draw 的 bindings 在 Compile 冻结并按真实 set 绑定，worker 不写 descriptor；失败恢复本次分配，随后可在同一有效 lease 重试

#### Scenario: 旧新纹理版本同时在途
- **WHEN** 两个提交各自采样旧、新 image 版本，目录被 Prune 且外部 sampler/pins 被释放
- **THEN** 两个提交各自保留原 image/sampler/descriptor 到完成，回读结果保持各自版本，不因新帧绑定而覆写旧帧

#### Scenario: depth 在 scope 与片段之间保留
- **WHEN** D32 attachment 先 CLEAR 再 LOAD，draw 被划分为多个编码片段或 native scope
- **THEN** 仅首个需要清除的片段执行 CLEAR，depth 的 LOAD/STORE、同步及 secondary inheritance 保持有效，串行和并行得到相同遮挡结果

#### Scenario: 稀疏 set 与 layout 切换
- **WHEN** shader 仅使用 set 2，或相邻 draw 切换具有不同 bindings 的 shader layout
- **THEN** native layout 保持真实 set 编号，Pipeline 绑定后重新绑定该 draw 的全部所需 sets 和 dynamic offsets，不沿用不兼容状态

#### Scenario: PSO 复用不混入 draw 绑定数据
- **WHEN** 同一兼容 native scope 内多个 draw 的 program、vertex layout、固定状态及 attachment 签名相同，但 buffer ranges 或 dynamic offsets 不同
- **THEN** 可复用相同 PSO，逐 draw 绑定仍使用各自冻结值；program/layout、vertex stride/attributes、depth/blend 或 attachment 签名不同不得误用同一缓存结果

#### Scenario: 有界跨帧 PSO 与 native scope 缓存
- **WHEN** 后续帧使用相同 immutable pipeline/scope 签名，但 attachment 对象或动态绑定改变
- **THEN** 可复用设备缓存；epoch、shader/layout、vertex/fixed state、附件格式/sample、传统 scope/subpass 不同不得误命中。native scope key 包含 resolve/load/store/layout/dependency，framebuffer/views 仍由当前帧保活；LRU 淘汰只释放 cache owner，失败不入缓存，worker 不查询或修改缓存

#### Scenario: Compute 与 Custom 的声明执行
- **WHEN** 冻结图包含 FrameComputePass 或 FrameCustomPass
- **THEN** 受支持的声明进入后端编译和编码，缺失 encoder、资源访问不合法或超出设备能力时明确失败；不得空分支伪报成功

#### Scenario: shader 生成顶点
- **WHEN** program 的 vertex input 为空，item 使用 shader 生成顶点且 draw args 有效
- **THEN** 可以不绑定 vertex buffer；缺少必需 vertex input 的其它 item 应报告无效并跳过
