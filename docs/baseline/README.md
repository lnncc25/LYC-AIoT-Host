# Refactor Baseline

阶段 0 基线用于约束后续重构的可观察行为，不定义新功能。

## 基线身份

- 工程：`LYC-AIoT-Host`
- qmake 目标：`AIoT_Test`
- Git 提交：`178cd50f93d55e523e7c775ccc2ae5d04b2f9e93`
- 本地标签：`refactor-baseline`
- 标签说明：`Baseline before architecture refactoring`
- 平台：Ubuntu 20.04
- Qt：5.12.9
- qmake：`/opt/Qt5.12.9/5.12.9/gcc_64/bin/qmake`

## 文件说明

- `structured_baseline.md`：用例字段、测试点、结果结构和固定参数。
- `scpi_trace_baseline.md`：关键 SCPI 命令、顺序约束和允许波动项。
- `ui_interaction_baseline.md`：导航、页面、按钮、占位功能和人工检查项。
- `verification_report.md`：构建、启动、真实仪表 L1 验证及已知差异。

## 使用规则

后续阶段必须：

1. 先与结构化基线比较，再进行 UI 截图人工检查。
2. SCPI 自动测试检查关键命令和相对顺序，不强制错误队列或状态轮询次数完全一致。
3. 业务 FAIL 与执行失败分别记录。
4. 未经独立需求评审，不借结构重构修改已验证测试参数。
5. 每个阶段独立提交；每个用例单独提交。

## 标签推送状态

本地标签已经创建。`git push origin refactor-baseline` 因当前环境没有可用 GitHub 凭据而未完成。认证恢复后应推送同一标签，不得重新指向其他提交。
