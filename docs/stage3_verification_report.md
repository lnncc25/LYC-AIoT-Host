# Stage 3 Synchronous Communication Adapter Report

## 1. 范围

阶段 3 只调整通信代码归属，不改变现有同步执行语义。

本阶段未实施：

- 工作线程。
- 异步请求队列。
- 请求取消。
- 用例拆分。
- SCPI 命令调整。
- 测试参数和判定调整。
- UI 布局、文本和交互调整。

## 2. 分层结果

### 2.1 Transport

文件：

- `src/devices/scpi/iscpitransport.h`
- `src/devices/scpi/tcpscpitransport.h`
- `src/devices/scpi/tcpscpitransport.cpp`

职责：

- TCP 连接与断开。
- 原始字节写入。
- 缓冲区读取。
- 同步等待可读和写入完成。
- 传递连接、可读和 Socket 错误信号。

`QTcpSocket` 只存在于 `TcpScpiTransport`，`MainWindow` 不再包含或调用
`QTcpSocket`。

### 2.2 InstrumentSession

文件：

- `src/devices/scpi/scpitypes.h`
- `src/devices/scpi/instrumentsession.h`
- `src/devices/scpi/instrumentsession.cpp`

稳定值类型：

- `ScpiStatus`
- `ScpiWriteResult`
- `ScpiReply`
- `BinaryBlockReply`

同步行为：

- 文本命令仍自动追加换行。
- 查询前仍清空已有输入缓冲。
- 文本查询仍以首个换行作为完成条件。
- 每次等待仍最多阻塞 100 ms，并在轮询间处理 Qt 事件。
- 文本响应仍进行 CR/LF 归一化并返回首个非空行。
- 二进制查询仍按 IEEE `#<n><length><payload>` 格式读取。
- 默认和调用点显式超时均保持不变。
- 查询期间仍抑制 `MainWindow` 对同一响应的异步重复读取。
- 命令发送成功或失败日志仍在实际写入发生时产生。

### 2.3 Driver

文件：

- `src/devices/analyzer4071.h`
- `src/devices/analyzer4071.cpp`
- `src/devices/generator1466.h`
- `src/devices/generator1466.cpp`

本阶段只建立最小稳定接口：

- 获取对应 `InstrumentSession`。
- `identify()`。
- `readError()`。

频谱配置、功率配置、截图和测试业务仍保留在 `MainWindow`，等待对应后续阶段按
用例迁移，避免本阶段同时改变通信归属和业务归属。

## 3. 同步到异步演进契约

阶段 3 后，上层 Driver 只依赖 `InstrumentSession` 和 SCPI 结果值类型，不依赖
`QTcpSocket`。

后续异步化遵循：

1. 保留 `ScpiStatus`、`ScpiReply`、`BinaryBlockReply` 的状态和错误语义。
2. 保留 Driver 方法和 SCPI 命令语义。
3. 串行队列、请求 ID、取消和优先级只在 Session 内增加。
4. Driver 不维护同步、异步两套命令实现。
5. 若过渡期仍需同步调用，由 Session 在异步队列外提供兼容适配，不向用例暴露
   Socket 或队列细节。
6. 安全清理后续使用独立请求类别和优先级，但不改变 Driver 的安全操作契约。

因此阶段 7 替换 Session 内部调度时，不要求再次修改 Transport 使用方、Driver
接口或用例结果模型。

## 4. Fake Transport

文件：

- `tests/fakes/fakescpitransport.h`
- `tests/fakes/fakescpitransport.cpp`

覆盖：

- 可注入的连接状态。
- 按命令排队返回文本或二进制响应。
- 完整命令和原始写入轨迹。
- 无响应超时。
- 断连拒绝发送。

8.1 命令轨迹验证：

```text
4071: *IDN? -> :SYSTem:ERRor?
1466: *IDN? -> :SYSTem:ERRor?
```

每条命令的实际写入保持尾部单个 `\n`。

## 5. 自动测试

命令：

```bash
./scripts/test_qt512.sh --clean /tmp/aiot-stage3-review-tests
```

结果：

- 构建目录保护测试通过。
- Qt 5.12.9 单元测试编译通过。
- SCPI 真实链路探针编译通过，但不会由 `make check` 自动连接仪表。
- 15 passed。
- 0 failed。
- 0 skipped。

新增测试：

- `scpiSessionCase81Trace`
- `scpiSessionTimeout`
- `scpiSessionDisconnected`
- `scpiSessionBinaryBlock`
- `IScpiTransport` 虚析构编译期约束。

## 6. 真实仪表只读验证

探针：

- `tests/integration/scpi_probe.cpp`
- `tests/integration/integration.pro`

探针只发送：

```text
*IDN?
:SYSTem:ERRor?
```

验证结果：

- `192.168.110.32:5025`
  - IDN：`Ceyear Technologies,4071E,QZNK000063,1.13.38`
  - Error：`+0, "No error"`
- `192.168.110.53:5025`
  - IDN：`Ceyear Technologies,1466G-V,QZOC000877,1.7.0`
  - Error：`+0,"No error"`

未发送 RF ON、ARB、频率、功率、扫描或测量配置命令。

## 7. 行为保持

- `MainWindow` 中全部现有 SCPI 调用点、字符串和排列顺序未调整。
- 连接成功后仍发送一次 `*IDN?`。
- UI 日志文案、来源、等级和发送时机保持原映射。
- 未修改 `mainwindow.ui` 和 QSS。
- 未修改测试参数、测试点和判定逻辑。
- 未引入线程、Future、局部异步队列或并发请求。
- 既有 `readVoltageWithTimeout()` 的局部事件循环未在本阶段改动。

## 8. 产品构建与冒烟

产品 clean build：

```bash
./scripts/build_qt512.sh --clean /tmp/aiot-stage3-review-build
```

结果：

- qmake 成功。
- 编译和链接成功。
- 二进制：`/tmp/aiot-stage3-review-build/AIoT_Test`
- SHA-256：`e9eb38f4997860153dd62b65b07185515e53b4e4a106fad880cc847c777993b3`
- 动态链接确认使用 Qt 5.12.9。

offscreen 冒烟：

```bash
QT_QPA_PLATFORM=offscreen \
XDG_RUNTIME_DIR=/tmp/aiot-stage3-review-runtime \
timeout 3s /tmp/aiot-stage3-review-build/AIoT_Test
```

结果：

- 应用成功进入事件循环。
- 退出码 124，由 `timeout` 按预期终止。
- 无应用运行错误输出。

## 9. 阶段结论

- 同步通信职责已从 `MainWindow` 下沉到可替换 Transport 和 Session。
- 4071/1466 最小 Driver 边界已建立。
- Fake Transport 已证明 Session 可替换。
- 真实双仪表只读查询已通过。
- 阶段 4 尚未开始。
