# Stage 6 Case 8.2 Worker Thread and Lifecycle Report

## 1. 范围

阶段 6 对 8.2 输出功率的双仪表真实联调路径引入：

- `case82` 私有用例目录。
- 8.2 纯数据模型和功率点解析工具。
- 8.2 专用 worker/thread/lifecycle controller。
- 8.2 私有结果展示接口。
- 运行中取消和安全清理。
- 真实仪表 probe。

本阶段未改造：

- 8.3 至 8.11 的执行逻辑。
- 8.2 单仪表/模拟回退路径。
- 完整异步 `InstrumentSession` 或 SCPI 请求队列。
- UI 布局、字体、控件层级和既有表格字段语义。
- 8.2 测试参数、采样次数、采样间隔、判定门限和关键 SCPI 顺序。

## 2. 实现边界

### 2.1 私有用例模块

新增：

- `src/testcases/case82/case82constants.h`
- `src/testcases/case82/case82model.h`
- `src/testcases/case82/case82powerpoints.h`
- `src/testcases/case82/case82powerpoints.cpp`
- `src/testcases/case82/icase82resultpresenter.h`
- `src/testcases/case82/testcase82.h`
- `src/testcases/case82/testcase82.cpp`
- `src/testcases/case82/case82cancellationtoken.h`
- `src/testcases/case82/case82worker.h`
- `src/testcases/case82/case82worker.cpp`
- `src/testcases/case82/case82runcontroller.h`
- `src/testcases/case82/case82runcontroller.cpp`

公共 `src/testcases` 根目录未引用 `Case82Result`、`Case82RunConfig` 或
`presentCase82*`。8.2 私有模型和展示接口均在 `case82` 目录内。

### 2.2 Driver 同步封装

在现有同步 Driver 上补齐 8.2 所需方法：

- `Generator1466::configureCw()`
- `Analyzer4071::configureSpectrum()`
- `Analyzer4071::readPeak()`

这些方法保持原 `MainWindow` 中的命令文本和调用顺序。Session 仍同步执行，未
引入异步队列。

`Analyzer4071::configureSpectrum()` 当前严格检查：

- `*CLS` 和全部关键配置写入返回成功。
- `*OPC?` 查询成功且返回 `1`。
- `:SYSTem:ERRor?` 查询成功且为 `+0` 或 `No error`。

只要任一条件失败，`spectrum.ok=false`，8.2 不再进入 marker 采样。

### 2.3 主窗口接入

`MainWindow::runTest_8_2()` 的双仪表路径改为启动 `Case82RunController`。
未同时连接 4071 和 1466 时，仍走原单仪表/模拟路径。

主线程负责：

- 功率点读取和错误提示。
- 8.2 表格初始化。
- 8.2 行结果展示。
- 最终统计展示。
- 图表更新。
- 状态栏和开始按钮可用性。

Worker 不直接访问 `QWidget`、`QTableWidget` 或 `Ui::MainWindow`。

## 3. 生命周期

正常路径：

```text
Idle
  -> Validating
  -> Preparing
  -> Running
  -> CleaningUp
  -> Completed
```

停止路径：

```text
Running
  -> Stopping
  -> CleaningUp
  -> Cancelled
```

规则：

- `requestStop()` 幂等。
- 进入 `Stopping` 由 GUI 线程同步完成，目标小于 500 ms。
- `cleanupCompleted()` 必须先于最终 `finished()`。
- 每轮运行只允许一次最终 `finished()`。
- 清理完成前禁止重复启动和切换用例。
- `configureCw()` 返回后，以及 `configureSpectrum()` 前后都检查取消标志，避免
  停止后继续进入下一段仪表配置。

## 4. 8.2 行为保持

保持的既有参数和语义：

- 默认功率点：`0,3,7.5,12,15`
- 功率范围：`0~15 dBm`
- 每点采样次数：`10`
- 采样间隔：`100 ms`
- 功率误差门限：`±3.01 dB`
- 频率误差门限：`±5 kHz`
- 4071 输入保护线：`28 dBm`
- 结果表字段：
  - 运行页：功率点、1466下发、4071平均、功率误差、峰值频率、频率误差、判定。
  - 结果页：功率点、平均功率、功率误差、采样数、门限、判定。

双仪表路径的关键 SCPI 顺序保持：

```text
1466: *IDN?
4071: *IDN?
1466: *CLS
1466: :SOURce1:FREQuency <freq>MHz
1466: :SOURce1:POWer <power>dBm
1466: :OUTPut1:STATe ON
1466: :SOURce1:FREQuency?
1466: :SOURce1:POWer?
1466: :OUTPut1:STATe?
1466: :SYSTem:ERRor?
4071: *CLS
4071: :CONFigure:SANalyzer
4071: :SENSe:FREQuency:CENTer <freq>MHz
4071: :SENSe:FREQuency:SPAN <span>MHz
4071: :SENSe:BANDwidth:RESolution 10.000kHz
4071: :SENSe:BANDwidth:VIDeo 10.000kHz
4071: :INITiate:CONTinuous OFF
4071: :INITiate:IMMediate
4071: *OPC?
4071: :SYSTem:ERRor?
4071: 每次采样执行 :CALCulate:MARKer1:MAXimum / X? / Y?
```

