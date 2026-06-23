# Stage 4 Synchronous Case 8.1 Vertical Slice Report

## 1. 范围

阶段 4 将 8.1 基本功能验证从 `MainWindow` 同步拆分为独立纵向样板。

本阶段严格保持：

- 主线程同步执行。
- 既有 SCPI 命令、顺序和超时。
- 既有双仪表与仅分析仪兼容分支。
- 既有测试参数、判定逻辑、表格内容和日志文案。
- 既有 UI 布局、字体、样式和用户操作入口。

本阶段未引入：

- 工作线程或线程切换。
- 状态机。
- 异步请求队列。
- Future、Promise 或并发执行。
- 其他测试用例迁移。

## 2. 纵向边界

### 2.1 用例接口

新增：

- `src/testcases/itestcase.h`
- `src/testcases/itestcaseview.h`
- `src/testcases/itesteventsink.h`
- `src/testcases/testcaseregistry.h`
- `src/testcases/testcaseregistry.cpp`

`ITestCase` 明确：

- 枚举型 `TestCaseId`。
- 展示名称。
- 启动前检查。
- 同步 `start()` 和结构化 `TestCompletion`。
- 幂等停止请求入口。
- 虚析构。

`TestCaseRegistry` 拥有用例对象，`MainWindow` 只按 `TestCaseId::Case81` 查找并启动，
不再包含 8.1 的测试执行流程。

### 2.2 8.1 用例

新增：

- `src/testcases/case81/case81model.h`
- `src/testcases/case81/icase81resultpresenter.h`
- `src/testcases/case81/testcase81.h`
- `src/testcases/case81/testcase81.cpp`

`TestCase81` 只依赖：

- `Analyzer4071`
- `Generator1466`
- `ITestCaseView`
- `ITestEventSink`
- `ICase81ResultPresenter`

结果展示接口与 `Case81Result` 一同位于 `case81/` 私有目录。公共
`src/testcases/` 文件不引用 `Case81Result`，也不声明 `presentCase81()`，后续
拆分其他用例不需要修改共享结果接口。

静态边界检查确认用例目录不包含：

- `QWidget`、`QTableWidget` 或 `Ui::MainWindow`。
- `InstrumentSession`。
- `findChild()`。
- Session 直接调用。

公共接口自动边界检查：

- 脚本：`tests/scripts/test_testcase_public_boundary.sh`
- 扫描范围：`src/testcases/` 目录直属的 `.h` 和 `.cpp` 公共文件。
- 禁止 `Case8x` 类型、`case8x/` 私有目录引用和 `presentCase8x` 方法。
- 已接入 `scripts/test_qt512.sh`，违规时在 qmake 前直接失败。

双仪表命令轨迹保持：

```text
4071: *IDN? -> :SYSTem:ERRor?
1466: *IDN? -> :SYSTem:ERRor?
```

仅分析仪兼容分支保持：

```text
4071: *IDN? -> 200 ms delay -> MEAS:VOLT:DC?
```

200 ms 延时仍使用 `QThread::msleep()` 在当前主线程同步执行。该调用不创建工作
线程、不切换线程，也未改变本阶段同步语义。

### 2.3 Driver

`Analyzer4071` 新增：

- `isConnected()`
- `measureBasicVoltage()`

`Generator1466` 新增：

- `isConnected()`

8.1 的全部仪表操作均通过以上 Driver 和阶段 3 已有的 `identify()`、
`readError()` 完成。用例不直接访问 Session。

### 2.4 UI 适配

新增：

- `src/ui/presenters/case81uiadapter.h`
- `src/ui/presenters/case81uiadapter.cpp`

`Case81UiAdapter` 通过值和回调实现最小展示边界，本身不依赖 QWidget。

`MainWindow` 仅负责：

- 注册和启动 8.1 用例。
- 将摘要值写入现有控件。
- 将 `Case81Result` 映射到现有表格、判定和统计控件。
- 将结构化日志转发到现有日志入口。

未修改：

- `src/ui/mainwindow.ui`
- `resources/qss/style.qss`
- 现有控件结构和用户交互入口。

## 3. 完成语义

8.1 返回的两个维度保持独立：

