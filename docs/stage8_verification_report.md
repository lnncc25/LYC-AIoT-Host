# Stage 8 Case 8.5 Async Vertical Slice Report

## 1. 范围

阶段 8 仅实现批准范围内的 `8.5 ACLR` 纵向拆分与异步化：

- 新增 `case85/` 用例私有目录。
- 将 8.5 的结果模型、执行逻辑、worker、run controller 下沉到用例私有层。
- 8.5 真实仪表路径统一通过 `Analyzer4071`、`Generator1466` Driver 调用。
- 为 8.5 引入专用工作线程、生命周期状态机和取消机制。
- 保持其他用例逻辑不迁移、不改造。

本阶段未改动：

- 8.1、8.2、8.6、8.7 等既有已批准用例逻辑。
- UI 布局、控件结构、字体和用户操作路径。
- 8.5 业务参数、SCPI 顺序、测试点组合和截图目录语义。
- 完整异步 Session 重写。

## 2. 目录与边界

新增 8.5 私有模块：

- `src/testcases/case85/case85cancellationtoken.h`
- `src/testcases/case85/case85model.h`
- `src/testcases/case85/icase85resultpresenter.h`
- `src/testcases/case85/testcase85.h`
- `src/testcases/case85/testcase85.cpp`
- `src/testcases/case85/case85worker.h`
- `src/testcases/case85/case85worker.cpp`
- `src/testcases/case85/case85runcontroller.h`
- `src/testcases/case85/case85runcontroller.cpp`

边界保持：

- `case85` 不直接操作 `Ui::MainWindow`、`QWidget` 或裸 `InstrumentSession`。
- `MainWindow` 仅通过 `Case85RunController` 接收摘要、日志、逐行结果和最终结果。
- 公共接口目录未新增 `Case85Result` 依赖；8.5 结果展示接口保留在 `case85` 私有目录。

## 3. 驱动层变化

为承接 8.5 私有逻辑，扩展了最小 Driver 能力，但仍保持同步调用语义：

- `Analyzer4071`
  - `configureAclr(...)`
  - `measureAclr()`
  - `saveScreenSnapshot(...)`
  - `downloadFile(...)`
  - `deleteFile(...)`
  - `parseAcpowerResponse(...)` 恢复兼容旧解析器支持的扩展六字段、legacy power
    list、relative ACLR tuple 和 direct ACLR tuple
- `Generator1466`
  - `configureArbPlayback(...)`

约束：

- 8.5 不再直接操作 Session 或 Socket。
- 8.5 真实路径的 SCPI 发送顺序保持原先主流程语义。
- Driver 扩展只承载 8.5 已有同步行为，没有引入异步请求队列。

## 4. 生命周期与停止语义

8.5 现改为专用 worker/thread/controller 执行，并复用已批准的生命周期规则：

- `Running -> Stopping -> CleaningUp -> Completed|Failed|Cancelled`
- 安全清理完成后才允许发出唯一一次 `finished()`
- 重复点击停止保持幂等
- 停止期间禁止继续切换到其他测试页面

阶段 8 新增覆盖的停止点：

- 带宽/频点之间停止
- 截图二进制下载期间停止

本阶段仍保持同步仪表调用，每轮线程内创建专用 Transport/Session/Driver，不改造其他用例。

## 5. 8.5 行为保持

已保持的 8.5 关键行为：

- 带宽配置、频点来源和组合逻辑保持不变。
- 频点文本仍按 `[,\s]+` 分割；无有效配置时仍回退 `925.0 MHz`。
- ARB 时钟仍为 `1.92 MHz`。
- ACLR 配置和读取仍经 4071 执行，判定逻辑保持不变。
- 截图目录仍使用 `screenshots/8.5_ACLR/<batch>`。
- 截图文件名仍为带时间戳的 `ACLR_...png` 形式。
- 仿真模式结果范围继续兼容原先 UI 演示语义。
- 结果表、统计图和日志展示字段保持原有前端行为。

## 6. 自动验证

命令：

```bash
./scripts/test_qt512.sh --clean /tmp/aiot-stage8-approval-tests
```

结果：

```text
Build directory guard tests passed
Testcase public boundary checks passed
Totals: 41 passed, 0 failed, 0 skipped
All Qt tests passed
```

