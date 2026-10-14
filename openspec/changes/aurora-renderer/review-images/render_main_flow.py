"""Integrated Aurora main flow; generated design figure, not runtime code."""
from render_diagrams import Diagram


def main_flow():
    d = Diagram(1975, '12  Aurora 渲染主流程',
                '目标设计 · ECS + FrameGraph + 多 Viewport · 后端完整 Compile / FrameContext Dispatch')
    d.box((65, 165, 1000, 300), '宿主主线程 · World / Editor UI / SceneBridge',
          '场景增量 · 相机与输出意图 · UI 快照\nIRenderer writer 汇集拥有数据的命令', 'blue', 31)
    d.box((1100, 165, 1535, 300), '资产 executor',
          'IO / 解码 / CPU 编译\nCpuReady 先回主线程归并', 'purple', 30)
    d.arrow([(1100, 230), (1000, 230)])
    d.arrow([(532, 300), (532, 330), (800, 330), (800, 360)])
    d.box((65, 360, 1535, 455), 'FramePacket → render coordinator',
          '有界 FIFO · 校验 generation / revision · 应用 ECS 增量 · 输出配置快照 · 冻结结构', 'blue', 31)
    d.arrow([(800, 455), (800, 490)])
    d.box((65, 490, 1535, 625), 'ScenePrepareScheduler → 专用 CPU prepare pool',
          '每 Scene：按组件读写依赖并行 prepare → SceneReady\n每 View：Cull / LOD / Extract → 稳定归并 → PreparedFrame', 'purple', 31)
    d.arrow([(800, 625), (800, 660)])
    d.box((65, 660, 1535, 755), 'FrameScheduler → TryBeginSlot / Acquire outputs',
          '取得唯一 GPU slot lease · 复核输出 extent / generation · 各 View 分配独立切片', 'blue', 31)
    d.arrow([(800, 755), (800, 790)])
    d.box((65, 790, 1535, 920), 'ResourceFinalize + ScenePipeline / FrameComposer',
          '预算内创建资源 / PSO · 准备 staging / UploadBatch\n声明共享 Upload → 各 View → UI / Composite → 输出 roots；Freeze 逻辑图', 'blue', 31)
    d.arrow([(800, 920), (800, 955)])
    d.box((65, 955, 1535, 1090), '当前后端 Compile · Vulkan / DX12 / Metal',
          '分析 / 剔除 / GPU 顺序 → Native Passes → Backing / Bindings / 同步\n生成 BackendCompiledFrame：EncodeTaskGraph + SubmissionPlan + ResourceLeases', 'green', 31)
    d.arrow([(800, 1090), (800, 1125)])
    d.box((65, 1125, 1535, 1260), '同一 DeviceFrameContext.Dispatch',
          'coordinator 步骤 + 后端 encode workers / dispatcher\n独占编码资源 → 收齐任务 → 按计划组装 → RecordedFrame', 'green', 31)
    d.arrow([(800, 1260), (800, 1295)])
    d.box((65, 1295, 1535, 1390), 'FrameScheduler 请求 DeviceFrameContext.Submit',
          'Accepted：登记 SubmissionRecord / 资源状态增量；上传进入 UploadSubmitted', 'green', 31)
    d.arrow([(800, 1390), (800, 1430), (415, 1430), (415, 1470)])
    d.arrow([(800, 1430), (1180, 1430), (1180, 1470)])
    d.box((65, 1470, 765, 1660), 'GPU 执行',
          '按后端计划执行 Copy / Draw / Compute\n真实 completion → slot 可退休\n上传成功完成 → Resident', 'green', 31)
    d.box((835, 1470, 1535, 1660), '输出呈现 / 消费',
          '窗口按平台协议排队 Present\n使用对应 render-done 同步\nPresentation retirement 独立确认', 'amber', 31)
    d.arrow([(415, 1660), (415, 1690), (800, 1690), (800, 1720)])
    d.arrow([(1180, 1660), (1180, 1690), (800, 1690)])
    d.box((65, 1720, 1535, 1820), 'Completion / Control Pump · 贯穿渲染循环',
          '各自满足条件后回收 CPU / GPU / 输出资源 · 发布状态事件 · 推进关闭 / Shutdown', 'blue', 31)
    d.footer(1860, ['CPU prepare 不先占用 GPU slot；CPU 帧 N+1 可与 GPU 帧 N 重叠。',
                    'ECS 依赖图、GPU 资源图、CPU 编码任务图分开；编译完成 / 提交接受 / GPU 就绪分开。'])
    d.save('12-renderer-main-flow.png')


if __name__ == '__main__':
    main_flow()
    print('Wrote integrated renderer figure 12')
