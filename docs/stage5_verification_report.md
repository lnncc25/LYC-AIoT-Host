# Stage 5 Case 8.1 Worker Thread and Lifecycle Report

## 1. 范围

阶段 5 仅对 8.1 基本功能验证引入：

- 专用工作线程。
- 生命周期状态机。
- 原子取消令牌。
- 统一安全清理。
- 唯一最终完成通知。

本阶段未改造：

- 8.2 至 8.11 的执行逻辑。
- 完整异步 `InstrumentSession` 或 SCPI 请求队列。
- 既有 UI 布局、控件、字体和样式。
- 8.1 测试参数、业务判定和基线查询顺序。

## 2. 实现边界

### 2.1 运行控制器

新增：

- `src/testcases/case81/case81runcontroller.h`
- `src/testcases/case81/case81runcontroller.cpp`
- `src/testcases/case81/case81runconfig.h`
- `src/testcases/case81/case81cancellationtoken.h`

`Case81RunController` 位于 GUI 线程，负责：

- 校验启动条件并拒绝重入。
- 为每轮运行创建独立 `QThread`、Worker 和取消令牌。
- 管理 `TestState` 转换。
- 将 Worker 信号排队转发到 UI。
- 保证清理完成后才进入最终状态并发送 `finished()`。
- 用单次完成保护防止重复最终通知。
- 在关闭窗口时请求停止并等待工作线程退出。

停止请求使用原子标志，可从 GUI 线程立即设置，不依赖工作线程事件循环处理，
重复调用不会重复改变状态或重复完成。

### 2.2 工作线程

新增：

- `src/testcases/case81/case81worker.h`
- `src/testcases/case81/case81worker.cpp`

`Case81Worker` 在专用工作线程中创建并拥有本轮运行的：

- `TcpScpiTransport`
- `InstrumentSession`
- `Analyzer4071`
- `Generator1466`
- `TestCase81`

这样避免将现有 `MainWindow` 中仍供其他用例使用的 Session 跨线程移动。当前
Session 内部继续同步执行，没有引入异步请求队列，也没有改变 Driver 或用例的
同步调用接口。

### 2.3 状态和通知顺序

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

执行错误或安全清理失败进入 `ExecutionFailed`。

硬约束：

- `cleanupCompleted()` 必须先于 `finished()`。
- `finished()` 每轮最多发送一次。
- `CleaningUp` 期间拒绝重复启动和用例切换。
- 最终状态保留到下一轮启动，下一轮启动时显式复位到 `Idle`。
- 未使用 `QThread::terminate()`。

### 2.4 取消检查点

8.1 在以下位置检查取消令牌：

- 连接等待期间，每次最多等待 100 ms。
- 每条基线 SCPI 查询完成后。
- 原有 200 ms 延时期间，每 20 ms。
- 测试执行前和安全清理前后。

点击停止后，控制器在 GUI 线程同步进入 `Stopping`；当前同步 Socket 请求仍使用
原有有界超时，取消不会无条件终止底层系统调用。本阶段未提前实现完整异步
Session。

## 3. 安全清理

每个终止路径统一执行：

```text
4071: :ABORt
4071: :INITiate:CONTinuous OFF
1466: :OUTPut:ALL OFF
1466: :OUTPut1:STATe OFF
```

随后断开本轮专用 Session，再发送清理完成和最终通知。

当前 `cleanupSafe=true` 表示安全命令已由 Transport 成功写出，不表示仪表物理
输出状态已经通过查询二次确认。若连接已失效或写入失败：

- 最终原因改为 `ExecutionFailed`。
- UI 记录 `ERROR`。
- UI 显示无法确认仪表安全状态。
- 操作人员必须人工确认仪表输出。

软件不能在网络中断后绝对保证仪表 RF 状态，此限制保持为安全硬约束。

## 4. UI 集成

`MainWindow` 只负责：

