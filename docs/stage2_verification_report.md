# Stage 2 Pure Models and Common Utilities Report

## 1. 范围

阶段 2 只增加纯值类型与公共路径工具，不修改现有 UI、SCPI、测试参数、用例路由或业务行为。

实施内容：

- 增加测试状态、完成原因、业务判定和测试用例 ID 值类型。
- 增加 `TestCompletion` 与 `LogEntry` 值对象。
- 为跨线程演进预先声明 Qt MetaType。
- 提取输出根目录、输出文件、截图目录、时间戳和文件名清洗工具。
- 将 `MainWindow` 中同实现的路径函数替换为公共工具调用。
- 扩展 Qt Test。
- 为构建和测试脚本增加 clean build 保证。

## 2. 新增纯模型

文件：

- `src/core/test/testtypes.h`
- `src/core/test/testtypes.cpp`
- `src/core/logging/logentry.h`

类型：

- `TestCaseId`
- `TestState`
- `CompletionReason`
- `TestVerdict`
- `TestCompletion`
- `LogEntry`

约束：

- 当前 `MainWindow` 仍使用原有 `currentTestCase` 字符串和分支。
- 当前用例函数、状态变量和判定流程未切换到新模型。
- 新模型只建立后续接口边界，不代表状态机已经启用。
- `CompletionReason::Completed` 与 `TestVerdict::Fail` 可同时存在，明确区分流程完成和业务不合格。

## 3. 输出路径工具

文件：

- `src/core/paths/outputpaths.h`
- `src/core/paths/outputpaths.cpp`

提取函数：

- `rootDirectory()`
- `path()`
- `screenshotRunDirectory()`
- `batchTimestamp()`
- `fileTimestamp()`
- `timestampedScreenshotName()`
- `sanitizeFileToken()`

行为兼容：

- `APP_OUTPUT_ROOT` 优先级不变。
- 无配置时回退到当前工作目录。
- 截图根目录仍为 `screenshots`。
- 批次时间格式仍为 `yyyyMMdd_HHmmss`。
- 文件时间格式仍为 `HHmmss`。
- 截图文件名仍为 `<base>_<HHmmss>.png`。
- 空格、逗号、加号、减号和斜杠替换规则不变。

## 4. Clean Build 脚本

脚本：

- `scripts/build_dir_guard.sh`
- `scripts/build_qt512.sh`
- `scripts/test_qt512.sh`
- `tests/scripts/test_build_directory_guard.sh`

新规则：

1. 工程根目录和目标构建目录先通过 `realpath -m` 规范化。
2. 拒绝工程根目录、工程内部任意目录以及工程根目录的所有上级目录。
3. 构建目录首次由脚本创建时写入专用标记 `.aiot-build-directory`。
4. 标记绑定规范化工程根目录和构建类型 `product` 或 `tests`。
5. 已存在但无标记的目录一律拒绝使用或清理，即使目录为空。
6. 已存在的合法标记目录在未指定 `--clean` 时退出码为 2，避免复用旧构建。
7. 只有显式 `--clean` 且标记内容完全匹配时才允许删除。
8. 标记属于其他工程、构建类型不匹配或内容被篡改时均拒绝删除。
9. 合法清理后重新创建构建目录和标记，再执行 qmake、编译及测试。
10. `test_qt512.sh` 在构建前自动运行构建目录保护测试。

验证结果：

- 工程根目录、`src`、`.git`、工程根目录上级目录和 `/`：均在删除前按预期拒绝。
- `build_qt512.sh --clean src`、`--clean .git`、`--clean ..`：脚本入口均按预期拒绝。
- 无标记目录：按预期拒绝，目录内哨兵文件保持存在。
- 伪造其他工程标记：按预期拒绝，目录内哨兵文件保持存在。
- `product` 与 `tests` 标记类型不匹配：按预期拒绝，目录内哨兵文件保持存在。
- 合法标记目录：`--clean` 成功删除旧内容、重建目录并重新写入标记。
- 产品和测试构建目录均通过首次创建及后续显式 clean build 验证。

## 5. Qt Test

测试目标由 `tst_csvutils` 扩展为 `tst_coreutils`。

覆盖：

- CSV 普通文本。
- CSV 逗号。
- CSV 双引号。
- CSV 换行。
- CSV 回车。
- CSV 空字符串。
- 固定时间的批次时间戳。
- 固定时间的文件时间戳。
- 截图文件名。
- 文件名字符清洗。
- 截图运行目录。
- 用例 ID 文本。
- 状态、完成原因和判定文本。
- `TestCompletion` 值相等。
- `LogEntry` 值相等。

结果：

- 11 passed
- 0 failed
- 0 skipped

## 6. 产品构建

命令：

```bash
./scripts/build_qt512.sh --clean /tmp/aiot-stage2-review-build
```

结果：

- qmake 成功。
- 编译成功。
- 链接成功。
- 二进制：`/tmp/aiot-stage2-review-build/AIoT_Test`
- SHA-256：`7c06709c49497fa742a3a94bf975386789f38afc047663715ad52455c300ddfd`
- 动态链接确认使用 Qt 5.12.9。
- 既有 Qt deprecated-copy 警告仍存在，未阻断构建。

## 7. Offscreen 冒烟

命令：

```bash
QT_QPA_PLATFORM=offscreen \
XDG_RUNTIME_DIR=/tmp/aiot-stage2-review-runtime \
timeout 3s /tmp/aiot-stage2-review-build/AIoT_Test
```

结果：

- 应用成功进入事件循环。
- 3 秒后由 `timeout` 终止，退出码 124 属于预期。
- 无应用运行错误输出。

## 8. 未改变项

- `src/ui/mainwindow.ui`
- `resources/qss/style.qss`
- 所有 UI 文本、布局和字体。
- 测试用例参数、测试点和判定。
- SCPI 命令与执行顺序。
- 连接对话框。
- 8.1 至 8.11 路由。
- 真实仪表模式与模拟模式选择。

## 9. Git 状态

- 阶段 1 提交：`fc10ff6`
- 自动推送尝试因当前非交互进程没有 GitHub 凭据失败。
- 本地 remote-tracking 状态显示 `origin/main` 已在 `fc10ff6`。
- 阶段 2 完成后创建独立提交，不在本阶段自动进入阶段 3。

## 10. 阶段结论

- 纯模型边界已建立。
- 输出路径工具已提取并保持兼容。
- clean build 仅允许清理由脚本创建且标记完全匹配的外部构建目录。
- 自动测试扩展成功。
- 产品构建和 offscreen 冒烟通过。
- 阶段 3 尚未开始。
