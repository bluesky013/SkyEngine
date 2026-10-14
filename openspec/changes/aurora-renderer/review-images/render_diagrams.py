"""Render the Aurora design review figures with Pillow; no runtime code changes."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).parent
FONT = 'C:/Windows/Fonts/msyh.ttc'
BOLD = 'C:/Windows/Fonts/msyhbd.ttc'
INK = '#172B46'
MUTED = '#53677E'
LINE = '#73889E'
BG = '#F5F8FC'
COLORS = {
    'blue': ('#E9F2FF', '#8FB5E8'),
    'purple': ('#F0EBFC', '#B6A0DD'),
    'green': ('#E7F5EF', '#8EC7AF'),
    'amber': ('#FFF4DC', '#DCC180'),
    'white': ('#FFFFFF', '#C5D3E0'),
}


class Diagram:
    def __init__(self, height, title, subtitle):
        self.image = Image.new('RGB', (1600, height), BG)
        self.draw = ImageDraw.Draw(self.image)
        self.text((64, 36), title, 48, bold=True)
        self.text((66, 105), subtitle, 25, color=MUTED)

    def text(self, xy, text, size=29, color=INK, bold=False, anchor=None):
        font = ImageFont.truetype(BOLD if bold else FONT, size)
        self.draw.text(xy, text, font=font, fill=color, anchor=anchor)

    def box(self, rect, title, detail='', color='white', size=30):
        fill, border = COLORS[color]
        self.draw.rounded_rectangle(rect, radius=18, fill=fill, outline=border, width=2)
        x0, y0, x1, y1 = rect
        lines = [(s, size, True) for s in title.split('\n')]
        if detail:
            lines += [(s, 25, False) for s in detail.split('\n')]
        height = sum(s + 12 for _, s, _ in lines) - 12
        y = (y0 + y1 - height) / 2
        for s, fs, bold in lines:
            self.text(((x0 + x1) / 2, y), s, fs, INK if bold else MUTED, bold, 'mt')
            y += fs + 12

    def arrow(self, pts, color=LINE, width=3, dashed=False):
        if dashed:
            for p, q in zip(pts, pts[1:]):
                dx, dy = q[0] - p[0], q[1] - p[1]
                length = math.hypot(dx, dy)
                for start in range(0, int(length), 18):
                    end = min(start + 10, length)
                    self.draw.line([(p[0] + dx*start/length, p[1] + dy*start/length),
                                    (p[0] + dx*end/length, p[1] + dy*end/length)], fill=color, width=width)
        else:
            self.draw.line(pts, fill=color, width=width, joint='curve')
        p, q = pts[-2:]
        angle = math.atan2(q[1] - p[1], q[0] - p[0])
        self.draw.polygon([q,
            (q[0] - 13*math.cos(angle - .5), q[1] - 13*math.sin(angle - .5)),
            (q[0] - 13*math.cos(angle + .5), q[1] - 13*math.sin(angle + .5))], fill=color)

    def footer(self, y, lines):
        self.draw.line((64, y, 1536, y), fill='#D4DFEA', width=2)
        for i, line in enumerate(lines):
            self.text((66, y + 20 + 38*i), line, 24, MUTED)

    def save(self, name):
        self.image.save(OUT / name)


def architecture():
    d = Diagram(1160, '01  Aurora 主体关系', '对象与数据关系 · 一个 Scene 服务多个 View · 本图不表示执行时间轴')
    d.box((400, 170, 1160, 265), '主线程  World / Editor UI', '场景变更 + 相机请求 + UI 数据快照')
    d.arrow([(780, 265), (780, 295)])
    d.box((400, 295, 1160, 390), 'IRenderer / FramePacket', '拥有数据的命令包 · 有界队列', 'blue')
    d.arrow([(780, 390), (780, 425)])
    d.box((400, 425, 1160, 520), 'Renderer 协调器', '应用命令、CPU 调度、GPU 建图与提交', 'blue')
    d.arrow([(620, 520), (620, 558)])
    d.box((350, 558, 890, 653), 'SceneState · 共享 ECS', '结构冻结后执行 prepare', 'purple')
    d.arrow([(620, 653), (620, 680), (340, 680), (340, 710)])
    d.arrow([(620, 680), (900, 680), (900, 710)])
    d.box((90, 710, 590, 805), 'View A', '独立相机 / 裁剪 / LOD / 历史', 'purple')
    d.box((650, 710, 1150, 805), 'View B', '独立相机 / 裁剪 / LOD / 历史', 'purple')
    d.arrow([(340, 805), (340, 840)])
    d.arrow([(900, 805), (900, 840)])
    d.box((90, 840, 590, 930), 'Viewport A · 离屏输出', color='green')
    d.box((650, 840, 1150, 930), 'Viewport B · 原生窗口', color='green')
    d.arrow([(340, 930), (340, 965)])
    d.box((90, 965, 1150, 1035), 'UI / FrameComposer → 编辑器主窗输出', color='green', size=28)
    d.arrow([(1160, 472), (1340, 472), (1340, 558)], dashed=True)
    d.box((1200, 558, 1536, 930), 'Renderer\n统一持有', 'Device\nResourceCache\nFrameScheduler', 'amber', 29)
    d.footer(1060, ['View 描述“如何看”；Viewport 描述“输出到哪里”；设备与资源缓存共享。',
                    'UI 快照同样经 FramePacket 进入；扩展接口仍需按评审 R5 固定。'])
    d.save('01-architecture.png')


def frame():
    d = Diagram(1490, '02  单帧执行流程 · 设计版', '包含 R2 上传同步与 R4 输出获取调整 · 已纳入设计，尚未实施')
    d.box((160, 165, 1440, 255), '应用命令 → 输出配置快照 → 冻结 ECS 结构', color='blue')
    for x in (310, 800, 1290):
        d.arrow([(800, 255), (800, 280), (x, 280), (x, 310)])
    d.box((90, 310, 530, 405), 'Bounds prepare', '按访问依赖拆分 chunks', 'purple')
    d.box((580, 310, 1020, 405), 'Light prepare', '无冲突时并行', 'purple')
    d.box((1070, 310, 1510, 405), 'Object prepare', '私有输出 / 稳定归并', 'purple')
    for x in (310, 800, 1290):
        d.arrow([(x, 405), (x, 430), (800, 430), (800, 455)])
    d.box((430, 455, 1170, 530), '该 Scene 的 SceneReady', color='purple')
    for x in (470, 1130):
        d.arrow([(800, 530), (800, 555), (x, 555), (x, 580)])
    d.box((160, 580, 780, 675), 'View A prepare', 'Cull / LOD / Extract', 'purple')
    d.box((820, 580, 1440, 675), 'View B prepare', 'Cull / LOD / Extract', 'purple')
    for x in (470, 1130):
        d.arrow([(x, 675), (x, 700), (800, 700), (800, 725)])
    d.box((160, 725, 1440, 805), 'CPU jobs 退出 + 结果归并完成', color='purple')
    d.arrow([(800, 805), (800, 840)])
    d.box((160, 840, 1440, 935), '可复用 FrameSlot → 有界 Acquire → 输出版本复核', '等待期间仍处理控制事件；输出变化时重做受影响 View 或跳过', 'amber', 29)
    d.arrow([(800, 935), (800, 970)])
    d.box((160, 970, 1440, 1065), 'ResourceFinalize → Build RDG → Compile → Record', '创建 backing / UploadPlan；分配本 slot 的 UBO、descriptor、staging', 'blue', 29)
    d.arrow([(800, 1065), (800, 1100)])
    d.box((420, 1100, 1180, 1175), 'Submit：检查是否接受提交', color='blue')
    d.arrow([(800, 1175), (800, 1200), (230, 1200), (230, 1250)])
    d.text((930, 1210), 'GPU · 同一 graphics queue', 25, MUTED)
    rects = [(65, 1250, 395, 1330), (445, 1250, 775, 1330), (825, 1250, 1155, 1330), (1205, 1250, 1535, 1330)]
    labels = ['Upload copy', 'Scene Draw', 'UI / Composite', '输出最终状态']
    for rect, label in zip(rects, labels):
        d.box(rect, label, color='green', size=28)
    for a, b in zip(rects, rects[1:]):
        d.arrow([(a[2], 1290), (b[0], 1290)])
    d.footer(1360, ['CPU deps DAG 管计算依赖；GPU RDG 管资源读写、执行顺序与 barrier。',
                    'CPU prepare 使用私有 scratch；GPU slot 资源只能在确认可复用后分配。',
                    '图中并列任务只有在声明无冲突时并行；首版 kernel 不使用协程挂起。'])
    d.save('02-frame-pipeline.png')


def lifetime():
    d = Diagram(1120, '03  取消与资源回收 · 设计版', '三个执行域分别证明不再使用资源 · 已纳入设计，尚未实施')
    cols = [(65, 510), (577, 1023), (1090, 1535)]
    stages = [
        ('CPU 任务与借用', 'purple', [('CancelRequested', '仅设置取消请求'), ('停止新工作 / 跳过未启动', '已运行 kernel 继续退出流程'), ('全部 CPU kernels 退出', '发布最终 JobGroup 结果'), ('结果 borrowers 也已退出', 'CPU scratch / ECS 借用可回收')]),
        ('GPU 提交与 FrameSlot', 'blue', [('Submit 已接受', '不代表 GPU 已完成'), ('查询 GPU 完成', '处理失败 / device lost'), ('Submission 完成', '已有可靠完成证明'), ('FrameSlot GPU 引用', '满足对应退休条件')]),
        ('窗口呈现', 'green', [('Present 已排队', '仍可能被呈现系统持有'), ('跟踪呈现退休', '使用平台对应完成协议'), ('Presentation 已退休', 'GPU fence 不能替代此证明'), ('输出 / swapchain 引用', '满足对应退休条件')]),
    ]
    for (x0, x1), (title, color, boxes) in zip(cols, stages):
        cx = (x0 + x1) / 2
        d.text((cx, 170), title, 29, bold=True, anchor='mt')
        for i, (label, detail) in enumerate(boxes):
            y = 225 + i*140
            d.box((x0, y, x1, y+105), label, detail, color, size=26)
            if i < 3:
                d.arrow([(cx, y+105), (cx, y+140)])
        d.arrow([(cx, 750), (cx, 800), (800, 800), (800, 840)])
    d.box((230, 840, 1370, 945), '旧输出资源回收 / 关闭确认', '相关资源的 CPU、GPU、呈现条件全部满足后执行', 'amber')
    d.footer(985, ['关闭前还需停止新 Acquire，并从待录制图与快照中移除对应输出引用。',
                   'FrameSlot 的一般复用只检查它实际持有的资源；不要求每帧等待所有窗口呈现结束。'])
    d.save('03-resource-lifetimes.png')


if __name__ == '__main__':
    architecture()
    frame()
    lifetime()
    print(f'Wrote 3 diagrams to {OUT}')