- 保存用户实际连接的仪表地址。
- 启动 `Case81RunController`。
- 将结构化摘要、日志和结果交给既有 `Case81UiAdapter`。
- 显示生命周期状态。
- 将停止、用例切换和窗口关闭汇入同一取消与清理路径。

8.1 Worker 和用例目录未引用：

- `QWidget`
- `QTableWidget`
- `Ui::MainWindow`
- `findChild()`
- 其他 8.x 用例私有类型

其他用例仍使用原有同步执行路径。

## 5. 自动测试

命令：

```bash
./scripts/test_qt512.sh --clean /tmp/aiot-stage5-review-tests
```

结果：

- 构建目录保护测试通过。
- 测试用例公共接口边界检查通过。
- Qt 5.12.9 单元测试和集成探针 clean build 通过。
- 22 passed。
- 0 failed。
- 0 skipped。

新增覆盖：

- `start()` 在 100 ms 内返回，不阻塞 GUI 调用线程。
- 正常状态顺序准确。
- 停止后 500 ms 内同步进入 `Stopping`。
- 连续三次停止请求保持幂等。
- 清理通知严格先于最终通知。
- 每轮只发送一次 `finished()`。
- 取消路径最终为 `Cancelled`。
- 正常和取消路径都发送 4071 安全命令。
- 正常路径发送 1466 RF OFF 命令。
- 安全命令写入失败时仍先通知清理结果，再唯一完成为 `ExecutionFailed`。
- 连续运行 20 次，每轮只完成一次。
- 8.1 原有同步业务单元测试继续通过。

## 6. 真实仪表验证

命令：

```bash
timeout 20 /tmp/aiot-stage5-review-tests/integration/case81_probe \
  192.168.110.32 5025 \
  192.168.110.53 5025
```

结果：

```text
STATES=Validating->Preparing->Running->CleaningUp->Completed
EVENTS=cleanup->finished
FINISH_COUNT=1
COMPLETION=Completed
VERDICT=PASS
CLEANUP_SAFE=true
ANALYZER_IDN=Ceyear Technologies,4071E,QZNK000063,1.13.38
GENERATOR_IDN=Ceyear Technologies,1466G-V,QZOC000877,1.7.0
ANALYZER_ERROR=+0, "No error"
GENERATOR_ERROR=+0,"No error"
```

8.1 基线查询顺序仍为：

```text
4071: *IDN? -> :SYSTem:ERRor?
1466: *IDN? -> :SYSTem:ERRor?
```

基线查询完成后追加本阶段要求的安全清理命令。没有发送 RF ON、ARB、频率、功率
或测量配置命令。

## 7. 产品构建与冒烟

clean build：

```bash
./scripts/build_qt512.sh --clean /tmp/aiot-stage5-review-build
```

结果：

- qmake、编译和链接成功。
- 二进制：`/tmp/aiot-stage5-review-build/AIoT_Test`
- SHA-256：`db0a33f0ba1ca613bb6c4754b24a83c81cfe490ca8c3f79e1d990d5b2f279854`
- Qt Core、Gui、Widgets、Network 均链接
  `/opt/Qt5.12.9/5.12.9/gcc_64/lib`。
- 仅存在 Qt 5.12.9 既有 deprecated-copy 编译警告。

offscreen 冒烟：

```bash
QT_QPA_PLATFORM=offscreen \
XDG_RUNTIME_DIR=/tmp/aiot-stage5-review-runtime \
timeout 3s /tmp/aiot-stage5-review-build/AIoT_Test
```

结果：

- 应用在超时前持续运行，成功进入事件循环。

## 8. 阶段结论

阶段 5 的 8.1 异步纵向样板满足：

- 工作线程只覆盖 8.1。
- 安全清理先于最终完成通知。
- 重复停止幂等。
- 每轮唯一完成。
- 清理期间禁止启动和切换。
- 其他用例、UI、测试参数和业务行为未迁移或改造。

当前暂停在阶段 5，等待审核，不进入阶段 6。
