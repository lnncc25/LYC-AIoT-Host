# SCPI Trace Baseline

## 1. 比较规则

SCPI 回归分为三类：

- 必须存在：测试定义要求的关键命令。
- 相对顺序：关键命令之间的先后关系。
- 允许波动：错误队列、状态回读、能力检查和轮询次数可因超时处理变化。

时间戳、日志格式和无业务影响的额外只读状态查询不参与严格匹配。

## 2. 通用连接

- Raw TCP Socket。
- 默认端口：5025。
- 文本命令以换行结束。
- 设备识别：`*IDN?`。
- 错误队列：`:SYSTem:ERRor?`。

## 3. 4071 关键命令

### 频谱配置

必要顺序：

1. `*CLS`
2. `:CONFigure:SANalyzer`
3. `:SENSe:FREQuency:CENTer ...MHz`
4. `:SENSe:FREQuency:SPAN ...MHz`
5. `:SENSe:BANDwidth:RESolution ...kHz`
6. `:SENSe:BANDwidth:VIDeo ...kHz`
7. `:INITiate:CONTinuous OFF`
8. `:INITiate:IMMediate`
9. `*OPC?`

### Marker 读取

必要顺序：

1. `:CALCulate:MARKer1:MAXimum`
2. `:CALCulate:MARKer1:X?`
3. `:CALCulate:MARKer1:Y?`

### ACPower

关键命令：

- `:CONFigure:ACPower`
- `:SENSe:FREQuency:CENTer ...MHz`
- `:SENSe:ACPower:FREQuency:SPAN ...MHz`
- `:SENSe:ACPower:CARRier:COUNt 1`
- `:SENSe:ACPower:MODE NORMal`
- `:SENSe:ACPower:TYPE TPRef`
- `:SENSe:ACPower:OFFSet:LIST:FREQuency ...Hz`
- `:SENSe:ACPower:OFFSet:LIST:BANDwidth:SHAPe FLATtop`
- `:SENSe:ACPower:METHod IBW`
- `:READ:ACPower?`

具体带宽、偏移和积分带宽必须与 8.5 当前测试点生成逻辑一致。

### 截图

必要顺序：

1. `:MMEMory:STORe:SCReen "<remote-name>"`
2. `:MMEMory:DATA? "<remote-name>"`
3. 下载成功后可执行 `:MMEMory:DELete "<remote-name>"`

### 停止

- `:ABORt`
- `:INITiate:CONTinuous OFF`

## 4. 1466 关键命令

### CW

必要顺序：

1. `*CLS`
2. `:SOURce1:FREQuency ...MHz`
3. `:SOURce1:POWer ...dBm`
4. `:OUTPut1:STATe ON|OFF`

当前配置后回读：

- `:SOURce1:FREQuency?`
- `:SOURce1:POWer?`
- `:OUTPut1:STATe?`
- `:SYSTem:ERRor?`

### ARB

必要顺序：

1. `*CLS`
2. `:SOURce1:RADio:ARB:WAVeform "<file>"`
3. `.bin` 文件设置 `:SOURce1:RADio:ARB:SCLock:RATE ...`
4. `:SOURce1:FREQuency ...MHz`
5. `:SOURce1:POWer ...dBm`
6. `:SOURce1:RADio:ARB:STATe ON`
7. `:OUTPut1:STATe ON`

### 安全关闭

必须尽力发送：

1. `ABORt`
2. `:OUTPut:ALL OFF`
3. `:OUTPut1:STATe OFF`

正常连接时必须使用 `:OUTPut1:STATe?` 回读确认。

## 5. 用例关键约束

### 8.1

- 两台设备分别执行 `*IDN?`。
- 两台设备分别读取错误队列。
- 不发送 RF ON。

### 8.2

- 每个功率点先配置 1466，再配置 4071。
- 每点读取 10 次 Marker。
- 测试结束停止 4071 并关闭 1466。

### 8.5

- 先加载 1466 ARB 波形并输出。
- 再配置 4071 ACPower。
- 执行测量、解析、截图和结果记录。
- 所有点结束或停止后关闭 RF。

### 8.6

- 波形文件、采样率、频率和功率按当前测试计划配置。
- 4071 执行频谱与峰值/频率读取。
- 截图和 BLER 人工输入保持当前关联。
- 仪表阶段结束后关闭 RF。

### 8.7

- 逐测试点选择 upper/lower 候选波形。
- 使用 30.72 MHz ARB 时钟。
- 保存并下载 4071 截图。
- 等待 BLER 输入前完成对应测试点仪表配置。
- 流程结束或停止后关闭 RF。

## 6. L1 实测基线

2026-06-23 实测：

| IP | `*IDN?` 返回 | 设备身份 |
| --- | --- | --- |
| 192.168.110.32 | `Ceyear Technologies,4071E,QZNK000063,1.13.38` | 4071E |
| 192.168.110.53 | `Ceyear Technologies,1466G-V,QZOC000877,1.7.0` | 1466G-V |

该映射与实施前口头提供的 IP 对应关系相反。后续连接配置必须以用户确认结果为准，不允许在结构重构中静默交换地址。

错误队列：

- `192.168.110.32`：`+0, "No error"`。
- `192.168.110.53` 首次：`-410,"Query INTERRUPTED;<Err>"`。
- `192.168.110.53` 复查：`+0,"No error"`。

首次 `-410` 发生在短连接 `*IDN?` 探测后，基线保留该事实；应用级长连接验证时应确认是否复现。
