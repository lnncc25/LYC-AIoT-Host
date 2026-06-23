#include "connectiondialog.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QTimer>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSettings>          // 新增：用于保存配置

ConnectionDialog::ConnectionDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("连接管理");
    createUI();
    resize(980, 680);
    setMinimumSize(820, 560);
}

ConnectionDialog::~ConnectionDialog() {}

void ConnectionDialog::accept()
{
    // 保存仪表 IP 和端口到注册表/配置文件
    QSettings settings("AIoT_Test", "ConnectionManager");
    settings.setValue("Analyzer/IP", instrumentIp->text());
    settings.setValue("Analyzer/Port", ctrlPort->value());
    settings.setValue("Generator/IP", cwIp->text());
    settings.setValue("Generator/Port", cwPort->value());
    settings.setValue("Instrument/IP", instrumentIp->text());
    settings.setValue("Instrument/Port", ctrlPort->value());
    QDialog::accept();
}

void ConnectionDialog::createUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    QLabel *titleLabel = new QLabel("设备连接与通信配置");
    titleLabel->setObjectName("dialogTitleLabel");
    QLabel *subtitleLabel = new QLabel("配置 4071 频谱分析仪、1466 信号发生器、AIoT 基站和标签模拟器，并在测试前检测各设备通信状态。");
    subtitleLabel->setObjectName("dialogSubtitleLabel");
    subtitleLabel->setWordWrap(true);
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(subtitleLabel);

    // ---------- 读取上次保存的仪表配置 ----------
    QSettings settings("AIoT_Test", "ConnectionManager");
    QString lastAnalyzerIp = settings.value("Analyzer/IP", settings.value("Instrument/IP", "192.168.152.1").toString()).toString();
    int lastAnalyzerPort = settings.value("Analyzer/Port", settings.value("Instrument/Port", 5025).toInt()).toInt();
    QString lastGeneratorIp = settings.value("Generator/IP", "192.168.152.2").toString();
    int lastGeneratorPort = settings.value("Generator/Port", 5025).toInt();

    // 4071 频谱分析仪组
    QGroupBox *instGroup = new QGroupBox("● 4071 频谱分析仪");
    instGroup->setObjectName("connectionCard");
    QFormLayout *instForm = new QFormLayout(instGroup);
    instForm->setLabelAlignment(Qt::AlignRight);
    deviceTypeCombo = new QComboBox;
    deviceTypeCombo->addItems({"4071 频谱分析仪", "SCPI 频谱分析仪"});
    instForm->addRow("设备类型:", deviceTypeCombo);
    instrumentIp = new QLineEdit(lastAnalyzerIp);
    instForm->addRow("IP 地址:", instrumentIp);
    ctrlPort = new QSpinBox;
    ctrlPort->setRange(1024, 65535);
    ctrlPort->setValue(lastAnalyzerPort);
    instForm->addRow("控制端口:", ctrlPort);
    resultPort = new QSpinBox;
    resultPort->setRange(1024, 65535);
    resultPort->setValue(8899);
    instForm->addRow("结果端口:", resultPort);
    protocolCombo = new QComboBox;
    protocolCombo->addItems({"SCPI", "JSON-RPC"});
    instForm->addRow("控制协议:", protocolCombo);
    QLabel *instStatus = new QLabel("返回状态: 待检测 (*IDN?)");
    instStatus->setObjectName("cardStatusLabel");
    instForm->addRow("", instStatus);

    // AIoT 基站组
    QGroupBox *bsGroup = new QGroupBox("● AIoT 基站");
    bsGroup->setObjectName("connectionCard");
    QFormLayout *bsForm = new QFormLayout(bsGroup);
    bsForm->setLabelAlignment(Qt::AlignRight);
    QComboBox *bsInterfaceCombo = new QComboBox;
    bsInterfaceCombo->addItems({"Ethernet (TCP)", "Serial", "SCPI"});
    bsForm->addRow("控制接口:", bsInterfaceCombo);
    bsIp = new QLineEdit("192.168.10.50");
    bsForm->addRow("IP 地址:", bsIp);
    bsPort = new QSpinBox;
    bsPort->setRange(1024, 65535);
    bsPort->setValue(5025);
    bsForm->addRow("控制端口:", bsPort);
    bandCombo = new QComboBox;
    bandCombo->addItems({"n8 (880-960 MHz)", "n5 (824-849 MHz)"});
    bsForm->addRow("工作频段:", bandCombo);
    bsPower = new QDoubleSpinBox;
    bsPower->setRange(-50, 30);
    bsPower->setValue(20);
    bsPower->setSuffix(" dBm");
    bsForm->addRow("额定功率:", bsPower);
    QLabel *bsStatus = new QLabel("返回状态: 待检测");
    bsStatus->setObjectName("cardStatusLabel");
    bsForm->addRow("", bsStatus);

    // 1466 信号发生器组
    QGroupBox *cwGroup = new QGroupBox("● 1466 信号发生器");
    cwGroup->setObjectName("connectionCard");
    QFormLayout *cwForm = new QFormLayout(cwGroup);
    cwForm->setLabelAlignment(Qt::AlignRight);
    cwEnable = new QCheckBox("启用 RF 输出");
    cwForm->addRow("", cwEnable);
    cwIp = new QLineEdit(lastGeneratorIp);
    cwForm->addRow("IP 地址:", cwIp);
    cwPort = new QSpinBox;
    cwPort->setRange(1024, 65535);
    cwPort->setValue(lastGeneratorPort);
    cwForm->addRow("控制端口:", cwPort);
    cwFreq = new QDoubleSpinBox;
    cwFreq->setRange(0, 6000);
    cwFreq->setValue(925.0);
    cwFreq->setSuffix(" MHz");
    cwForm->addRow("输出频率:", cwFreq);
    cwPower = new QDoubleSpinBox;
    cwPower->setRange(-100, 20);
    cwPower->setValue(-30.0);
    cwPower->setSuffix(" dBm");
    cwForm->addRow("输出功率:", cwPower);
    QLabel *cwStatus = new QLabel("返回状态: 待检测 (*IDN?)");
    cwStatus->setObjectName("cardStatusLabel");
    cwForm->addRow("", cwStatus);

    // 标签模拟器组
    QGroupBox *tagGroup = new QGroupBox("● 标签模拟器 / UART");
    tagGroup->setObjectName("connectionCard");
    QFormLayout *tagForm = new QFormLayout(tagGroup);
    tagForm->setLabelAlignment(Qt::AlignRight);
    serialPortCombo = new QComboBox;
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        serialPortCombo->addItem(info.portName());
    }
    if (serialPortCombo->count() == 0) {
        serialPortCombo->addItem("tty.usbserial-AIOT");
    }
    tagForm->addRow("串口号:", serialPortCombo);
    baudRateCombo = new QComboBox;
    baudRateCombo->addItems({"9600", "19200", "38400", "115200"});
    baudRateCombo->setCurrentText("115200");
    tagForm->addRow("波特率:", baudRateCombo);
    tagCountSpin = new QSpinBox;
    tagCountSpin->setRange(1, 16);
    tagCountSpin->setValue(8);
    tagForm->addRow("标签数量:", tagCountSpin);
    modulationCombo = new QComboBox;
    modulationCombo->addItems({"OOK", "BPSK"});
    tagForm->addRow("调制方式:", modulationCombo);
    kFactors = new QLineEdit("1,2,4,8");
    tagForm->addRow("小移频 k:", kFactors);
    QLabel *tagStatus = new QLabel("返回状态: 待检测");
    tagStatus->setObjectName("cardStatusLabel");
    tagForm->addRow("", tagStatus);

    QWidget *gridWidget = new QWidget(this);
    QGridLayout *gridLayout = new QGridLayout(gridWidget);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(12);
    gridLayout->addWidget(instGroup, 0, 0);
    gridLayout->addWidget(bsGroup, 0, 1);
    gridLayout->addWidget(cwGroup, 1, 0);
    gridLayout->addWidget(tagGroup, 1, 1);
    gridLayout->setColumnStretch(0, 1);
    gridLayout->setColumnStretch(1, 1);
    mainLayout->addWidget(gridWidget, 1);

    statusLabel = new QLabel("总体状态: 待检测。建议先检测全部连接，再保存预设。");
    statusLabel->setObjectName("connectionStatusLabel");
    mainLayout->addWidget(statusLabel);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    QPushButton *testBtn = new QPushButton("检测全部连接");
    QPushButton *savePresetBtn = new QPushButton("保存预设");
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    btnLayout->addWidget(testBtn);
    btnLayout->addWidget(savePresetBtn);
    btnLayout->addWidget(buttonBox);
    mainLayout->addLayout(btnLayout);

    connect(testBtn, &QPushButton::clicked, this, &ConnectionDialog::onTestConnection);
    connect(savePresetBtn, &QPushButton::clicked, this, &ConnectionDialog::onSavePreset);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setStyleSheet(
        "QDialog { background: #eef3f8; }"
        "QLabel#dialogTitleLabel { color: #123b63; font-size: 17pt; font-weight: 800; }"
        "QLabel#dialogSubtitleLabel { color: #53677e; }"
        "QGroupBox#connectionCard { background: #ffffff; border: 1px solid #c9d6e2; border-radius: 6px; margin-top: 18px; padding: 14px 12px 12px 12px; font-weight: 700; color: #0e4d86; }"
        "QGroupBox#connectionCard::title { subcontrol-origin: margin; left: 12px; top: 1px; padding: 0 8px; background: #ffffff; }"
        "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: #ffffff; border: 1px solid #bdcad8; border-radius: 4px; min-height: 28px; padding: 4px 8px; }"
        "QPushButton { background: #f7fbff; border: 1px solid #b9c9d9; border-radius: 4px; padding: 7px 14px; min-width: 84px; }"
        "QPushButton:hover { background: #eaf4ff; border-color: #74a6d8; }"
        "QLabel#cardStatusLabel { color: #12643d; background: #e6f6ef; border: 1px solid #a7dbc2; border-radius: 4px; padding: 6px 8px; }"
        "QLabel#connectionStatusLabel { color: #17324f; background: #ffffff; border: 1px solid #c9d6e2; border-radius: 5px; padding: 9px 12px; }"
    );
}

void ConnectionDialog::onTestConnection()
{
    statusLabel->setText("正在检测连接...");
    QTimer::singleShot(500, [this](){
        statusLabel->setText("检测项: 4071/1466 使用 LAN Socket + SCPI，默认端口 5025；实际在线状态以主界面日志中的 *IDN? 返回为准。");
    });
}

void ConnectionDialog::onSavePreset()
{
    QMessageBox::information(this, "保存预设", "预设已保存（模拟）");
}