本阶段新增覆盖：

- `case85AsyncLifecycle()`
- `case85AsyncCancellationDuringScreenshotDownload()`
- `case85AsyncScreenshotLocalWriteFailure()`
- `analyzerAcpowerParsing()`
- `case85_probe` 构建目标纳入测试工程

验证点包括：

- 8.5 异步运行后只完成一次
- 清理先于最终完成通知
- 停止在截图下载阻塞期间仍可生效
- 4071 `:READ:ACPower?` 响应兼容：
  - 实机常见扩展六字段格式
  - legacy power list
  - relative ACLR tuple
  - direct ACLR tuple
- 截图本地打开/写入失败时不删除仪表端文件，并记录 `ERROR` 日志

## 7. 产品构建

命令：

```bash
./scripts/build_qt512.sh --clean /tmp/aiot-stage8-approval-build
```

结果：

```text
Built /tmp/aiot-stage8-approval-build/AIoT_Test
```

二进制：

```text
SHA-256 df44dabe6e482f7bd7e7473e786e413ce0c3676712e308741af665e3d81d81bc
```

## 8. 边界检查

本阶段应继续通过：

- `git diff --check`
- `tests/scripts/test_testcase_public_boundary.sh`
- `./scripts/test_qt512.sh --clean /tmp/aiot-stage8-approval-tests`

其中公共边界检查用于防止公共模块重新依赖 `case8x` 私有模型。

## 9. 真实仪表验证

实机回归时间：

- 2026-06-23 23:23（Asia/Shanghai）

实机设备：

- 4071E：`192.168.110.32:5025`
- 1466G-V：`192.168.110.53:5025`

执行命令：

```bash
timeout 90 /tmp/aiot-stage8-fix-tests/integration/case85_probe \
  192.168.110.32 5025 192.168.110.53 5025 0 925
```

实机结果：

```text
STATES=Validating->Preparing->Running->CleaningUp->Completed
EVENTS=cleanup->finished
FINISH_COUNT=1
COMPLETION=Completed
VERDICT=PASS
CLEANUP_SAFE=true
ROWS=2
PASS_COUNT=2
FAIL_COUNT=0
```

ACLR 数值：

- 第一邻道 ACLR（±300 kHz / IBW 180 kHz）
  - 左 ACLR：`63.26 dB`
  - 右 ACLR：`70.44 dB`
  - 限值：`40.8 dB`
- 第二邻道 ACLR（±500 kHz / IBW 180 kHz）
  - 左 ACLR：`76.81 dB`
  - 右 ACLR：`77.19 dB`
  - 限值：`45.8 dB`

截图落盘：

- 目录：`/home/hotwater/Documents/AIoT-Host-6.23/LYC-AIoT-Host/screenshots/8.5_ACLR_probe/20260623_232300`
- 文件：
  - `ACLR_925.000MHz_BW200k_OFF300k_IBW180k_PASS_232301.png`
  - `ACLR_925.000MHz_BW200k_OFF500k_IBW180k_PASS_232302.png`

远端删除与安全清理证据：

- Probe 日志记录了每张截图的顺序：
  - `:MMEMory:STORe:SCReen`
  - `:MMEMory:DATA?`
  - `:MMEMory:DELete`
- 最终清理顺序为：
  - 4071：`:ABORt` -> `:INITiate:CONTinuous OFF`
  - 1466：`:OUTPut:ALL OFF` -> `:OUTPut1:STATe OFF`
- `cleanup->finished` 顺序和 `FINISH_COUNT=1` 满足阶段 8 生命周期约束。

## 10. 结论

阶段 8 已在批准边界内完成：

- 8.5 私有模型、执行逻辑和生命周期控制下沉到 `case85`
- 8.5 真实路径改为仅通过 Driver 调用
- 8.5 获得专用工作线程、停止机制和唯一完成通知约束
- 4071 ACLR 解析兼容旧实现支持的实机格式
- 截图仅在本地完整写入成功后才删除仪表端文件
- 本地 clean build、41 项自动测试、产品构建和真实双仪表 8.5 全流程回归通过

当前暂停在阶段 8，等待审核，不进入阶段 9。
