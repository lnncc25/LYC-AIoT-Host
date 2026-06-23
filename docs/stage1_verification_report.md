# Stage 1 Build and Test Skeleton Report

## 1. 范围

阶段 1 只建立构建与自动测试骨架，不修改测试用例、SCPI 命令、UI、参数、结果表或用户交互行为。

实施内容：

- 固定 Qt 5.12.9 的本地构建脚本。
- 固定 Qt 5.12.9 的自动测试脚本。
- 增加 qmake Qt Test 子工程。
- 增加 Qt 5.12.9 最低版本门禁。
- 提取原 `MainWindow` 内部纯 CSV 单元格转义函数。
- 为 CSV 转义增加首批数据驱动单元测试。

## 2. 文件边界

新增：

- `scripts/build_qt512.sh`
- `scripts/test_qt512.sh`
- `src/core/export/csvutils.h`
- `src/core/export/csvutils.cpp`
- `tests/tests.pro`
- `tests/unit/unit.pro`
- `tests/unit/tst_csvutils.cpp`

修改：

- `AIoT_Test.pro`
- `src/ui/mainwindow.cpp`

未修改：

- `src/ui/mainwindow.ui`
- `src/ui/mainwindow.h`
- `resources/qss/style.qss`
- `src/ui/dialogs/connectiondialog.*`
- 所有测试用例参数和 SCPI 命令。

## 3. 行为兼容说明

原 CSV 转义逻辑：

1. 双引号替换为两个双引号。
2. 包含逗号、双引号、换行或回车时，用双引号包裹整个字段。
3. 其他文本原样返回。

提取后的 `CsvUtils::escapeCell()` 保持相同输入、处理顺序和输出。产品中的测试结果 CSV 与日志 CSV 都改为调用该函数，没有改变导出字段和写入顺序。

## 4. 构建验证

命令：

```bash
./scripts/build_qt512.sh /tmp/aiot-stage1-build-20260623-a
```

结果：

- qmake 成功。
- 编译成功。
- 链接成功。
- 输出：`/tmp/aiot-stage1-build-20260623-a/AIoT_Test`
- 二进制 SHA-256：`c6f41f3a733f9451fdf1d9e8355dbe4ceafa2701f6c0a2e7678b9c1988828e91`
- 动态链接确认使用 Qt 5.12.9 的 Charts、Widgets、Network、SerialPort 和 Core。
- 既有 Qt 头文件 deprecated-copy 警告仍存在，未新增构建失败。

## 5. 自动测试验证

命令：

```bash
./scripts/test_qt512.sh /tmp/aiot-stage1-tests-20260623-a
```

测试环境：

- Qt Test 5.12.9
- Qt 5.12.9
- C++17

结果：

- 8 passed
- 0 failed
- 0 skipped

覆盖：

- 普通文本。
- 包含逗号。
- 包含双引号。
- 包含换行。
- 包含回车。
- 空字符串。

Qt Test 自动增加 `initTestCase()` 和 `cleanupTestCase()` 两项，因此总通过数为 8。

## 6. 最低版本门禁

Qt 5.12.9：

- 产品 qmake 配置成功。
- 测试 qmake 配置成功。

Qt 5.9.7 负向验证：

```text
Project ERROR: AIoT_Test requires Qt 5.12.9 or newer
```

结果符合预期，可阻止 PATH 误选 Anaconda Qt 5.9.7。

## 7. Offscreen 冒烟

命令：

```bash
QT_QPA_PLATFORM=offscreen \
XDG_RUNTIME_DIR=/tmp/aiot-stage1-runtime \
timeout 3s /tmp/aiot-stage1-build-20260623-a/AIoT_Test
```

结果：

- 应用进入事件循环。
- 3 秒后由 `timeout` 终止，退出码 124 属于预期。
- 无应用运行错误输出。

## 8. 阶段结论

- 阶段 1 构建骨架可用。
- Qt Test 骨架可用。
- Qt 版本门禁有效。
- 产品 UI 和业务逻辑未改变。
- 未执行真实仪表 L2/L3 操作。
- 阶段 2 尚未开始。