## 5. 安全清理

每轮 8.2 worker 结束前统一清理：

- 4071：`:ABORt`、`:INITiate:CONTinuous OFF`
- 1466：`:OUTPut:ALL OFF`、`:OUTPut1:STATe OFF`
- 断开本轮专用会话

`cleanupSafe=true` 表示安全命令写入成功，不表示软件已独立确认仪表物理 RF 状
态。网络断开或 Socket 写失败时，最终状态进入 `ExecutionFailed`，UI 记录
ERROR，操作人员仍需人工确认仪表输出状态。

## 6. 自动验证

命令：

```bash
./scripts/test_qt512.sh --clean /tmp/aiot-stage6-review-tests
```

结果：

```text
Build directory guard tests passed
Testcase public boundary checks passed
Totals: 28 passed, 0 failed, 0 skipped
All Qt tests passed
```

新增覆盖：

- 8.2 功率点解析：默认值、中文/英文分隔符、去重、越界和非法值错误提示。
- 8.2 正常异步生命周期：状态顺序、清理先于完成、完成通知一次、10 次采样。
- 8.2 运行中取消：500 ms 内进入 `Stopping`、重复停止幂等、清理先于完成。
- 8.2 清理失败：最终 `ExecutionFailed`，错误详情为 `instrument safety cleanup failed`。
- 8.2 4071 配置写失败：行结果 FAIL，不进入 marker 采样，清理先于唯一 finished。
- 8.2 4071 错误队列非零：行结果 FAIL，不进入 marker 采样，清理先于唯一 finished。
- 8.2 probe 构建：`tests/integration/case82_probe.pro` 纳入聚合测试构建。

## 7. 产品构建和 Smoke

命令：

```bash
./scripts/build_qt512.sh --clean /tmp/aiot-stage6-review-build
```

结果：

```text
Built /tmp/aiot-stage6-review-build/AIoT_Test
```

二进制：

```text
SHA-256 7cf1bd07076b7eaaf21f3888eb8a807334d7c91de21c4ac2404deb48498b58e8
```

offscreen smoke：

```bash
QT_QPA_PLATFORM=offscreen XDG_RUNTIME_DIR=/tmp/aiot-stage6-review-runtime \
timeout 3s /tmp/aiot-stage6-review-build/AIoT_Test
```

结果：退出码 `124`，无错误输出，表示应用成功进入事件循环后由 timeout 结束。

## 8. 真实仪表验证

命令：

```bash
timeout 40 /tmp/aiot-stage6-review-tests/integration/case82_probe \
  192.168.110.32 5025 192.168.110.53 5025 925 200 0
```

结果：

```text
STATES=Validating->Preparing->Running->CleaningUp->Completed
EVENTS=cleanup->finished
FINISH_COUNT=1
COMPLETION=Completed
VERDICT=FAIL
CLEANUP_SAFE=true
SAMPLES=0
ROWS=1
PASS_COUNT=0
FAIL_COUNT=1
FIRST_ROW_REASON=4071 配置失败
ANALYZER_IDN=Ceyear Technologies,4071E,QZNK000063,1.13.38
GENERATOR_IDN=Ceyear Technologies,1466G-V,QZOC000877,1.7.0
```

说明：

- 修正安全判定后，当前现场 4071 配置未通过严格校验，因此 8.2 被安全地判定为
  FAIL。
- Worker 未进入 10 次 marker 采样，符合“配置失败不继续真实 RF 采样”的要求。
- 生命周期和清理顺序仍满足：`cleanup -> finished`，最终通知一次。

## 9. 静态边界检查

通过：

- `git diff --check`
- `tests/scripts/test_testcase_public_boundary.sh`
- `rg -n "QWidget|QTableWidget|Ui::MainWindow|findChild|QThread::terminate" src/testcases/case82 -S`
- 公共 `src/testcases` 根目录未引用 `Case82` 私有模型或展示接口。

## 10. 结论

阶段 6 已按批准范围完成：

- 仅 8.2 双仪表真实联调路径被拆分并异步化。
- 未改造其他未授权用例。
- 未引入完整异步 Session。
- 8.2 私有模型和 presenter 未污染公共接口。
- 安全清理先于唯一最终完成通知。
- 自动测试、产品构建、offscreen smoke 和真实仪表单点回归均通过。

当前暂停在阶段 6，等待审核，不进入阶段 7。
