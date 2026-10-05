## MODIFIED Requirements

### Requirement: Tick 驱动最小出帧闭环

`AuroraModule::Tick` SHALL 每帧执行：frame begin → viewport `Begin`/`Acquire` → backbuffer `UNDEFINED → COLOR_ATTACHMENT` barrier → `BeginRendering`（`LoadOp::CLEAR`）→ `COLOR_ATTACHMENT → PRESENT` barrier → `Submit`（wait acquire sema、signal render-done sema、frame fence）→ `Release`/present → frame end。SHALL 无 window 或 `Begin`/`Acquire` 失败时取消该帧（不 present）。SHALL 在 `Acquire` 之前检查 surface 尺寸，extent 为零时跳过该帧而不 acquire；若已 acquire 但 backbuffer 不可用，SHALL `Release` 该图像而非遗留未 present。

#### Scenario: 出帧并呈现

- **WHEN** viewport 可用且 Acquire 成功
- **THEN** backbuffer 被 clear、提交并以 render-done 信号量 present

#### Scenario: acquire 失败取消

- **WHEN** `Begin()` 或 `Acquire()` 返回 false（最小化 / OUT_OF_DATE / LOST）
- **THEN** 不提交、不 present，且不崩溃

#### Scenario: 零尺寸不 acquire

- **WHEN** 帧开始时 surface extent 为零
- **THEN** 不 acquire backbuffer，直接结束该帧

#### Scenario: 不可用的已获取图像不遗留

- **WHEN** 已 acquire 但没有可用的 backbuffer
- **THEN** 先 `Release` 该图像再返回
