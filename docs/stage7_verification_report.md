# Stage 7 Async Session and Safety Cleanup Report

## 1. 范围

阶段 7 在不改变 8.1、8.2 业务参数、SCPI 顺序和 UI 展示语义的前提下，完成：

- `InstrumentSession` 内部串行请求队列。
- 普通请求与安全清理请求分级。
- Session 级取消、主动中止和默认取消源。
- 4071/1466 Driver 安全清理选项透传。
- 8.1、8.2 worker 三级安全清理策略。
- Fake Transport 场景扩展和自动测试补强。

本阶段未改动：

- 8.3 至 8.11 用例逻辑。
- 8.1、8.2 的测试参数、采样次数、采样间隔、判定门限。
- `MainWindow` 既有控件结构、字体、布局和展示字段。
- 完整异步 socket event-loop Session 重写。
- UI 页拆分。

## 2. 实现边界

### 2.1 Session 演进方式

`InstrumentSession` 对外仍保留同步接口：

- `send()`
- `query()`
- `queryBinaryBlock()`

内部改为：

- 每次请求进入 Session 内部串行队列。
- 同一时刻只允许一个活动请求访问 Transport。
- 安全清理请求优先于普通请求调度。
- 普通请求可响应：
  - 默认取消源
  - 显式取消源
  - `abortActiveRequest()`
- 安全清理请求不受普通取消影响。

因此 Driver 和用例层无需再区分同步/异步两套接口，满足阶段 3 的演进约束。

### 2.2 SCPI 类型扩展

`scpitypes.h` 新增：

- `IScpiCancellation`
- `ScpiRequestPriority`
- `ScpiRequestOptions`
- `ScpiStatus::Cancelled`

这些扩展仅用于通信层调度和停止语义，不改变现有成功/失败业务判定。

### 2.3 Driver 变更

新增安全清理透传重载：

- `Analyzer4071::stopMeasurement(const ScpiRequestOptions &)`
- `Generator1466::shutdownOutput(const ScpiRequestOptions &)`

原无参接口仍保留，现有调用点无需改写。

## 3. 三级安全清理

8.1、8.2 worker 的统一策略：

1. 先以 `Safety` 优先级发送常规安全清理命令。
2. 若失败，则调用 `abortActiveRequest()`，再以 `Safety + abortActiveRequest`
   强制重试。
3. 若仍失败，则断开当前仪表连接，记录 `ERROR` 日志，并要求人工确认仪表输出状态。

保持的硬规则：

- 安全清理完成前不发送最终 `finished()`。
- 每轮只发出一次最终 `finished()`。
- 重复停止仍保持幂等。

## 4. 8.1 / 8.2 行为保持

保持不变的关键点：

- 8.1、8.2 仍分别通过 `Analyzer4071`、`Generator1466` 调用。
- 用例不直接操作 `QWidget`、`Ui::MainWindow` 或 Session 私有队列。
- 8.2 的 `configureCw()`、`configureSpectrum()`、marker 采样顺序不变。
- 8.1、8.2 的结果展示字段、日志语义和状态流转名称不变。

## 5. 自动验证

命令：

```bash
./scripts/test_qt512.sh --clean /tmp/aiot-stage7-tests
```

结果：

```text
Build directory guard tests passed
Testcase public boundary checks passed
Totals: 34 passed, 0 failed, 0 skipped
All Qt tests passed
```

新增覆盖：

- Session 并发查询串行化，不发生响应串扰。
- `disconnectFromHost()` 后重连，后续普通查询恢复成功。
- `abortActiveRequest()` 中止挂起普通查询后，后续新的普通查询恢复成功。
- 安全清理请求可中止活动普通查询，并优先完成清理发送。
- 8.2 强制清理重试路径。
- 8.2 断链/持续失败路径：清理失败、断开连接、ERROR 日志、人工确认提示。

既有覆盖继续通过：

- 8.1 生命周期、取消、清理失败、重复运行。
- 8.2 生命周期、取消、清理失败、4071 配置失败、错误队列非零。

## 6. 产品构建和 Smoke

命令：

```bash
./scripts/build_qt512.sh --clean /tmp/aiot-stage7-build
```

结果：

```text
Built /tmp/aiot-stage7-build/AIoT_Test
```

二进制：

```text
SHA-256 e5e780fdc58e137e157d5f2b15f94e2086786886f54ef5804cb27907991dd0a1
```

offscreen smoke：

```bash
mkdir -p /tmp/aiot-stage7-runtime && chmod 700 /tmp/aiot-stage7-runtime
QT_QPA_PLATFORM=offscreen XDG_RUNTIME_DIR=/tmp/aiot-stage7-runtime \
timeout 3s /tmp/aiot-stage7-build/AIoT_Test
```

结果：退出码 `124`，无错误输出，表示应用成功进入事件循环后由 `timeout` 结束。

## 7. 边界检查

通过：

- `git diff --check`
- `tests/scripts/test_testcase_public_boundary.sh`
- `./scripts/test_qt512.sh --clean /tmp/aiot-stage7-tests`

本阶段未新增公共目录对 `case81` / `case82` 私有模型的反向依赖。

## 8. 真实仪表验证

本次阶段 7 实施先完成本地 clean build、自动测试和产品 smoke。

真实仪表联机 probe 未在本轮执行；原因是本阶段改动集中在 Session 内部调度和
清理语义，先以 Fake Transport 和现有 8.1/8.2 probe 构建保证可编译、可回归。
如需审核前补做联机验证，可继续执行 8.1 / 8.2 probe。

## 9. 结论

阶段 7 已完成以下目标：

- Session 内部完成可替换的串行请求调度。
- 8.1、8.2 获得统一三级安全清理。
- 安全清理失败路径具备断链、ERROR 日志和人工确认提示。
- 本地 clean build、34 项自动测试和产品 smoke 全部通过。

当前暂停在阶段 7，等待审核，不进入阶段 8。
