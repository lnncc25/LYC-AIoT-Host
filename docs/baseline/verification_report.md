# Stage 0 Verification Report

## 1. 执行时间

- 日期：2026-06-23
- 时区：Asia/Shanghai

## 2. Git 基线

- 提交：`178cd50f93d55e523e7c775ccc2ae5d04b2f9e93`
- 本地标签：`refactor-baseline`
- 标签已正确指向基线提交。
- 推送状态：未完成。
- 原因：当前 HTTPS remote 没有可用 GitHub 凭据。
- 待执行：认证恢复后运行 `git push origin refactor-baseline`。

## 3. 本地配置排除

- `*.pro.user*` 已排除 Qt Creator 用户配置，包括 `AIoT_Test.pro-6.17.user`。
- `.vscode/` 已加入 `.gitignore`。
- 本次阶段提交不得包含上述本地配置。

## 4. 源码规模与哈希

| 文件 | 行数 | SHA-256 |
| --- | ---: | --- |
| `AIoT_Test.pro` | - | `79f97db3f6753b841c1b030423de03f64068c5c53da2371f61af50bf3091b4e0` |
| `src/app/main.cpp` | 14 | `af461729331ca61eebd696ada3bf0cec07a3d345ef0f47ee81bb7ad04447a656` |
| `src/ui/mainwindow.cpp` | 7137 | `85a4ca1e3eee8c520cd1c2cf5f6f95da907e3448e0b1a51190a229687ccbf82f` |
| `src/ui/mainwindow.h` | 238 | `0982a680e9d35272052264d9b327aa75be843c3d67eb3b736306b20642ba92e4` |
| `src/ui/mainwindow.ui` | 1474 | `204f1ffac25c09fdbb5f65709ba4f8b8a1f194dbc59cbf949b12b4aaa2bd86d0` |
| `src/ui/dialogs/connectiondialog.cpp` | 221 | `f8b9a6032e3460e019c3fda152bb03658bde02958dab6ae1e5d871eac178d226` |
| `src/ui/dialogs/connectiondialog.h` | 63 | `3eeb9d61422252593a437c4bc794f2322f972da85c4b320db00c8ced5e1ed6c0` |
| `resources/qss/style.qss` | 293 | `82a607733e11b5c8333f6ca6ef0eb70e375091fbd7f8a370830a11b5e2425c75` |
| `resources/qrc/resources.qrc` | - | `ec18af711bd83bc7c1ef3940bae756fbb757683096bc07c99d2f10d5b9baa2ee` |

## 5. Qt 5.12.9 Clean Build

构建目录：`/tmp/aiot-stage0-build-20260623`

命令：

```bash
/opt/Qt5.12.9/5.12.9/gcc_64/bin/qmake \
  /home/hotwater/Documents/AIoT-Host-6.23/LYC-AIoT-Host/AIoT_Test.pro
make -j2
```

结果：

- qmake：成功。
- 编译：成功。
- 链接：成功。
- 目标文件：`AIoT_Test`。
- 二进制 SHA-256：`e1d522bcb28879226b406148a756182c237e50d18af52df53d6711a8ee74dca3`。
- 动态链接确认使用 `/opt/Qt5.12.9/5.12.9/gcc_64/lib` 中的 Qt Charts、Widgets、Network、SerialPort 和 Core。
- 编译存在大量来自 Qt 5.12.9 头文件与当前编译器组合的 deprecated-copy 警告，未阻断构建。

## 6. Offscreen 启动

命令：

```bash
QT_QPA_PLATFORM=offscreen \
XDG_RUNTIME_DIR=/tmp/aiot-stage0-runtime \
timeout 3s ./AIoT_Test
```

结果：

- 应用成功进入 Qt 事件循环。
- 3 秒后由 `timeout` 终止，退出码 124 属于预期。
- 标准输出和标准错误无应用运行错误。

## 7. L1 真实仪表验证

只执行：

- TCP 5025 连接。
- `*IDN?`。
- `:SYSTem:ERRor?`。

未执行：

- RF ON。
- 频率、功率、ARB 或测量配置。
- 截图、文件写入或删除。

### 结果

| IP | TCP 5025 | IDN | 最终错误队列 |
| --- | --- | --- | --- |
| 192.168.110.32 | 成功 | `Ceyear Technologies,4071E,QZNK000063,1.13.38` | `+0, "No error"` |
| 192.168.110.53 | 成功 | `Ceyear Technologies,1466G-V,QZOC000877,1.7.0` | `+0,"No error"` |

### 重要差异

实测映射为：

- 4071E：`192.168.110.32`
- 1466G-V：`192.168.110.53`

该映射与实施前提供的对应关系相反。阶段 0 不修改应用配置，进入涉及真实连接的后续阶段前必须由用户确认最终映射。

`192.168.110.53` 首次错误队列读取返回 `-410 Query INTERRUPTED`，再次读取后为 `No error`。推测与短连接查询关闭时序有关，后续使用应用长连接复核。

## 8. 阶段 0 结论

- Qt 5.12.9 可重复构建基线成立。
- Offscreen 启动基线成立。
- 两台仪表 TCP 和身份只读验证成立。
- 结构化、SCPI 和 UI 交互基线已记录。
- 未修改产品源码、UI 或构建配置。
- 阶段 1 尚未开始。

## 9. 待确认

1. 是否确认后续以实测 IP 映射为准：
   - 4071：`192.168.110.32`
   - 1466：`192.168.110.53`
2. GitHub 认证恢复后是否立即推送 `refactor-baseline` 标签和阶段 0 commit。