- 执行完成原因：`CompletionReason::Completed`
- 业务判定：`TestVerdict::Pass` 或 `TestVerdict::Fail`

仪表未连接时：

- `canStart()` 返回 false。
- `start()` 的保护分支返回 `ExecutionFailed/NotRun`。

阶段 4 尚未引入停止状态机。`requestStop()` 是可重复调用的幂等入口，在当前同步
样板中不改变执行流程，后续仅使用 8.1 验证工作线程和状态机时再赋予取消语义。

## 4. 自动测试

命令：

```bash
./scripts/test_qt512.sh --clean /tmp/aiot-stage4-review-tests
```

结果：

- 构建目录保护测试通过。
- 测试用例公共接口边界检查通过。
- Qt 5.12.9 单元测试和两个集成探针编译通过。
- 18 passed。
- 0 failed。
- 0 skipped。

新增覆盖：

- 双仪表 8.1 命令顺序、结果和日志。
- 仅分析仪兼容分支命令顺序、延时后电压结果和判定。
- `TestCaseRegistry` 所有权和查找。
- 同一注册用例连续运行 20 次。
- 每轮重复调用两次 `requestStop()`。
- 每轮只产生一次结果展示。
- `ITestCase` 虚析构编译期约束。

## 5. 真实仪表验证

专用纵向探针：

- `tests/integration/case81_probe.cpp`
- `tests/integration/case81_probe.pro`

探针负责连接 Session，但测试执行只调用 `TestCase81::start()`。SCPI 命令由
`TestCase81 -> Analyzer4071/Generator1466 -> InstrumentSession` 发出。

命令：

```bash
timeout 12 /tmp/aiot-stage4-review-tests/integration/case81_probe \
  192.168.110.32 5025 \
  192.168.110.53 5025
```

结果：

- Summary：`8.1 基本功能验证`
- Completion：`Completed`
- Verdict：`PASS`
- 4071E：`Ceyear Technologies,4071E,QZNK000063,1.13.38`
- 1466G-V：`Ceyear Technologies,1466G-V,QZOC000877,1.7.0`
- 4071E Error：`+0, "No error"`
- 1466G-V Error：`+0,"No error"`

只发送：

```text
*IDN?
:SYSTem:ERRor?
```

未发送 RF ON、ARB、频率、功率、扫描或测量配置命令。

## 6. 产品构建与冒烟

clean build：

```bash
./scripts/build_qt512.sh --clean /tmp/aiot-stage4-review-build
```

结果：

- qmake 成功。
- 编译和链接成功。
- 二进制：`/tmp/aiot-stage4-review-build/AIoT_Test`
- SHA-256：`26394aad60f4ab7ed55f1e86638d90eb6af0d601659c06078b8f37f54bfac580`
- 动态链接确认使用 `/opt/Qt5.12.9/5.12.9/gcc_64/lib`。
- 仅存在 Qt 5.12.9 既有 deprecated-copy 编译警告。

offscreen 冒烟：

```bash
QT_QPA_PLATFORM=offscreen \
XDG_RUNTIME_DIR=/tmp/aiot-stage4-review-runtime \
timeout 3s /tmp/aiot-stage4-review-build/AIoT_Test
```

结果：

- 应用成功进入事件循环。
- 退出码 124，由 `timeout` 按预期终止。
- 无应用运行错误输出。

## 7. 行为保持结论

- 8.1 双仪表 SCPI 命令和顺序未变。
- 8.1 仅分析仪兼容分支命令、200 ms 延时和 2000 ms 查询超时未变。
- 表头、行数、字段文本、PASS/FAIL 判定和统计文本保持原映射。
- 8.1 仍由原开始按钮和原用例路由触发。
- 其他用例逻辑未迁移。
- 未修改 UI 文件和样式文件。
- 未引入状态机、工作线程或异步队列。

## 8. 阶段结论

- 8.1 已形成可编译、可测试、可回滚的同步纵向样板。
- 用例执行逻辑不再依赖 `MainWindow`、Session 或 QWidget。
- UI 展示和测试执行边界已建立。
- Fake Transport 自动验证与真实双仪表验证均通过。
- 阶段 5 尚未开始。
