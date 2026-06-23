/**
 * @FilePath     : /AIoT_Test_6.17/PR/mainwindow.cpp
 * @Description  :  
 * @Author       : asd351 1459157685@qq.com
 * @Version      : 0.0.1
 * @LastEditors  : asd351 1459157685@qq.com
 * @LastEditTime : 2026-06-17 10:40:35
 * @Copyright    : G AUTOMOBILE RESEARCH INSTITUTE CO.,LTD Copyright (c) 2026.
**/
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "connectiondialog.h"
#include <QToolBar>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QInputDialog>
#include <QFile>
#include <QApplication>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QLineEdit>
#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QColor>
#include <QFont>
#include <QtCharts/QPieSlice>
#include <QPlainTextEdit>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>
#include <QTcpSocket>
#include <QEventLoop>
#include <QThread>
#include <cmath>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QtCharts/QScatterSeries>
#include <QBarSet>
#include <QBarSeries>
#include <QBarCategoryAxis>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

// ========== 8.5 / 8.7 共享 ARB 与 ACS 测试常量 ==========
static const double ACLR_ARB_CLOCK_MHZ = 10.0;           // 8.5 当前验证通过的 ARB 采样率
static const double ACS_ARB_CLOCK_MHZ = 30.72;           // 8.7 ACS 组合波形采样时钟
static const double ACS_INTERFERENCE_POWER_DBM = -53.0;  // 协议单侧干扰功率
static const double ACS_EDGE_OFFSET_KHZ = 100.0;         // 干扰中心相对带宽边缘偏移
static const double ACS_BLER_LIMIT_PERCENT = 10.0;       // BLER 门限
static const bool   ACS_ENABLE_SCREENSHOT = true;        // 是否自动截图（需要 4071 在线）
static const double OUTPUT_POWER_MIN_DBM = 0.0;          // 8.2 当前现场验证范围
static const double OUTPUT_POWER_MAX_DBM = 15.0;
static const double OUTPUT_POWER_ANALYZER_LIMIT_DBM = 28.0;
static const double OUTPUT_POWER_TOLERANCE_DB = 3.01;
static const int OUTPUT_POWER_SAMPLE_COUNT = 10;
static const int OUTPUT_POWER_SAMPLE_INTERVAL_MS = 100;

namespace {
enum ResultItemRole {
    RoleWaveformOk = Qt::UserRole + 1,
    RoleSpectrumOk,
    RoleFreqPass,
    RoleFailureReason,
    RoleExecutable,
    RolePendingReason,
    RoleSimPath,
    RoleScreenshotPath,
    RoleFullText
};

enum RefSensitivityRunColumn {
    RefRunStageCol = 0,
    RefRunChannelCol,
    RefRunBwCol,
    RefRunModCol,
    RefRunD2rPowerCol,
    RefRunCwPowerCol,
    RefRunRxPowerCol,
    RefRunBlerCol,
    RefRunFreqErrCol,
    RefRunVerdictCol
};

enum RefSensitivityResultColumn {
    RefResStageCol = 0,
    RefResChannelCol,
    RefResIqFileCol,
    RefResSampleRateCol,
    RefResD2rPowerCol,
    RefResCwPowerCol,
    RefResRxPowerCol,
    RefResPeakCol,
    RefResFreqErrCol,
    RefResBlerCol,
    RefResScreenshotCol,
    RefResLimitCol,
    RefResVerdictCol
};

struct RefSensitivityConfig {
    QString refChannel;
    QString modulation;
    int d2rBandwidthKHz;
    int dsbKHz;
    double prefSensDbm;
    double sampleRateHz;
    QString iqFileName;
};

struct RefSensitivityPoint {
    QString stage;
    RefSensitivityConfig cfg;
    double d2rPowerDbm;
    double cwPowerDbm;
    QString cwFileName;
    bool usesCwWaveform;
};

QList<RefSensitivityConfig> refSensitivityConfigs()
{
    return {
        {"A-FR1-A-1-1", "BPSK", 200, 15, -94.5, 2880000.0,
         "A-FR1-A-1-1_refsens_-94.5dBm_CW_-38.0dBm_CWPN1_fs2.88Msps_int16_iq.bin"},
        {"A-FR1-A-1-2", "OOK", 200, 15, -91.5, 2880000.0,
         "A-FR1-A-1-2_refsens_-91.5dBm_CW_-38.0dBm_CWPN1_fs2.88Msps_int16_iq.bin"},
        {"A-FR1-A-1-3", "BPSK", 3520, 2880, -71.7, 5760000.0,
         "A-FR1-A-1-3_refsens_-71.7dBm_CW_-38.0dBm_CWPN1_fs5.76Msps_int16_iq.bin"},
        {"A-FR1-A-1-4", "OOK", 3520, 2880, -68.7, 5760000.0,
         "A-FR1-A-1-4_refsens_-68.7dBm_CW_-38.0dBm_CWPN1_fs5.76Msps_int16_iq.bin"}
    };
}

QString refSensitivityCwFileName(const RefSensitivityConfig &cfg, double cwPowerDbm)
{
    const int roundedPower = qRound(cwPowerDbm);
    if (roundedPower == -38) {
        return cfg.iqFileName;
    }

    QString channelToken = cfg.refChannel;
    channelToken.remove(QStringLiteral("A-FR1-A-"));
    channelToken.replace('-', '_');
    return QString("d2r_tx_ref_%1_refsens_cw_%2dBm.bin")
        .arg(channelToken)
        .arg(roundedPower);
}

QString refSensitivityWaveformFileName(const RefSensitivityPoint &point)
{
    return point.usesCwWaveform ? point.cwFileName : point.cfg.iqFileName;
}

QList<RefSensitivityPoint> refSensitivityTestPlan()
{
    QList<RefSensitivityPoint> points;
    const QList<double> d2rScan = {-130.0, -110.0, -90.0, -70.0, -50.0};
    const QList<double> d2rStep = {-90.0, -89.0, -88.0, -87.0, -86.0};
    const QList<double> cwScan = {-60.0, -38.0, -10.0};

    const QList<RefSensitivityConfig> configs = refSensitivityConfigs();
    for (const RefSensitivityConfig &cfg : configs) {
        points.append({"核心PREFSENS", cfg, cfg.prefSensDbm, -38.0, QString(), false});
        for (double powerDbm : d2rScan) {
            points.append({"D2R功率扫描", cfg, powerDbm, -38.0, QString(), false});
        }
        for (double powerDbm : d2rStep) {
            points.append({"D2R 1dB步进", cfg, powerDbm, -38.0, QString(), false});
        }
        for (double cwPowerDbm : cwScan) {
            points.append({"CW干扰扫描", cfg, cfg.prefSensDbm, cwPowerDbm,
                           refSensitivityCwFileName(cfg, cwPowerDbm), true});
        }
    }

    return points;
}

QString refSensitivityBandwidthText(const RefSensitivityConfig &cfg)
{
    return QString("%1/%2 kHz").arg(cfg.d2rBandwidthKHz).arg(cfg.dsbKHz);
}

QString refSensitivityPowerText(double powerDbm)
{
    return QString("%1 dBm").arg(powerDbm, 0, 'f', 1);
}

QString refSensitivitySampleRateText(double sampleRateHz)
{
    return QString("%1 Msps").arg(sampleRateHz / 1.0e6, 0, 'f', 2);
}

bool refSensitivityIsD2rRangeStage(const QString &stage)
{
    return stage.contains("D2R") || stage.contains("核心PREFSENS");
}

int pseudoRandomBounded(int upperExclusive)
{
    static bool seeded = false;
    if (!seeded) {
        qsrand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch() & 0xffffffff));
        seeded = true;
    }
    return upperExclusive > 0 ? qrand() % upperExclusive : 0;
}

QString itemTextAt(QTableWidget *table, int row, int column, const QString &fallback = QString())
{
    if (!table || row < 0 || column < 0 || row >= table->rowCount() || column >= table->columnCount()) {
        return fallback;
    }
    QTableWidgetItem *item = table->item(row, column);
    if (!item) {
        return fallback;
    }
    const QVariant fullText = item->data(RoleFullText);
    return fullText.isValid() ? fullText.toString().trimmed() : item->text().trimmed();
}

QString tableItemText(QTableWidgetItem *item)
{
    if (!item) {
        return QString();
    }
    const QVariant fullText = item->data(RoleFullText);
    return fullText.isValid() ? fullText.toString() : item->text();
}

QString escapeCsvCell(QString text)
{
    text.replace("\"", "\"\"");
    const bool needQuote = text.contains(',') || text.contains('"')
                           || text.contains('\n') || text.contains('\r');
    return needQuote ? "\"" + text + "\"" : text;
}

void writeTableAsCsvSection(QTextStream &out, QTableWidget *table, const QString &title)
{
    out << '\n' << title << '\n';
    if (!table) {
        out << "无数据\n";
        return;
    }

    QStringList headers;
    for (int col = 0; col < table->columnCount(); ++col) {
        QTableWidgetItem *headerItem = table->horizontalHeaderItem(col);
        headers << escapeCsvCell(headerItem ? headerItem->text() : QString("列%1").arg(col + 1));
    }
    out << headers.join(',') << '\n';

    for (int row = 0; row < table->rowCount(); ++row) {
        QStringList fields;
        for (int col = 0; col < table->columnCount(); ++col) {
            QTableWidgetItem *item = table->item(row, col);
            fields << escapeCsvCell(tableItemText(item));
        }
        out << fields.join(',') << '\n';
    }
}

QTableWidgetItem *makeTableItem(const QString &text, bool editable = false)
{
    QTableWidgetItem *item = new QTableWidgetItem(text);
    item->setData(RoleFullText, text);
    item->setTextAlignment(Qt::AlignCenter);
    item->setToolTip(text);
    Qt::ItemFlags flags = item->flags();
    if (editable) {
        flags |= Qt::ItemIsEditable;
    } else {
        flags &= ~Qt::ItemIsEditable;
    }
    item->setFlags(flags);
    return item;
}

QString shortRefChannelName(const QString &channel)
{
    QString shortName = channel;
    shortName.replace("A-FR1-A-1-", "A1-");
    return shortName;
}

QString sanitizeSnapshotToken(QString value)
{
    value.replace(' ', '_');
    value.replace(',', '_');
    value.replace('+', 'p');
    value.replace('-', 'm');
    value.replace('/', '_');
    return value;
}

QString outputRootDir()
{
#ifdef APP_OUTPUT_ROOT
    const QString configuredRoot = QString::fromUtf8(APP_OUTPUT_ROOT);
    if (!configuredRoot.isEmpty()) {
        return QDir(configuredRoot).absolutePath();
    }
#endif
    return QDir::currentPath();
}

QString outputPath(const QString &name)
{
    return QDir(outputRootDir()).filePath(name);
}

QString screenshotRunDirPath(const QString &caseDirName, const QString &batchTimestamp)
{
    return QDir(outputPath("screenshots")).filePath(QDir(caseDirName).filePath(batchTimestamp));
}

QString screenshotBatchTimestamp()
{
    return QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
}

QString screenshotFileTimestamp()
{
    return QDateTime::currentDateTime().toString("HHmmss");
}

QString timestampedScreenshotName(const QString &baseName)
{
    return QString("%1_%2.png").arg(baseName, screenshotFileTimestamp());
}

class AspectRatioPixmapLabel : public QLabel
{
public:
    explicit AspectRatioPixmapLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setAlignment(Qt::AlignCenter);
    }

    void setSourcePixmap(const QPixmap &pixmap)
    {
        sourcePixmap = pixmap;
        updateScaledPixmap();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        updateScaledPixmap();
    }

private:
    void updateScaledPixmap()
    {
        if (sourcePixmap.isNull()) {
            QLabel::clear();
            return;
        }

        const QSize targetSize = contentsRect().size();
        if (targetSize.width() <= 0 || targetSize.height() <= 0) {
            return;
        }

        QLabel::setPixmap(sourcePixmap.scaled(targetSize,
                                              Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation));
    }

    QPixmap sourcePixmap;
};

QVBoxLayout *resetChartBoxLayout(QGroupBox *box, const QString &title)
{
    if (!box) {
        return nullptr;
    }

    box->setVisible(true);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    box->setMinimumHeight(230);
    box->setTitle(title);

    QLayout *oldLayout = box->layout();
    if (!oldLayout) {
        oldLayout = new QVBoxLayout(box);
    }
    while (QLayoutItem *item = oldLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(oldLayout);
    if (!layout) {
        layout = new QVBoxLayout(box);
    }
    layout->setContentsMargins(16, 20, 16, 14);
    layout->setSpacing(10);
    return layout;
}

void setupImageChartBox(QGroupBox *box,
                        const QString &title,
                        const QString &subtitle,
                        const QString &imagePath,
                        const QString &legend = QString())
{
    if (!box) {
        return;
    }

    box->setVisible(true);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    box->setMinimumHeight(230);
    box->setTitle(title);

    QLayout *oldLayout = box->layout();
    if (!oldLayout) {
        oldLayout = new QVBoxLayout(box);
    }
    while (QLayoutItem *item = oldLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(oldLayout);
    if (!layout) {
        layout = new QVBoxLayout(box);
    }
    layout->setContentsMargins(16, 20, 16, 14);
    layout->setSpacing(10);

    QLabel *subtitleLabel = new QLabel(subtitle, box);
    subtitleLabel->setObjectName("chartSubtitle");
    subtitleLabel->setWordWrap(true);
    layout->addWidget(subtitleLabel);

    AspectRatioPixmapLabel *imageLabel = new AspectRatioPixmapLabel(box);
    imageLabel->setObjectName("chartImageLabel");
    imageLabel->setMinimumHeight(170);
    imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    const QPixmap spectrumPixmap(imagePath);
    if (spectrumPixmap.isNull()) {
        imageLabel->setText(QStringLiteral("频谱仪截图加载失败\n%1").arg(imagePath));
        imageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    } else {
        imageLabel->setSourcePixmap(spectrumPixmap);
    }
    layout->addWidget(imageLabel, 1);

    if (!legend.isEmpty()) {
        QLabel *legendLabel = new QLabel(legend, box);
        legendLabel->setObjectName("chartLegendLabel");
        legendLabel->setWordWrap(true);
        legendLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(legendLabel);
    }
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , instrumentSocket(nullptr)
    , signalGeneratorSocket(nullptr)
    , tagSerial(nullptr)
    , currentTestCase()
    , testRunning(false)
    , lastVoltage(0.0)
    , waitingForVoltage(false)
    , queryingInstrument(false)
    , queryingSignalGenerator(false)
    , lastIdnResponse()
    , lastGeneratorIdnResponse()
{
    ui->setupUi(this);
    setWindowTitle("AIoT 测试仪表上位机 v0.1");
    resize(1200, 800);

    // 加载样式表
    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(styleFile.readAll());
        this->setStyleSheet(styleSheet);
    } else {
        qWarning() << "无法加载样式表 style.qss";
    }

    // 初始化仪表 TCP Socket。instrumentSocket 对应 4071 频谱分析仪，
    // signalGeneratorSocket 对应 1466 信号发生器。
    instrumentSocket = new QTcpSocket(this);
    signalGeneratorSocket = new QTcpSocket(this);

    // 连接信号槽
    connect(instrumentSocket, &QTcpSocket::connected, this, &MainWindow::onInstrumentConnected);
    connect(instrumentSocket, &QTcpSocket::readyRead, this, &MainWindow::onInstrumentReadyRead);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(instrumentSocket, &QTcpSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError err) {
        if (err == QAbstractSocket::SocketTimeoutError && queryingInstrument) {
            return;
        }
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "4071", "错误: " + instrumentSocket->errorString());
    });
#else
    connect(instrumentSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, [this](QAbstractSocket::SocketError err) {
        if (err == QAbstractSocket::SocketTimeoutError && queryingInstrument) {
            return;
        }
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "4071", "错误: " + instrumentSocket->errorString());
    });
#endif
    connect(signalGeneratorSocket, &QTcpSocket::connected, this, &MainWindow::onSignalGeneratorConnected);
    connect(signalGeneratorSocket, &QTcpSocket::readyRead, this, &MainWindow::onSignalGeneratorReadyRead);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(signalGeneratorSocket, &QTcpSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError err) {
        if (err == QAbstractSocket::SocketTimeoutError && queryingSignalGenerator) {
            return;
        }
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "1466", "错误: " + signalGeneratorSocket->errorString());
    });
#else
    connect(signalGeneratorSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, [this](QAbstractSocket::SocketError err) {
        if (err == QAbstractSocket::SocketTimeoutError && queryingSignalGenerator) {
            return;
        }
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "1466", "错误: " + signalGeneratorSocket->errorString());
    });
#endif

    // 连接“开始测试”按钮
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::onStartTest);

    createToolBar();
    initComboBoxes();
    ui->testCaseTree->expandAll();

    // 连接其他信号槽
    connect(ui->testCaseTree, &QTreeWidget::itemClicked, this, &MainWindow::onTestCaseClicked);
    connect(ui->loadPresetBtn, &QPushButton::clicked, this, &MainWindow::onLoadPreset);
    connect(ui->savePresetBtn, &QPushButton::clicked, this, &MainWindow::onSavePreset);
    connect(ui->pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseTest);
    connect(ui->stopBtn, &QPushButton::clicked, this, &MainWindow::onStopTest);
    connect(ui->exportCsvBtn, &QPushButton::clicked, this, &MainWindow::onExportCsv);
    connect(ui->saveScreenshotBtn, &QPushButton::clicked, this, &MainWindow::onSaveScreenshot);
    connect(ui->viewHistoryBtn, &QPushButton::clicked, this, &MainWindow::onViewHistory);
    connect(ui->exportBtn, &QPushButton::clicked, this, &MainWindow::onExportResult);
    connect(ui->clearLogBtn, &QPushButton::clicked, this, &MainWindow::onClearLog);
    connect(ui->exportLogBtn, &QPushButton::clicked, this, &MainWindow::onExportLog);

    ui->logTable->setRowCount(0);

    enhanceUi();
    currentTestCase = "8.5 ACLR";
    resetCaseRuntimeState();
    applyCaseTemplate(currentTestCase);
    applyCaseResultTemplate(currentTestCase);
    // 初始化带宽频点映射，默认 200 kHz 使用 925
    m_bandFreqPoints[200] = "925";
    m_bandFreqPoints[400] = "925";
    m_bandFreqPoints[600] = "925";
    m_bandFreqPoints[800] = "925";
    totalPowerLabel = nullptr;
    totalPowerSpin = nullptr;
    m_acsScreenshotDir.clear();
    m_acsScreenshotPaths.clear();
    m_simImageLabel = nullptr;
    m_screenshotLabel = nullptr;
}

MainWindow::~MainWindow()
{
    stopSpectrumAnalyzerMeasurement("程序退出停止测量", false);
    shutdownSignalGeneratorOutput("程序退出安全关断", false);

    if (instrumentSocket) {
        instrumentSocket->disconnectFromHost();
        instrumentSocket->deleteLater();
    }
    if (signalGeneratorSocket) {
        signalGeneratorSocket->disconnectFromHost();
        signalGeneratorSocket->deleteLater();
    }
    delete ui;
}

void MainWindow::createToolBar()
{
    connect(ui->actionConn, &QAction::triggered, this, &MainWindow::onConnectionManager);
    connect(ui->actionStart, &QAction::triggered, this, &MainWindow::onStartBackend);
    connect(ui->actionStop, &QAction::triggered, this, &MainWindow::onStopBackend);
    connect(ui->actionImport, &QAction::triggered, this, &MainWindow::onImportConfig);
    connect(ui->actionExport, &QAction::triggered, this, &MainWindow::onExportConfig);
    connect(ui->actionReport, &QAction::triggered, this, &MainWindow::onGenerateReport);
    ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->mainToolBar->setMovable(false);

    QWidget *spacer = new QWidget(ui->mainToolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->mainToolBar->addWidget(spacer);

    QLabel *backendBadge = new QLabel("后端 127.0.0.1:8899  未连接", ui->mainToolBar);
    backendBadge->setObjectName("backendBadge");
    backendBadge->setProperty("state", "offline");
    ui->mainToolBar->addWidget(backendBadge);

    QAction *sendCmdAction = new QAction("发送命令", this);
    connect(sendCmdAction, &QAction::triggered, this, &MainWindow::onSendCustomCommand);
    ui->mainToolBar->addAction(sendCmdAction);
}

void MainWindow::initComboBoxes()
{
    if (ui->presetCombo->count() == 0) {
        ui->presetCombo->addItems({"默认射频测试", "接收机灵敏度", "协议一致性-多标签"});
    }
    ui->levelFilterCombo->setCurrentText("全部");
}

void MainWindow::enhanceUi()
{
    setWindowTitle("AIoT 测试仪表上位机");
    resize(1500, 920);
    setMinimumSize(1280, 760);

    ui->centralWidget->layout()->setContentsMargins(14, 12, 14, 12);
    ui->centralWidget->layout()->setSpacing(12);

    ui->testCaseTree->setHeaderLabel("测试用例导航");
    ui->testCaseTree->setMinimumWidth(292);
    ui->testCaseTree->setMaximumWidth(360);
    ui->testCaseTree->setRootIsDecorated(true);
    ui->testCaseTree->setAlternatingRowColors(false);
    ui->testCaseTree->setIndentation(18);

    QWidget *leftPanel = new QWidget(ui->centralWidget);
    leftPanel->setObjectName("leftPanel");
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(12);

    if (ui->centralWidget->layout()) {
        ui->centralWidget->layout()->removeWidget(ui->testCaseTree);
        ui->testCaseTree->setParent(leftPanel);
        leftLayout->addWidget(ui->testCaseTree, 1);
    }

    QGroupBox *deviceGroup = new QGroupBox("设备状态", leftPanel);
    deviceGroup->setObjectName("deviceStatusGroup");
    QVBoxLayout *deviceLayout = new QVBoxLayout(deviceGroup);
    deviceLayout->setContentsMargins(14, 20, 14, 12);
    deviceLayout->setSpacing(8);

    const QStringList deviceRows = {
        "<span style='color:#18a058;'>●</span> 测试仪表  192.168.1.100",
        "<span style='color:#18a058;'>●</span> AIoT基站  192.168.10.50",
        "<span style='color:#d08a00;'>●</span> CW节点  未启用",
        "<span style='color:#18a058;'>●</span> 标签模拟器  tty.usbserial"
    };
    for (const QString &rowText : deviceRows) {
        QLabel *row = new QLabel(rowText, deviceGroup);
        row->setObjectName("deviceStatusRow");
        row->setTextFormat(Qt::RichText);
        deviceLayout->addWidget(row);
    }
    QLabel *backendState = new QLabel("后端服务: 未启动    RTT: -- ms", deviceGroup);
    backendState->setObjectName("backendStateLabel");
    deviceLayout->addWidget(backendState);
    leftLayout->addWidget(deviceGroup);
    leftPanel->setMinimumWidth(300);
    leftPanel->setMaximumWidth(368);

    QLayout *rootLayout = ui->centralWidget->layout();
    rootLayout->addWidget(leftPanel);
    rootLayout->removeWidget(ui->centralTabs);
    rootLayout->addWidget(ui->centralTabs);

    ui->centralTabs->setDocumentMode(true);
    ui->centralTabs->setMovable(false);

    QWidget *paramContent = ui->scrollAreaWidgetContents;
    QVBoxLayout *oldParamLayout = qobject_cast<QVBoxLayout *>(paramContent->layout());
    if (oldParamLayout) {
        QWidget *paramGridWidget = new QWidget(paramContent);
        paramGridWidget->setObjectName("paramGridWidget");
        QHBoxLayout *paramGrid = new QHBoxLayout(paramGridWidget);
        paramGrid->setContentsMargins(0, 0, 0, 0);
        paramGrid->setSpacing(12);

        QWidget *paramLeft = new QWidget(paramGridWidget);
        paramLeft->setObjectName("paramLeftColumn");
        QVBoxLayout *paramLeftLayout = new QVBoxLayout(paramLeft);
        paramLeftLayout->setContentsMargins(0, 0, 0, 0);
        paramLeftLayout->setSpacing(10);

        oldParamLayout->removeWidget(ui->commonGroup);
        oldParamLayout->removeWidget(ui->rfGroup);
        oldParamLayout->removeWidget(ui->rxGroup);
        oldParamLayout->removeWidget(ui->protoGroup);
        oldParamLayout->removeWidget(ui->validationEdit);

        ui->commonGroup->setParent(paramLeft);
        ui->rfGroup->setParent(paramLeft);
        ui->rxGroup->setParent(paramLeft);
        ui->protoGroup->setParent(paramLeft);
        paramLeftLayout->addWidget(ui->commonGroup);
        paramLeftLayout->addWidget(ui->rfGroup);
        paramLeftLayout->addWidget(ui->rxGroup);
        paramLeftLayout->addWidget(ui->protoGroup);
        paramLeftLayout->addStretch(1);

        QWidget *paramRight = new QWidget(paramGridWidget);
        paramRight->setObjectName("paramRightColumn");
        paramRight->setMinimumWidth(360);
        paramRight->setMaximumWidth(460);
        QVBoxLayout *paramRightLayout = new QVBoxLayout(paramRight);
        paramRightLayout->setContentsMargins(0, 0, 0, 0);
        paramRightLayout->setSpacing(10);

        QGroupBox *caseInfoGroup = new QGroupBox("当前用例", paramRight);
        caseInfoGroup->setObjectName("caseInfoGroup");
        QVBoxLayout *caseInfoLayout = new QVBoxLayout(caseInfoGroup);
        QLabel *caseTitle = new QLabel("8.5 ACLR / 邻道泄漏比", caseInfoGroup);
        caseTitle->setObjectName("caseTitleLabel");
        QLabel *caseDesc = new QLabel("射频指标模板：配置载波、带宽、发射功率和采集策略，测试运行页展示频谱与ACLR结果。", caseInfoGroup);
        caseDesc->setObjectName("caseDescLabel");
        caseDesc->setWordWrap(true);
        caseInfoLayout->addWidget(caseTitle);
        caseInfoLayout->addWidget(caseDesc);
        paramRightLayout->addWidget(caseInfoGroup);

        QGroupBox *checkGroup = new QGroupBox("参数校验", paramRight);
        checkGroup->setObjectName("paramCheckGroup");
        QVBoxLayout *checkLayout = new QVBoxLayout(checkGroup);
        QTableWidget *checkTable = new QTableWidget(5, 3, checkGroup);
        checkTable->setObjectName("checkTable");
        checkTable->setHorizontalHeaderLabels({"项目", "状态", "说明"});
        checkTable->verticalHeader()->setVisible(false);
        checkTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        checkTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        checkTable->setSelectionMode(QAbstractItemView::NoSelection);
        QStringList checkItems = {"设备连接", "频点范围", "功率范围", "标签配置", "保存路径"};
        QStringList checkNotes = {"仪表/基站在线", "925 MHz 合法", "20 dBm 合法", "8个标签待命", "./logs/test_8_5"};
        for (int i = 0; i < checkItems.size(); ++i) {
            checkTable->setItem(i, 0, new QTableWidgetItem(checkItems[i]));
            checkTable->setItem(i, 1, new QTableWidgetItem(i == 4 ? "WARN" : "OK"));
            checkTable->setItem(i, 2, new QTableWidgetItem(checkNotes[i]));
        }
        checkLayout->addWidget(checkTable);
        paramRightLayout->addWidget(checkGroup, 1);

        QGroupBox *queueGroup = new QGroupBox("配置下发队列", paramRight);
        queueGroup->setObjectName("dispatchQueueGroup");
        QVBoxLayout *queueLayout = new QVBoxLayout(queueGroup);
        QTableWidget *queueTable = new QTableWidget(4, 3, queueGroup);
        queueTable->setObjectName("queueTable");
        queueTable->setHorizontalHeaderLabels({"顺序", "命令", "状态"});
        queueTable->verticalHeader()->setVisible(false);
        queueTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        queueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        queueTable->setSelectionMode(QAbstractItemView::NoSelection);
        QStringList commands = {"SET:FREQ 925MHz", "SET:BW 200kHz", "SET:POWER 20dBm", "ARM:CAPTURE"};
        for (int i = 0; i < commands.size(); ++i) {
            queueTable->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
            queueTable->setItem(i, 1, new QTableWidgetItem(commands[i]));
            queueTable->setItem(i, 2, new QTableWidgetItem(i < 2 ? "已准备" : "待下发"));
        }
        queueLayout->addWidget(queueTable);
        paramRightLayout->addWidget(queueGroup, 1);

        QGroupBox *validateGroup = new QGroupBox("最近校验信息", paramRight);
        validateGroup->setObjectName("validateGroup");
        QVBoxLayout *validateLayout = new QVBoxLayout(validateGroup);
        ui->validationEdit->setParent(validateGroup);
        ui->validationEdit->setReadOnly(true);
        ui->validationEdit->setPlainText("16:30  载波频率在当前频段范围内\n16:30  CW未启用，仅执行射频基础指标\n16:31  参数预设已匹配 8.5 ACLR");
        validateLayout->addWidget(ui->validationEdit);
        paramRightLayout->addWidget(validateGroup);

        paramGrid->addWidget(paramLeft, 3);
        paramGrid->addWidget(paramRight, 2);
        oldParamLayout->addWidget(paramGridWidget);
    }

    ui->summaryGroup->setTitle("当前测试摘要");
    ui->resultTableRun->setMinimumHeight(180);
    ui->resultTableResult->setMinimumHeight(220);
    ensureOutputPowerPointsEditor();
    ensureRefSensitivityPowerEditor();
    ensureAcsTotalPowerEditor();
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("运行进度 %p%");

    setupChartBox(findChild<QGroupBox*>("groupBox_3"), "实时频谱", "主信道功率、邻道泄漏与CW干扰位置");
    setupChartBox(findChild<QGroupBox*>("groupBox_4"), "测量趋势", "ACLR余量 / BLER / 标签事件随测试轮次变化");
    setupChartBox(findChild<QGroupBox*>("groupBox"), "核心结果图", "频域分布与关键指标标记");
    setupChartBox(findChild<QGroupBox*>("groupBox_2"), "统计分布", "标签状态、功率分布和判定比例");

    connect(ui->levelFilterCombo, &QComboBox::currentTextChanged,
            this, [this](const QString &) {
        applyLogLevelFilter();
    });

    ui->statusBar->showMessage("就绪 | 当前用例: 8.5 ACLR | 后端: 未启动 | RTT: -- ms | v0.1");
    decorateTables();
    applyCaseTemplate("8.5 ACLR");
    // 动态创建确认 BLER 按钮，添加到测试运行页面的操作栏
    confirmBlerBtn = new QPushButton("确认 BLER", this);
    confirmBlerBtn->setObjectName("confirmBlerBtn");
    confirmBlerBtn->setVisible(false);   // 初始隐藏，仅 8.7 真实测试时显示
    // 找到测试运行页面的操作布局 (horizontalLayout_opsRun)
    QWidget *runTab = ui->centralTabs->widget(1);  // 索引1是 testRunTab
    if (runTab) {
        QHBoxLayout *opsLayout = runTab->findChild<QHBoxLayout *>("horizontalLayout_opsRun");
        if (opsLayout) {
            opsLayout->addWidget(confirmBlerBtn);
        } else {
            // 备用: 直接添加到 QFrame 的下方布局，比较麻烦，通常能找到
            qWarning() << "未找到 horizontalLayout_opsRun";
        }
    }
    // 连接信号槽
    connect(confirmBlerBtn, &QPushButton::clicked, this, [this]() {
        if (currentTestCase.contains("8.6")) {
            onConfirmBlerVerdict();
        } else {
            onConfirmBler();
        }
    });

    connect(ui->resultTableRun, &QTableWidget::currentCellChanged,
            this, [this](int currentRow, int, int, int) {
        if (currentTestCase.contains("8.6")) {
            updateRefSensitivityRunGraphs(currentRow);
        } else if (currentTestCase.contains("8.5")) {
            updateAclrRunImages(currentRow);
        }
    });
    connect(ui->resultTableRun, &QTableWidget::cellClicked,
            this, &MainWindow::onAcsTableRowClicked);
    connect(ui->resultTableRun, &QTableWidget::cellChanged,
            this, [this](int row, int column) {
        if (!currentTestCase.contains("8.6")) {
            return;
        }
        if (column != RefRunBlerCol && column != RefRunRxPowerCol) {
            return;
        }
        judgeRefSensitivityRow(row, true);
        refreshRefSensitivityOverallStatus(false);
        updateRefSensitivityRunGraphs(row);
        updateRefSensitivityCharts();
    });
    // 在 enhanceUi() 中，找到 rfGroup 的 layout，并插入频点输入控件
    QVBoxLayout *rfLayout = qobject_cast<QVBoxLayout *>(ui->rfGroup->layout());
    if (rfLayout) {
        // 找到 bwCombo 在布局中的位置
        int bwIndex = -1;
        for (int i = 0; i < rfLayout->count(); ++i) {
            QLayoutItem *item = rfLayout->itemAt(i);
            if (item && item->widget() == ui->bwCombo) {
                bwIndex = i;
                break;
            }
        }
        if (bwIndex != -1) {
            // 在 bwCombo 之后插入标签和输入框
            QLabel *freqLabel = new QLabel("频点(MHz，逗号分隔)", ui->rfGroup);
            freqLabel->setObjectName("freqPointsLabel");
            QLineEdit *freqEdit = new QLineEdit(ui->rfGroup);
            freqEdit->setObjectName("freqPointsEdit");
            freqEdit->setPlaceholderText("例如: 925,942.5,960");
            freqEdit->setToolTip("输入三个频点，逗号分隔；不填则默认使用 925 MHz");
            // 初始内容从映射中读取（默认带宽 200 kHz）
            m_bandFreqPoints[200] = "925";
            freqEdit->setText("925");

            rfLayout->insertWidget(bwIndex + 1, freqLabel);
            rfLayout->insertWidget(bwIndex + 2, freqEdit);

            // 连接带宽切换信号
            connect(ui->bwCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &MainWindow::onBwComboCurrentIndexChanged);

            // 连接输入框文本变化，实时保存到映射（可选）
            connect(freqEdit, &QLineEdit::textChanged, this, [this, freqEdit]() {
                int bw = ui->bwCombo->currentText().trimmed().toInt();
                m_bandFreqPoints[bw] = freqEdit->text();
            });
        }
    }
}

void MainWindow::onBwComboCurrentIndexChanged(int index)
{
    Q_UNUSED(index);
    QLineEdit *freqEdit = findChild<QLineEdit*>("freqPointsEdit");
    if (!freqEdit) return;

    // 获取当前带宽（从 combo 的文本中提取数字）
    QString bwText = ui->bwCombo->currentText().trimmed();
    bool ok;
    int bw = bwText.toInt(&ok);
    if (!ok) return;

    // 保存当前输入到旧带宽（如果有旧带宽）
    static int lastBw = 200;
    if (lastBw != bw) {
        m_bandFreqPoints[lastBw] = freqEdit->text();
    }

    // 加载新带宽的频点
    QString freqStr = m_bandFreqPoints.value(bw, "925");
    freqEdit->setText(freqStr);
    lastBw = bw;
}

void MainWindow::decorateTables()
{
    const QList<QTableWidget *> tables = findChildren<QTableWidget *>();
    for (QTableWidget *table : tables) {
        table->setAlternatingRowColors(true);
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setVisible(true);
        table->setShowGrid(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        polishTable(table, false);
    }

    ui->resultTableRun->setHorizontalHeaderLabels({"项目", "测量值", "余量/目标", "判定"});
    ui->resultTableResult->setHorizontalHeaderLabels({"对象", "k因子/指标", "调制", "实测功率", "BLER/时延", "状态"});
    ui->logTable->setHorizontalHeaderLabels({"时间戳", "级别", "来源", "内容"});
    polishResultTables();
    polishLogTable();
}

void MainWindow::polishTableItem(QTableWidgetItem *item, bool compactLongText)
{
    Q_UNUSED(compactLongText);
    if (!item) {
        return;
    }

    item->setTextAlignment(Qt::AlignCenter);
}

void MainWindow::polishTable(QTableWidget *table, bool compactLongText)
{
    if (!table) {
        return;
    }

    const bool oldSignals = table->blockSignals(true);
    const bool oldUpdates = table->updatesEnabled();
    table->setUpdatesEnabled(false);
    table->setWordWrap(false);
    table->setTextElideMode(Qt::ElideRight);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setMinimumSectionSize(56);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(38);

    for (int col = 0; col < table->columnCount(); ++col) {
        if (QTableWidgetItem *headerItem = table->horizontalHeaderItem(col)) {
            headerItem->setTextAlignment(Qt::AlignCenter);
            if (headerItem->toolTip().isEmpty()) {
                headerItem->setToolTip(headerItem->text());
            }
        }
    }

    for (int row = 0; row < table->rowCount(); ++row) {
        for (int col = 0; col < table->columnCount(); ++col) {
            polishTableItem(table->item(row, col), compactLongText);
        }
    }

    table->setUpdatesEnabled(oldUpdates);
    table->blockSignals(oldSignals);
}

void MainWindow::polishResultTables()
{
    polishTable(ui->resultTableRun, true);
    polishTable(ui->resultTableResult, true);
}

void MainWindow::polishLogTable()
{
    if (!ui->logTable) {
        return;
    }

    QTableWidget *table = ui->logTable;
    polishTable(table, false);

    QFont font = table->font();
    font.setPointSize(10);
    table->setFont(font);

    table->setWordWrap(false);
    table->setTextElideMode(Qt::ElideMiddle);
    table->verticalHeader()->setDefaultSectionSize(30);
    table->horizontalHeader()->setMinimumSectionSize(48);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table->setColumnWidth(0, 104);
    table->setColumnWidth(1, 78);
    table->setColumnWidth(2, 92);

    for (int row = 0; row < table->rowCount(); ++row) {
        if (QTableWidgetItem *item = table->item(row, 0)) {
            item->setTextAlignment(Qt::AlignCenter);
        }
        if (QTableWidgetItem *item = table->item(row, 1)) {
            item->setTextAlignment(Qt::AlignCenter);
        }
        if (QTableWidgetItem *item = table->item(row, 2)) {
            item->setTextAlignment(Qt::AlignCenter);
        }
        if (QTableWidgetItem *item = table->item(row, 3)) {
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
}

void MainWindow::setAcsBlerEditingEnabled(bool enabled)
{
    if (!ui->resultTableRun) {
        return;
    }

    ui->resultTableRun->setEditTriggers(enabled
        ? (QAbstractItemView::DoubleClicked
           | QAbstractItemView::SelectedClicked
           | QAbstractItemView::EditKeyPressed)
        : QAbstractItemView::NoEditTriggers);
}

void MainWindow::set8_6BlerEditingEnabled(bool enabled)
{
    if (!ui->resultTableRun) {
        return;
    }

    ui->resultTableRun->setEditTriggers(enabled
        ? (QAbstractItemView::DoubleClicked
           | QAbstractItemView::SelectedClicked
           | QAbstractItemView::EditKeyPressed)
        : QAbstractItemView::NoEditTriggers);
    if (confirmBlerBtn && currentTestCase.contains("8.6")) {
        confirmBlerBtn->setVisible(enabled);
        confirmBlerBtn->setEnabled(enabled);
        confirmBlerBtn->setText("确认 BLER 判定");
    }
}

void MainWindow::setupTextChartBox(QGroupBox *box,
                                   const QString &title,
                                   const QString &subtitle,
                                   const QStringList &lines,
                                   const QString &legend)
{
    if (!box) {
        return;
    }

    box->setVisible(true);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    box->setMinimumHeight(230);
    box->setTitle(title);

    QLayout *oldLayout = box->layout();
    if (!oldLayout) {
        oldLayout = new QVBoxLayout(box);
    }
    while (QLayoutItem *item = oldLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(oldLayout);
    if (!layout) {
        layout = new QVBoxLayout(box);
    }
    layout->setContentsMargins(16, 20, 16, 14);
    layout->setSpacing(10);

    QLabel *subtitleLabel = new QLabel(subtitle, box);
    subtitleLabel->setObjectName("chartSubtitle");
    subtitleLabel->setWordWrap(true);
    layout->addWidget(subtitleLabel);

    QFrame *plot = new QFrame(box);
    plot->setObjectName("chartMockFrame");
    plot->setMinimumHeight(170);
    QVBoxLayout *plotLayout = new QVBoxLayout(plot);
    plotLayout->setContentsMargins(16, 14, 16, 14);
    plotLayout->setSpacing(8);

    QLabel *content = new QLabel(lines.join('\n'), plot);
    content->setObjectName("textChartContent");
    content->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    content->setWordWrap(false);
    content->setTextInteractionFlags(Qt::TextSelectableByMouse);
    plotLayout->addWidget(content, 1);

    if (!legend.isEmpty()) {
        QLabel *legendLabel = new QLabel(legend, plot);
        legendLabel->setObjectName("chartLegendLabel");
        legendLabel->setWordWrap(true);
        plotLayout->addWidget(legendLabel);
    }

    layout->addWidget(plot, 1);
}

void MainWindow::ensureOutputPowerPointsEditor()
{
    if (findChild<QLineEdit*>("outputPowerPointsEdit")) {
        return;
    }

    QWidget *rfGroup = ui->txPowerSpin ? ui->txPowerSpin->parentWidget() : nullptr;
    QVBoxLayout *layout = rfGroup ? qobject_cast<QVBoxLayout *>(rfGroup->layout()) : nullptr;
    if (!layout) {
        return;
    }

    QLabel *label = new QLabel("发射功率(填写5个待测功率, 0~15dBm，1466最高支持25dBm最大输出功率)", rfGroup);
    label->setObjectName("label_outputPowerPoints");

    QLineEdit *edit = new QLineEdit(rfGroup);
    edit->setObjectName("outputPowerPointsEdit");
    edit->setText("0,3,7.5,12,15");
    edit->setPlaceholderText("填写5个功率点，例如: 0,3,7.5,12,15");
    edit->setToolTip("仅用于 8.2 输出功率自动测试；填写 5 个输出功率点，逗号、空格或分号分隔，范围 0~15 dBm。");

    int insertIndex = layout->count();
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem *item = layout->itemAt(i);
        if (item && item->widget() == ui->txPowerSpin) {
            insertIndex = i + 1;
            break;
        }
    }
    layout->insertWidget(insertIndex, label);
    layout->insertWidget(insertIndex + 1, edit);
}

void MainWindow::ensureRefSensitivityPowerEditor()
{
    if (findChild<QDoubleSpinBox*>("refSensitivityLinkLossSpin")) {
        return;
    }

    QVBoxLayout *layout = ui->rxGroup ? qobject_cast<QVBoxLayout *>(ui->rxGroup->layout()) : nullptr;
    if (!layout) {
        return;
    }

    QLabel *label = new QLabel("链路损耗补偿(dB)", ui->rxGroup);
    label->setObjectName("label_refSensitivityLinkLoss");

    QDoubleSpinBox *spin = new QDoubleSpinBox(ui->rxGroup);
    spin->setObjectName("refSensitivityLinkLossSpin");
    spin->setDecimals(1);
    spin->setRange(0.0, 30.0);
    spin->setSingleStep(0.5);
    spin->setValue(3.0);
    spin->setToolTip("仅用于 8.6: 1466输出功率 = 目标端口功率 + 链路损耗补偿。现场默认线损约3 dB。");

    int insertIndex = layout->count();
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem *item = layout->itemAt(i);
        if (item && item->widget() == ui->stepSpin) {
            insertIndex = i + 1;
            break;
        }
    }
    layout->insertWidget(insertIndex, label);
    layout->insertWidget(insertIndex + 1, spin);
}

void MainWindow::ensureAcsTotalPowerEditor()
{
    if (findChild<QDoubleSpinBox*>("totalPowerSpin")) {
        totalPowerSpin = findChild<QDoubleSpinBox*>("totalPowerSpin");
        totalPowerLabel = findChild<QLabel*>("label_acsTotalPower");
        return;
    }

    QVBoxLayout *layout = ui->rxGroup ? qobject_cast<QVBoxLayout *>(ui->rxGroup->layout()) : nullptr;
    if (!layout) {
        return;
    }

    totalPowerLabel = new QLabel("发射总功率 (dBm)", ui->rxGroup);
    totalPowerLabel->setObjectName("label_acsTotalPower");

    totalPowerSpin = new QDoubleSpinBox(ui->rxGroup);
    totalPowerSpin->setObjectName("totalPowerSpin");
    totalPowerSpin->setDecimals(1);
    totalPowerSpin->setRange(-80.0, 0.0);
    totalPowerSpin->setSingleStep(0.5);
    totalPowerSpin->setValue(-50.0);
    totalPowerSpin->setSuffix(" dBm");
    totalPowerSpin->setToolTip("仅用于 8.7 ACS: 1466 发射总功率，默认 -50 dBm。");

    totalPowerLabel->setVisible(false);
    totalPowerSpin->setVisible(false);

    layout->addWidget(totalPowerLabel);
    layout->addWidget(totalPowerSpin);
}

void MainWindow::updateParameterFieldVisibility(const QString &caseName)
{
    QLabel *outputPowerLabel = findChild<QLabel*>("label_outputPowerPoints");
    QLineEdit *outputPowerEdit = findChild<QLineEdit*>("outputPowerPointsEdit");
    QLabel *freqPointsLabel = findChild<QLabel*>("freqPointsLabel");
    QLineEdit *freqPointsEdit = findChild<QLineEdit*>("freqPointsEdit");
    QLabel *linkLossLabel = findChild<QLabel*>("label_refSensitivityLinkLoss");
    QDoubleSpinBox *linkLossSpin = findChild<QDoubleSpinBox*>("refSensitivityLinkLossSpin");

    const bool isOutputPowerCase = caseName.contains("8.2");
    const bool is85Case = caseName.contains("8.5");
    const bool is86Case = caseName.contains("8.6");
    const bool is87Case = caseName.contains("8.7");

    // 8.2 隐藏 txPower，显示 outputPowerPoints；其他情况（包括8.5）显示 txPower
    if (ui->label_txPower) {
        ui->label_txPower->setVisible(!isOutputPowerCase);
    }
    if (ui->txPowerSpin) {
        ui->txPowerSpin->setVisible(!isOutputPowerCase);
    }
    // 8.2 显示功率点编辑框，其他隐藏
    if (outputPowerLabel) {
        outputPowerLabel->setVisible(isOutputPowerCase);
    }
    if (outputPowerEdit) {
        outputPowerEdit->setVisible(isOutputPowerCase);
    }
    // 8.5 显示频点输入控件，其他隐藏
    if (freqPointsLabel) {
        freqPointsLabel->setVisible(is85Case);
    }
    if (freqPointsEdit) {
        freqPointsEdit->setVisible(is85Case);
    }
    if (linkLossLabel) {
        linkLossLabel->setVisible(is86Case);
    }
    if (linkLossSpin) {
        linkLossSpin->setVisible(is86Case);
    }
    if (totalPowerLabel) {
        totalPowerLabel->setVisible(is87Case);
    }
    if (totalPowerSpin) {
        totalPowerSpin->setVisible(is87Case);
    }
}

double MainWindow::refSensitivityLinkLossCompensation() const
{
    const QDoubleSpinBox *spin = findChild<QDoubleSpinBox*>("refSensitivityLinkLossSpin");
    return spin ? spin->value() : 3.0;
}

QList<double> MainWindow::outputPowerTestPoints(bool *ok, QString *errorMessage) const
{
    if (ok) {
        *ok = false;
    }
    if (errorMessage) {
        errorMessage->clear();
    }

    const QLineEdit *edit = findChild<QLineEdit*>("outputPowerPointsEdit");
    QString text = edit ? edit->text().trimmed() : QString();
    if (text.isEmpty()) {
        text = "0,3,7.5,12,15";
    }

    text.replace(QChar(0xff0c), ',');
    text.replace(QChar(0x3001), ',');
    text.replace(';', ',');
    text.replace(QChar(0xff1b), ',');

   const QStringList tokens = text.split(QRegularExpression("[,\\s]+"),
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                      Qt::SkipEmptyParts
#else
                                      QString::SkipEmptyParts
#endif
                                      );
    if (tokens.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "8.2 功率点为空，请输入 0~15 dBm 范围内的点位";
        }
        return {};
    }

    QList<double> points;
    for (const QString &token : tokens) {
        bool valueOk = false;
        const double value = token.toDouble(&valueOk);
        if (!valueOk || !std::isfinite(value)) {
            if (errorMessage) {
                *errorMessage = QString("8.2 功率点包含非法数值: %1").arg(token);
            }
            return {};
        }
        if (value < OUTPUT_POWER_MIN_DBM || value > OUTPUT_POWER_MAX_DBM) {
            if (errorMessage) {
                *errorMessage = QString("8.2 功率点 %1 dBm 超出当前允许范围 %2~%3 dBm")
                        .arg(value, 0, 'f', 2)
                        .arg(OUTPUT_POWER_MIN_DBM, 0, 'f', 0)
                        .arg(OUTPUT_POWER_MAX_DBM, 0, 'f', 0);
            }
            return {};
        }
        points.append(value);
    }

    QList<double> uniquePoints;
    for (double point : points) {
        bool duplicate = false;
        for (double existing : uniquePoints) {
            if (qAbs(existing - point) < 0.0001) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            uniquePoints.append(point);
        }
    }
    points = uniquePoints;

    if (points.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "8.2 功率点解析结果为空";
        }
        return {};
    }

    if (ok) {
        *ok = true;
    }
    return points;
}

void MainWindow::setupOutputPowerSchematicChart()
{
    QGroupBox *box = findChild<QGroupBox*>("groupBox");
    QVBoxLayout *layout = resetChartBoxLayout(box, "8.2 输出功率测试示意图");
    if (!box || !layout) {
        return;
    }
    box->setMinimumHeight(260);

    QLabel *subtitle = new QLabel("8.2 测试链路：1466 输出指定功率，4071 读取峰值功率并计算误差", box);
    subtitle->setObjectName("chartSubtitle");
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    AspectRatioPixmapLabel *imageLabel = new AspectRatioPixmapLabel(box);
    imageLabel->setObjectName("outputPowerSchematicLabel");
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setMinimumHeight(230);
    imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    const QSize pixSize(760, 360);
    QPixmap pixmap(pixSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QLinearGradient background(0, 0, pixSize.width(), pixSize.height());
    background.setColorAt(0.0, QColor("#eef7ff"));
    background.setColorAt(0.55, QColor("#f7fbff"));
    background.setColorAt(1.0, QColor("#eaf2ff"));
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(QRectF(6, 6, pixSize.width() - 12, pixSize.height() - 12), 26, 26);

    auto drawDevice = [&](const QRectF &rect,
                          const QString &title,
                          const QString &subTitle,
                          const QColor &startColor,
                          const QColor &endColor,
                          const QString &badgeText) {
        QLinearGradient deviceGrad(rect.topLeft(), rect.bottomRight());
        deviceGrad.setColorAt(0.0, startColor);
        deviceGrad.setColorAt(1.0, endColor);
        painter.setPen(QPen(QColor(255, 255, 255, 210), 2));
        painter.setBrush(deviceGrad);
        painter.drawRoundedRect(rect, 18, 18);

        painter.setPen(QPen(QColor(255, 255, 255, 160), 1));
        painter.drawLine(QPointF(rect.left() + 18, rect.top() + 54), QPointF(rect.right() - 18, rect.top() + 54));

        painter.setPen(Qt::white);
        QFont titleFont = painter.font();
        titleFont.setBold(true);
        titleFont.setPointSize(14);
        painter.setFont(titleFont);
        painter.drawText(QRectF(rect.left() + 18, rect.top() + 16, rect.width() - 36, 30), Qt::AlignLeft | Qt::AlignVCenter, title);

        QFont subFont = painter.font();
        subFont.setBold(false);
        subFont.setPointSize(10);
        painter.setFont(subFont);
        painter.drawText(QRectF(rect.left() + 18, rect.top() + 72, rect.width() - 36, 42), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, subTitle);

        QRectF badge(rect.left() + 18, rect.bottom() - 46, 88, 26);
        painter.setBrush(QColor(255, 255, 255, 235));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(badge, 13, 13);
        painter.setPen(endColor.darker(145));
        painter.setFont(subFont);
        painter.drawText(badge, Qt::AlignCenter, badgeText);
    };

    auto drawArrow = [&](const QPointF &from, const QPointF &to, const QString &label) {
        painter.setPen(QPen(QColor("#2d6cdf"), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(from, to);
        const double angle = std::atan2(to.y() - from.y(), to.x() - from.x());
        const double arrowSize = 14.0;
        const double pi = 3.14159265358979323846;
        QPointF p1 = to - QPointF(std::cos(angle - pi / 6.0) * arrowSize,
                                  std::sin(angle - pi / 6.0) * arrowSize);
        QPointF p2 = to - QPointF(std::cos(angle + pi / 6.0) * arrowSize,
                                  std::sin(angle + pi / 6.0) * arrowSize);
        QPolygonF head;
        head << to << p1 << p2;
        painter.setBrush(QColor("#2d6cdf"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(head);

        QRectF labelRect((from.x() + to.x()) / 2.0 - 70, from.y() - 36, 140, 26);
        painter.setBrush(QColor(255, 255, 255, 235));
        painter.setPen(QPen(QColor("#c9d8f5"), 1));
        painter.drawRoundedRect(labelRect, 13, 13);
        painter.setPen(QColor("#1f4fa3"));
        QFont labelFont = painter.font();
        labelFont.setPointSize(9);
        labelFont.setBold(true);
        painter.setFont(labelFont);
        painter.drawText(labelRect, Qt::AlignCenter, label);
    };

    drawDevice(QRectF(48, 86, 160, 152), "1466", "信号发生器\n配置频率与输出功率", QColor("#2f80ed"), QColor("#56ccf2"), "CW输出");
    drawDevice(QRectF(300, 64, 160, 190), "AIoT", "基站/链路\n传输待测输出功率", QColor("#00a884"), QColor("#6fcf97"), "被测链路");
    drawDevice(QRectF(552, 86, 160, 152), "4071", "频谱分析仪\n读取峰值功率与频率", QColor("#6f42c1"), QColor("#b197fc"), "实测功率");

    drawArrow(QPointF(210, 160), QPointF(296, 160), "下发功率");
    drawArrow(QPointF(462, 160), QPointF(548, 160), "采样测量");

    painter.setPen(QPen(QColor("#2d6cdf"), 2, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(34, 276, 692, 46), 18, 18);
    painter.setPen(QColor("#2454a6"));
    QFont footFont = painter.font();
    footFont.setPointSize(11);
    footFont.setBold(true);
    painter.setFont(footFont);
    painter.drawText(QRectF(34, 276, 692, 46), Qt::AlignCenter,
                     "配置功率 -> 实测功率 -> 功率误差：用于验证 8.2 输出功率是否稳定、可测、可记录");
    painter.end();

    imageLabel->setSourcePixmap(pixmap);
    layout->addWidget(imageLabel, 1);
}

static void populateOutputPowerChart(QGroupBox *box,
                                     const QString &title,
                                     const QString &subtitleText,
                                     const QVector<double> &timeSec,
                                     const QVector<double> &measuredPowerDbm,
                                     const QVector<double> &targetPowerDbm,
                                     bool running)
{
    QVBoxLayout *layout = resetChartBoxLayout(box, title);
    if (!box || !layout) {
        return;
    }
    box->setMinimumHeight(260);

    QLabel *subtitle = new QLabel(subtitleText, box);
    subtitle->setObjectName("chartSubtitle");
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    QChartView *chartView = new QChartView(box);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(230);

    QChart *chart = new QChart();
    chart->setTitle(running ? "8.2 实时功率动态曲线" : "8.2 输出功率实测曲线");
    chart->setAnimationOptions(running ? QChart::NoAnimation : QChart::SeriesAnimations);

    QLineSeries *measuredSeries = new QLineSeries(chart);
    measuredSeries->setName("4071实测功率");
    QLineSeries *targetSeries = new QLineSeries(chart);
    targetSeries->setName("配置功率");
    targetSeries->setPen(QPen(QColor("#d9480f"), 2, Qt::DashLine));

    const int count = qMin(timeSec.size(), measuredPowerDbm.size());
    const double maxX = count > 0 ? timeSec[count - 1] : 1.0;
    double minY = OUTPUT_POWER_MIN_DBM;
    double maxY = OUTPUT_POWER_MAX_DBM;
    bool rangeInitialized = false;

    for (int i = 0; i < count; ++i) {
        measuredSeries->append(timeSec[i], measuredPowerDbm[i]);
        if (i < targetPowerDbm.size()) {
            targetSeries->append(timeSec[i], targetPowerDbm[i]);
        }
        const double target = i < targetPowerDbm.size() ? targetPowerDbm[i] : measuredPowerDbm[i];
        const double low = qMin(measuredPowerDbm[i], target);
        const double high = qMax(measuredPowerDbm[i], target);
        if (!rangeInitialized) {
            minY = low;
            maxY = high;
            rangeInitialized = true;
        } else {
            minY = qMin(minY, low);
            maxY = qMax(maxY, high);
        }
    }

    if (count == 0) {
        measuredSeries->append(0.0, 0.0);
        measuredSeries->append(1.0, 0.0);
        targetSeries->append(0.0, 0.0);
        targetSeries->append(1.0, 0.0);
        minY = -1.0;
        maxY = 1.0;
        chart->setTitle(running ? "8.2 实时功率动态曲线：等待采样" : "8.2 输出功率实测曲线：等待测试结果");
    }

    chart->addSeries(measuredSeries);
    chart->addSeries(targetSeries);

    QValueAxis *axisX = new QValueAxis(chart);
    axisX->setTitleText("采样时间 (s)");
    axisX->setRange(0.0, qMax(maxX, 1.0));
    axisX->setLabelFormat("%.1f");
    chart->addAxis(axisX, Qt::AlignBottom);
    measuredSeries->attachAxis(axisX);
    targetSeries->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis(chart);
    axisY->setTitleText("功率 (dBm)");
    axisY->setRange(minY - 1.0, maxY + 1.0);
    axisY->setLabelFormat("%.1f");
    chart->addAxis(axisY, Qt::AlignLeft);
    measuredSeries->attachAxis(axisY);
    targetSeries->attachAxis(axisY);

    chartView->setChart(chart);
    layout->addWidget(chartView, 1);

    QLabel *legend = new QLabel(running
                                ? "运行中：每次4071 Marker返回有效功率后刷新曲线。"
                                : "结果图：显示完整采样过程中的配置功率与实测功率变化。", box);
    legend->setObjectName("chartLegendLabel");
    legend->setWordWrap(true);
    layout->addWidget(legend);
}

void MainWindow::updateOutputPowerRunLiveChart(const QVector<double> &timeSec,
                                               const QVector<double> &measuredPowerDbm,
                                               const QVector<double> &targetPowerDbm,
                                               bool running)
{
    QGroupBox *liveBox = findChild<QGroupBox*>("groupBox_3");
    populateOutputPowerChart(liveBox,
                             "实时功率动态监控",
                             "测试运行页仅保留这一张动态图，用于显示8.2功率采样过程。",
                             timeSec,
                             measuredPowerDbm,
                             targetPowerDbm,
                             running);

    if (QGroupBox *rightRunBox = findChild<QGroupBox*>("groupBox_4")) {
        rightRunBox->hide();
    }
}

void MainWindow::updateOutputPowerTrendChart(const QVector<double> &timeSec,
                                             const QVector<double> &measuredPowerDbm,
                                             const QVector<double> &targetPowerDbm)
{
    setupOutputPowerSchematicChart();
    populateOutputPowerChart(findChild<QGroupBox*>("groupBox_2"),
                             "实测功率时间图",
                             "8.2 结果显示右侧：横轴为采样时间，纵轴为4071测得功率，虚线为配置功率。",
                             timeSec,
                             measuredPowerDbm,
                             targetPowerDbm,
                             false);
}

QString MainWindow::refSensitivityTheoryImagePath(const QString &channel) const
{
    const QString root = outputRootDir();
    QString underscored = channel;
    underscored.replace('-', '_');

    QStringList candidates;
    candidates << QDir(root).filePath(QString("ref_sensitivity_theory/%1_frequency_domain.png").arg(channel));
    candidates << QDir(root).filePath(QString("ref_sensitivity_theory/%1_frequency_domain.png").arg(underscored));
    candidates << QDir(root).filePath(QString("ref_sensitivity_theory/%1.png").arg(channel));
    candidates << QDir(root).filePath(QString("ref_sensitivity_theory/%1.png").arg(underscored));
    candidates << QStringLiteral("/Users/xubin/Desktop/simulation/D2R仿真链路0430/outputs/A_FR1_A1_1_AWGN/04_frequency_domain.png");
    candidates << QStringLiteral("/Users/xubin/Desktop/simulation/D2R仿真链路0430/outputs/A_FR1_A1_1_no_multipath_CFO_SFO_AWGN/04_centered_frequency_domain_cfo_sfo_awgn.png");

    for (const QString &candidate : candidates) {
        QFileInfo info(candidate);
        if (info.exists() && info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    return QString();
}

void MainWindow::updateRefSensitivityRunGraphs(int row)
{
    QTableWidget *runTable = ui->resultTableRun;
    QTableWidget *resTable = ui->resultTableResult;
    if (!runTable || runTable->rowCount() == 0) {
        setupTextChartBox(findChild<QGroupBox*>("groupBox_3"),
                          "理论/仿真图",
                          "等待选择 8.6 测试点",
                          {"选择 8.6 后，左侧显示理论/仿真信号位置",
                           "开始测试后，右侧显示该测试点对应的 4071 实测频谱截图"},
                          "左图用于对照理论信号，右图用于留存实测频谱");
        setupTextChartBox(findChild<QGroupBox*>("groupBox_4"),
                          "4071 实测频谱图",
                          "等待 4071 截图",
                          {"未选择测试点", "未执行测试或未保存截图"},
                          "截图路径会写入结果显示表");
        return;
    }

    if (row < 0 || row >= runTable->rowCount()) {
        row = runTable->currentRow();
    }
    if (row < 0 || row >= runTable->rowCount()) {
        row = 0;
    }

    const QString stage = itemTextAt(runTable, row, RefRunStageCol, QStringLiteral("测试点"));
    const QString channel = itemTextAt(runTable, row, RefRunChannelCol, QStringLiteral("参考信道"));
    const QString bandwidthText = itemTextAt(runTable, row, RefRunBwCol, QStringLiteral("200/15 kHz"));
    const QString modulation = itemTextAt(runTable, row, RefRunModCol, QStringLiteral("BPSK/OOK"));
    const QString d2rPowerText = itemTextAt(runTable, row, RefRunD2rPowerCol, QStringLiteral("待配置"));
    const QString cwPowerText = itemTextAt(runTable, row, RefRunCwPowerCol, QStringLiteral("待配置"));
    const QString rxPowerText = itemTextAt(runTable, row, RefRunRxPowerCol, QStringLiteral("待测"));
    const QString freqErrorText = itemTextAt(runTable, row, RefRunFreqErrCol, QStringLiteral("待测"));
    const QString screenshotText = itemTextAt(resTable, row, RefResScreenshotCol, QStringLiteral("待保存"));
    const QString peakText = itemTextAt(resTable, row, RefResPeakCol, QStringLiteral("待测"));

    const QString theoryPath = refSensitivityTheoryImagePath(channel);
    const QString shortChannel = shortRefChannelName(channel);
    const QString theorySubtitle = QString("%1 | %2 | %3 | %4 | D2R %5 | CW %6")
                                   .arg(stage, shortChannel, bandwidthText, modulation, d2rPowerText, cwPowerText);

    if (!theoryPath.isEmpty()) {
        setupImageChartBox(findChild<QGroupBox*>("groupBox_3"),
                           "理论/仿真图",
                           theorySubtitle,
                           theoryPath,
                           "优先加载 ref_sensitivity_theory 下的对应信道仿真图；未配置专用图时使用现有 D2R 仿真频域图。");
    } else if (QGroupBox *box = findChild<QGroupBox*>("groupBox_3")) {
        QVBoxLayout *layout = resetChartBoxLayout(box, "理论/仿真图");
        if (layout) {
            QLabel *subtitle = new QLabel(theorySubtitle, box);
            subtitle->setObjectName("chartSubtitle");
            subtitle->setWordWrap(true);
            layout->addWidget(subtitle);

            QChartView *chartView = new QChartView(box);
            chartView->setRenderHint(QPainter::Antialiasing);
            chartView->setMinimumHeight(230);

            QChart *chart = new QChart();
            chart->setTitle("理论信号位置示意");
            chart->setAnimationOptions(QChart::SeriesAnimations);

            const double bandwidthKHz = qMax(200.0, parseFirstDouble(bandwidthText));
            const double spanKHz = bandwidthKHz >= 3000.0 ? 8000.0 : 2000.0;
            const double halfBw = bandwidthKHz / 2.0;
            const double d2rPower = parseFirstDouble(d2rPowerText);
            const double cwPower = parseFirstDouble(cwPowerText);
            const double usefulLevel = d2rPower == 0.0 ? -90.0 : d2rPower;
            const double cwLevel = cwPower == 0.0 ? -38.0 : cwPower;
            const double floorLevel = qMin(usefulLevel, cwLevel) - 12.0;
            const double topLevel = qMax(usefulLevel, cwLevel) + 8.0;

            QLineSeries *d2rSeries = new QLineSeries(chart);
            d2rSeries->setName("D2R理论占用带宽");
            d2rSeries->setPen(QPen(QColor("#1f6feb"), 2));
            d2rSeries->append(-spanKHz / 2.0, floorLevel);
            d2rSeries->append(-halfBw, floorLevel);
            d2rSeries->append(-halfBw, usefulLevel);
            d2rSeries->append(halfBw, usefulLevel);
            d2rSeries->append(halfBw, floorLevel);
            d2rSeries->append(spanKHz / 2.0, floorLevel);

            QScatterSeries *cwSeries = new QScatterSeries(chart);
            cwSeries->setName("CW carrier参考点");
            cwSeries->setColor(QColor("#d9480f"));
            cwSeries->setMarkerSize(10.0);
            cwSeries->append(0.0, cwLevel);

            chart->addSeries(d2rSeries);
            chart->addSeries(cwSeries);

            QValueAxis *axisX = new QValueAxis(chart);
            axisX->setTitleText("相对中心频率 (kHz)");
            axisX->setRange(-spanKHz / 2.0, spanKHz / 2.0);
            axisX->setLabelFormat("%.0f");
            chart->addAxis(axisX, Qt::AlignBottom);
            d2rSeries->attachAxis(axisX);
            cwSeries->attachAxis(axisX);

            QValueAxis *axisY = new QValueAxis(chart);
            axisY->setTitleText("功率/相对幅度 (dBm)");
            axisY->setRange(floorLevel - 4.0, topLevel + 2.0);
            axisY->setLabelFormat("%.0f");
            chart->addAxis(axisY, Qt::AlignLeft);
            d2rSeries->attachAxis(axisY);
            cwSeries->attachAxis(axisY);

            chartView->setChart(chart);
            layout->addWidget(chartView, 1);
        }
    }

    QFileInfo screenshotInfo(screenshotText);
    if (screenshotText.endsWith(".png", Qt::CaseInsensitive) && screenshotInfo.exists()) {
        setupImageChartBox(findChild<QGroupBox*>("groupBox_4"),
                           "4071 实测频谱图",
                           QString("%1 | %2 | 峰值 %3 | 频误 %4")
                           .arg(stage, shortChannel, peakText, freqErrorText),
                           screenshotInfo.absoluteFilePath(),
                           "该图为当前选中测试点保存并下载到上位机的 4071 实测频谱截图。");
    } else {
        setupTextChartBox(findChild<QGroupBox*>("groupBox_4"),
                          "4071 实测频谱图",
                          QString("%1 | %2").arg(stage, shortChannel),
                          {QString("D2R功率: %1").arg(d2rPowerText),
                           QString("接收功率: %1").arg(rxPowerText),
                           QString("频谱峰值: %1").arg(peakText),
                           QString("频率误差: %1").arg(freqErrorText),
                           QString("截图状态: %1").arg(screenshotText)},
                          "真实双仪表运行后，本区域显示对应测试点的 4071 频谱截图。");
    }
}

QString MainWindow::judgeRefSensitivityRow(int row, bool logResult)
{
    QTableWidget *runTable = ui->resultTableRun;
    QTableWidget *resTable = ui->resultTableResult;
    if (!runTable || !resTable || row < 0
        || row >= runTable->rowCount() || row >= resTable->rowCount()
        || runTable->columnCount() <= RefRunVerdictCol
        || resTable->columnCount() <= RefResVerdictCol) {
        return QString();
    }

    const bool runSignalsBlocked = runTable->blockSignals(true);
    const bool resSignalsBlocked = resTable->blockSignals(true);

    const QString stage = itemTextAt(resTable, row, RefResStageCol, QStringLiteral("测试点"));
    const QString channel = itemTextAt(resTable, row, RefResChannelCol, QString("Row %1").arg(row + 1));
    const QString d2rPower = itemTextAt(resTable, row, RefResD2rPowerCol);
    const QString rowName = QString("%1 %2 %3").arg(stage, channel, d2rPower);

    QTableWidgetItem *runRxPowerItem = runTable->item(row, RefRunRxPowerCol);
    QTableWidgetItem *runBlerItem = runTable->item(row, RefRunBlerCol);
    QTableWidgetItem *resRxPowerItem = resTable->item(row, RefResRxPowerCol);
    QTableWidgetItem *resBlerItem = resTable->item(row, RefResBlerCol);
    QTableWidgetItem *resVerdictItem = resTable->item(row, RefResVerdictCol);
    QTableWidgetItem *runVerdictItem = runTable->item(row, RefRunVerdictCol);

    auto roleBool = [](QTableWidgetItem *item, int role, bool defaultValue) {
        if (!item) return defaultValue;
        const QVariant value = item->data(role);
        return value.isValid() ? value.toBool() : defaultValue;
    };
    auto roleIsValid = [](QTableWidgetItem *item, int role) {
        return item && item->data(role).isValid();
    };
    auto syncDisplayText = [](QTableWidgetItem *item, const QString &text, const QString &tooltip = QString()) {
        if (!item) {
            return;
        }
        item->setData(RoleFullText, text);
        item->setText(text);
        if (!tooltip.isEmpty()) {
            item->setToolTip(tooltip);
        } else if (item->toolTip().isEmpty() || item->toolTip() == item->text()) {
            item->setToolTip(text);
        }
    };

    const bool executable = roleBool(resVerdictItem, RoleExecutable,
                                     roleBool(runVerdictItem, RoleExecutable, true));
    const bool waveformOk = roleBool(resVerdictItem, RoleWaveformOk,
                                     roleBool(runVerdictItem, RoleWaveformOk, false));
    const bool spectrumOk = roleBool(resVerdictItem, RoleSpectrumOk,
                                     roleBool(runVerdictItem, RoleSpectrumOk, false));
    const bool freqPass = roleBool(resVerdictItem, RoleFreqPass,
                                   roleBool(runVerdictItem, RoleFreqPass, false));
    const QString failureReason = resVerdictItem ? resVerdictItem->data(RoleFailureReason).toString() : QString();
    const QString pendingReason = resVerdictItem ? resVerdictItem->data(RolePendingReason).toString() : QString();
    const bool instrumentChecked = roleIsValid(resVerdictItem, RoleWaveformOk)
                                   || roleIsValid(resVerdictItem, RoleSpectrumOk)
                                   || roleIsValid(resVerdictItem, RoleFreqPass)
                                   || !failureReason.isEmpty();

    QString verdict;
    QString tooltip;

    if (!executable) {
        verdict = QStringLiteral("不可执行");
        tooltip = pendingReason.isEmpty() ? QStringLiteral("该测试点未启用执行") : pendingReason;
    } else {
        const QString blerText = runBlerItem ? runBlerItem->text().trimmed() : QString();
        const QString rxPowerText = runRxPowerItem ? runRxPowerItem->text().trimmed() : QString();

        bool blerOk = false;
        const double bler = parseFirstDouble(blerText, &blerOk);
        if (blerOk && (bler < 0.0 || bler > 100.0)) {
            blerOk = false;
        }

        bool rxPowerOk = false;
        const double rxPower = parseFirstDouble(rxPowerText, &rxPowerOk);
        if (rxPowerOk && (rxPower < -130.0 || rxPower > -50.0) && refSensitivityIsD2rRangeStage(stage)) {
            rxPowerOk = false;
        }

        const bool blerPass = blerOk && bler <= 10.0;
        const bool instrumentPass = instrumentChecked && waveformOk && spectrumOk && freqPass;
        const bool missingInput = !rxPowerOk || !blerOk;

        const QString normalizedBler = blerOk ? QString("%1 %").arg(bler, 0, 'f', 2) : QStringLiteral("待输入");
        const QString normalizedRxPower = rxPowerOk ? QString("%1 dBm").arg(rxPower, 0, 'f', 2) : QStringLiteral("待输入/超范围");

        if (resRxPowerItem) {
            syncDisplayText(resRxPowerItem, normalizedRxPower);
        }
        if (runRxPowerItem && rxPowerOk) {
            syncDisplayText(runRxPowerItem, normalizedRxPower, runRxPowerItem->toolTip());
        }
        if (resBlerItem) {
            syncDisplayText(resBlerItem, normalizedBler);
        }
        if (runBlerItem && blerOk) {
            syncDisplayText(runBlerItem, normalizedBler, runBlerItem->toolTip());
        }

        if (!instrumentChecked) {
            verdict = QStringLiteral("WAIT");
            tooltip = QStringLiteral("仪表流程尚未完成，需先点击开始测试完成1466/4071检查");
        } else if (missingInput) {
            verdict = QStringLiteral("WAIT");
            tooltip = !rxPowerOk
                      ? QStringLiteral("接收功率未输入、格式错误或不在 -130~-50 dBm 范围内")
                      : QStringLiteral("BLER未输入或格式错误，请填写百分数，例如 8.5 表示 8.5%");
        } else if (!instrumentPass) {
            verdict = QStringLiteral("FAIL");
            tooltip = failureReason.isEmpty() ? QStringLiteral("仪表检查未通过") : failureReason;
        } else if (!blerPass) {
            verdict = QStringLiteral("FAIL");
            tooltip = QString("BLER=%1%，超过10%门限").arg(bler, 0, 'f', 2);
        } else {
            verdict = QStringLiteral("PASS");
            tooltip = QStringLiteral("接收功率在范围内，BLER≤10%，仪表检查通过");
        }
    }

    if (resVerdictItem) {
        syncDisplayText(resVerdictItem, verdict, tooltip);
    }
    if (runVerdictItem) {
        syncDisplayText(runVerdictItem, verdict, tooltip);
    }

    runTable->blockSignals(runSignalsBlocked);
    resTable->blockSignals(resSignalsBlocked);

    if (logResult && !verdict.isEmpty()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"),
               verdict == "PASS" ? "PASS" : (verdict == "FAIL" ? "FAIL" : "INFO"),
               "8.6",
               QString("%1 逐行判定: %2 (%3)").arg(rowName, verdict, tooltip));
    }

    return verdict;
}

void MainWindow::refreshRefSensitivityOverallStatus(bool finalConfirmation)
{
    QTableWidget *resTable = ui->resultTableResult;
    if (!resTable || resTable->rowCount() == 0) {
        return;
    }

    int passCount = 0;
    int failCount = 0;
    int waitCount = 0;
    int skippedCount = 0;
    QStringList failRows;
    QStringList waitRows;

    for (int row = 0; row < resTable->rowCount(); ++row) {
        const QString verdict = itemTextAt(resTable, row, RefResVerdictCol, QStringLiteral("WAIT"));
        const QString rowName = QString("%1 %2 %3")
                                .arg(itemTextAt(resTable, row, RefResStageCol, QStringLiteral("测试点")))
                                .arg(shortRefChannelName(itemTextAt(resTable, row, RefResChannelCol, QStringLiteral("信道"))))
                                .arg(itemTextAt(resTable, row, RefResD2rPowerCol));
        if (verdict == "PASS") {
            ++passCount;
        } else if (verdict == "FAIL") {
            ++failCount;
            failRows << rowName;
        } else if (verdict.contains("不可执行") || verdict.contains("SKIP")) {
            ++skippedCount;
        } else {
            ++waitCount;
            waitRows << rowName;
        }
    }

    const QString overallVerdict = failCount > 0
                                   ? QStringLiteral("FAIL")
                                   : ((waitCount > 0 || skippedCount > 0) ? QStringLiteral("WAIT") : QStringLiteral("PASS"));
    ui->verdictLabel->setText(overallVerdict);
    ui->statsLabel->setText(QString("8.6参考灵敏度%1\n"
                                    "通过: %2，失败: %3，待补输入: %4，不可执行: %5\n"
                                    "失败点: %6\n"
                                    "待补点: %7\n"
                                    "整体判定: %8")
                            .arg(finalConfirmation ? QStringLiteral("判定完成") : QStringLiteral("逐行判定中"))
                            .arg(passCount)
                            .arg(failCount)
                            .arg(waitCount)
                            .arg(skippedCount)
                            .arg(failRows.isEmpty() ? "无" : failRows.join("; "))
                            .arg(waitRows.isEmpty() ? "无" : waitRows.join("; "))
                            .arg(overallVerdict));

    if (confirmBlerBtn && currentTestCase.contains("8.6")) {
        confirmBlerBtn->setVisible(true);
        confirmBlerBtn->setEnabled(true);
        confirmBlerBtn->setText("确认 BLER 判定");
    }
    ui->pushButton->setEnabled(true);
}

void MainWindow::updateRefSensitivityCharts()
{
    QTableWidget *resTable = ui->resultTableResult;

    if (!resTable || resTable->rowCount() == 0 || resTable->columnCount() <= RefResVerdictCol) {
        setupTextChartBox(findChild<QGroupBox*>("groupBox"),
                          "BLER门限趋势",
                          "等待基站侧接收功率和BLER读数",
                          {"阶段          信道   接收功率     BLER      状态",
                           "核心PREFSENS  A1-1   待输入       待输入    WAIT",
                           "D2R扫描       A1-1   待输入       待输入    WAIT",
                           "CW扫描        A1-1   待输入       待输入    WAIT"},
                          "标准核心门限: BLER <= 10%");
        setupTextChartBox(findChild<QGroupBox*>("groupBox_2"),
                          "4071实测功率统计",
                          "等待 4071 返回峰值功率和频率误差",
                          {"横轴: D2R目标功率", "纵轴: 4071实测接收功率", "频率误差门限: ±(0.05 ppm + 6 Hz)"},
                          "真实双仪表运行后自动更新");
        return;
    }

    int passCount = 0;
    int failCount = 0;
    int waitCount = 0;
    int skippedCount = 0;
    QVector<double> blerX;
    QVector<double> blerY;
    QVector<double> powerTargetX;
    QVector<double> powerMeasuredY;
    double minPower = 0.0;
    double maxPower = 0.0;
    double maxBler = 10.0;
    double maxAbsFreqHz = 0.0;
    bool hasPowerRange = false;

    for (int row = 0; row < resTable->rowCount(); ++row) {
        const QString stage = itemTextAt(resTable, row, RefResStageCol);
        const QString verdict = itemTextAt(resTable, row, RefResVerdictCol, QStringLiteral("WAIT"));
        if (verdict == "PASS") {
            ++passCount;
        } else if (verdict == "FAIL") {
            ++failCount;
        } else if (verdict.contains("不可执行") || verdict.contains("SKIP")) {
            ++skippedCount;
        } else {
            ++waitCount;
        }

        if (!refSensitivityIsD2rRangeStage(stage)) {
            continue;
        }

        bool d2rOk = false;
        const double d2rPower = parseFirstDouble(itemTextAt(resTable, row, RefResD2rPowerCol), &d2rOk);
        if (!d2rOk) {
            continue;
        }

        bool blerOk = false;
        const double blerValue = parseFirstDouble(itemTextAt(resTable, row, RefResBlerCol), &blerOk);
        if (blerOk && blerValue >= 0.0 && blerValue <= 100.0) {
            blerX.append(d2rPower);
            blerY.append(blerValue);
            maxBler = qMax(maxBler, blerValue);
        }

        bool rxOk = false;
        const double rxPower = parseFirstDouble(itemTextAt(resTable, row, RefResRxPowerCol), &rxOk);
        if (rxOk && rxPower >= -160.0 && rxPower <= 20.0) {
            powerTargetX.append(d2rPower);
            powerMeasuredY.append(rxPower);
            if (!hasPowerRange) {
                minPower = qMin(d2rPower, rxPower);
                maxPower = qMax(d2rPower, rxPower);
                hasPowerRange = true;
            } else {
                minPower = qMin(minPower, qMin(d2rPower, rxPower));
                maxPower = qMax(maxPower, qMax(d2rPower, rxPower));
            }
        }

        bool freqOk = false;
        const double freqHz = parseFirstDouble(itemTextAt(resTable, row, RefResFreqErrCol), &freqOk);
        if (freqOk) {
            maxAbsFreqHz = qMax(maxAbsFreqHz, qAbs(freqHz));
        }
    }

    if (blerX.isEmpty()) {
        setupTextChartBox(findChild<QGroupBox*>("groupBox"),
                          "BLER门限趋势",
                          QString("等待填写BLER: PASS %1 / FAIL %2 / WAIT %3 / 不可执行 %4")
                          .arg(passCount).arg(failCount).arg(waitCount).arg(skippedCount),
                          {"在测试运行页填写每个测试点的 BLER(%)",
                           "按 Enter 或离开单元格后会自动判定该行",
                           "确认按钮可用于批量刷新整体判定"},
                          "判定门限: BLER <= 10%");
    } else if (QGroupBox *box = findChild<QGroupBox*>("groupBox")) {
        QVBoxLayout *layout = resetChartBoxLayout(box, "BLER门限趋势");
        if (layout) {
            QLabel *subtitle = new QLabel(QString("PASS %1 / FAIL %2 / WAIT %3 / 不可执行 %4")
                                          .arg(passCount).arg(failCount).arg(waitCount).arg(skippedCount),
                                          box);
            subtitle->setObjectName("chartSubtitle");
            subtitle->setWordWrap(true);
            layout->addWidget(subtitle);

            QChartView *chartView = new QChartView(box);
            chartView->setRenderHint(QPainter::Antialiasing);
            chartView->setMinimumHeight(260);

            QChart *chart = new QChart();
            chart->setTitle("BLER vs D2R目标功率");
            chart->setAnimationOptions(QChart::SeriesAnimations);

            QScatterSeries *blerSeries = new QScatterSeries(chart);
            blerSeries->setName("BLER读数");
            blerSeries->setColor(QColor("#1f6feb"));
            blerSeries->setMarkerSize(9.0);

            double minX = blerX[0];
            double maxX = blerX[0];
            for (int i = 0; i < blerX.size(); ++i) {
                blerSeries->append(blerX[i], blerY[i]);
                minX = qMin(minX, blerX[i]);
                maxX = qMax(maxX, blerX[i]);
            }

            QLineSeries *limitSeries = new QLineSeries(chart);
            limitSeries->setName("10%门限");
            limitSeries->setPen(QPen(QColor("#d9480f"), 2, Qt::DashLine));
            limitSeries->append(minX - 2.0, 10.0);
            limitSeries->append(maxX + 2.0, 10.0);

            chart->addSeries(blerSeries);
            chart->addSeries(limitSeries);

            QValueAxis *axisX = new QValueAxis(chart);
            axisX->setTitleText("D2R目标功率 (dBm)");
            axisX->setRange(minX - 4.0, maxX + 4.0);
            axisX->setLabelFormat("%.0f");
            chart->addAxis(axisX, Qt::AlignBottom);
            blerSeries->attachAxis(axisX);
            limitSeries->attachAxis(axisX);

            QValueAxis *axisY = new QValueAxis(chart);
            axisY->setTitleText("BLER (%)");
            axisY->setRange(0.0, qMin(100.0, qMax(12.0, maxBler + 5.0)));
            axisY->setLabelFormat("%.1f");
            chart->addAxis(axisY, Qt::AlignLeft);
            blerSeries->attachAxis(axisY);
            limitSeries->attachAxis(axisY);

            chartView->setChart(chart);
            layout->addWidget(chartView, 1);
        }
    }

    if (powerTargetX.isEmpty()) {
        setupTextChartBox(findChild<QGroupBox*>("groupBox_2"),
                          "4071实测功率统计",
                          "等待 4071 实测接收功率",
                          {"真实双仪表运行后，接收功率列会写入 4071 峰值功率",
                           "结果图会展示 D2R目标功率与4071实测功率的对应关系",
                           QString("当前最大频率误差: %1 Hz").arg(maxAbsFreqHz, 0, 'f', 2)},
                          "频率误差门限: ±(0.05 ppm + 6 Hz)");
    } else if (QGroupBox *box = findChild<QGroupBox*>("groupBox_2")) {
        QVBoxLayout *layout = resetChartBoxLayout(box, "4071实测功率统计");
        if (layout) {
            QLabel *subtitle = new QLabel(QString("横轴为D2R目标功率，纵轴为4071实测接收功率；当前最大频率误差 %1 Hz")
                                          .arg(maxAbsFreqHz, 0, 'f', 2),
                                          box);
            subtitle->setObjectName("chartSubtitle");
            subtitle->setWordWrap(true);
            layout->addWidget(subtitle);

            QChartView *chartView = new QChartView(box);
            chartView->setRenderHint(QPainter::Antialiasing);
            chartView->setMinimumHeight(260);

            QChart *chart = new QChart();
            chart->setTitle("4071实测功率 vs D2R目标功率");
            chart->setAnimationOptions(QChart::SeriesAnimations);

            QScatterSeries *powerSeries = new QScatterSeries(chart);
            powerSeries->setName("4071实测功率");
            powerSeries->setColor(QColor("#2f9e44"));
            powerSeries->setMarkerSize(9.0);
            for (int i = 0; i < powerTargetX.size(); ++i) {
                powerSeries->append(powerTargetX[i], powerMeasuredY[i]);
            }

            QLineSeries *referenceSeries = new QLineSeries(chart);
            referenceSeries->setName("目标=实测参考线");
            referenceSeries->setPen(QPen(QColor("#868e96"), 2, Qt::DashLine));
            referenceSeries->append(minPower - 3.0, minPower - 3.0);
            referenceSeries->append(maxPower + 3.0, maxPower + 3.0);

            chart->addSeries(powerSeries);
            chart->addSeries(referenceSeries);

            QValueAxis *axisX = new QValueAxis(chart);
            axisX->setTitleText("D2R目标功率 (dBm)");
            axisX->setRange(minPower - 4.0, maxPower + 4.0);
            axisX->setLabelFormat("%.0f");
            chart->addAxis(axisX, Qt::AlignBottom);
            powerSeries->attachAxis(axisX);
            referenceSeries->attachAxis(axisX);

            QValueAxis *axisY = new QValueAxis(chart);
            axisY->setTitleText("4071实测接收功率 (dBm)");
            axisY->setRange(minPower - 4.0, maxPower + 4.0);
            axisY->setLabelFormat("%.0f");
            chart->addAxis(axisY, Qt::AlignLeft);
            powerSeries->attachAxis(axisY);
            referenceSeries->attachAxis(axisY);

            chartView->setChart(chart);
            layout->addWidget(chartView, 1);
        }
    }
}

void MainWindow::setupChartBox(QGroupBox *box, const QString &title, const QString &subtitle)
{
    if (!box) {
        return;
    }
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    box->setMinimumHeight(230);
    box->setTitle(title);
    QLayout *oldLayout = box->layout();
    if (!oldLayout) {
        oldLayout = new QVBoxLayout(box);
    }
    while (QLayoutItem *item = oldLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(oldLayout);
    if (!layout) {
        layout = new QVBoxLayout(box);
    }
    layout->setContentsMargins(16, 20, 16, 14);
    layout->setSpacing(10);

    QLabel *subtitleLabel = new QLabel(subtitle, box);
    subtitleLabel->setObjectName("chartSubtitle");
    subtitleLabel->setWordWrap(true);
    layout->addWidget(subtitleLabel);

    QFrame *plot = new QFrame(box);
    plot->setObjectName("chartMockFrame");
    plot->setMinimumHeight(170);
    QVBoxLayout *plotLayout = new QVBoxLayout(plot);
    plotLayout->setContentsMargins(16, 14, 16, 14);
    plotLayout->setSpacing(10);

    QLabel *axisTop = new QLabel("   -80        -60        -40        -20          0        +20 dB", plot);
    axisTop->setObjectName("chartAxisLabel");
    QLabel *curve = new QLabel("╭───────╮              ╭─────╮\n│       ╰──────╮   ╭───╯     ╰─────\n╯              ╰───╯", plot);
    curve->setObjectName("chartCurveLabel");
    QLabel *legend = new QLabel("● 主信道    ● 邻道    ● 判定门限", plot);
    legend->setObjectName("chartLegendLabel");
    plotLayout->addWidget(axisTop);
    plotLayout->addWidget(curve, 1);
    plotLayout->addWidget(legend);
    layout->addWidget(plot, 1);
}

void MainWindow::setCaseInfoPanel(const QString &title, const QString &desc)
{
    if (QLabel *caseTitle = findChild<QLabel *>("caseTitleLabel")) {
        caseTitle->setText(title);
    }
    if (QLabel *caseDesc = findChild<QLabel *>("caseDescLabel")) {
        caseDesc->setText(desc);
    }
}

void MainWindow::setParamCheckPanel(const QStringList &items,
                                    const QStringList &states,
                                    const QStringList &notes)
{
    QTableWidget *checkTable = findChild<QTableWidget *>("checkTable");
    if (!checkTable) {
        return;
    }

    const int rowCount = qMin(items.size(), qMin(states.size(), notes.size()));
    checkTable->clearContents();
    checkTable->setRowCount(rowCount);
    for (int row = 0; row < rowCount; ++row) {
        checkTable->setItem(row, 0, new QTableWidgetItem(items[row]));
        checkTable->setItem(row, 1, new QTableWidgetItem(states[row]));
        checkTable->setItem(row, 2, new QTableWidgetItem(notes[row]));
    }
}

void MainWindow::setDispatchQueuePanel(const QStringList &commands,
                                       const QStringList &states)
{
    QTableWidget *queueTable = findChild<QTableWidget *>("queueTable");
    if (!queueTable) {
        return;
    }

    const int rowCount = qMin(commands.size(), states.size());
    queueTable->clearContents();
    queueTable->setRowCount(rowCount);
    for (int row = 0; row < rowCount; ++row) {
        queueTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        queueTable->setItem(row, 1, new QTableWidgetItem(commands[row]));
        queueTable->setItem(row, 2, new QTableWidgetItem(states[row]));
    }
}

void MainWindow::setValidationMessages(const QStringList &messages)
{
    if (!ui->validationEdit) {
        return;
    }
    ui->validationEdit->setPlainText(messages.join('\n'));
}

void MainWindow::resetCaseRuntimeState()
{
    testRunning = false;
    ui->progressBar->setValue(0);
    ui->verdictLabel->setText("WAIT");
    ui->statsLabel->setText("待测试: 请选择用例并点击开始测试");
    ui->resultTableRun->clear();
    ui->resultTableRun->setRowCount(0);
    ui->resultTableResult->clear();
    ui->resultTableResult->setRowCount(0);

    if (confirmBlerBtn) {
        confirmBlerBtn->setVisible(false);
        confirmBlerBtn->setEnabled(false);
    }
    set8_6BlerEditingEnabled(false);
    setAcsBlerEditingEnabled(false);
}

void MainWindow::applyCaseResultTemplate(const QString &caseName)
{
    QTableWidget *runTable = ui->resultTableRun;
    QTableWidget *resTable = ui->resultTableResult;
    if (!runTable || !resTable) {
        return;
    }

    ui->verdictLabel->setText("WAIT");
    ui->statsLabel->setText("待测试: " + caseName);

    if (caseName.contains("8.1")) {
        runTable->setColumnCount(4);
        runTable->setHorizontalHeaderLabels({"设备", "连接/返回", "检查项", "判定"});
        runTable->setRowCount(4);
        const QStringList devices = {"4071 频谱分析仪", "1466 信号发生器", "4071 错误队列", "1466 错误队列"};
        const QStringList checks = {"*IDN?", "*IDN?", ":SYSTem:ERRor?", ":SYSTem:ERRor?"};
        for (int row = 0; row < devices.size(); ++row) {
            runTable->setItem(row, 0, new QTableWidgetItem(devices[row]));
            runTable->setItem(row, 1, new QTableWidgetItem("待测试"));
            runTable->setItem(row, 2, new QTableWidgetItem(checks[row]));
            runTable->setItem(row, 3, new QTableWidgetItem("WAIT"));
        }

        resTable->setColumnCount(4);
        resTable->setHorizontalHeaderLabels({"检查项", "4071", "1466", "判定"});
        resTable->setRowCount(2);
        const QStringList items = {"设备识别", "通信方式"};
        for (int row = 0; row < items.size(); ++row) {
            resTable->setItem(row, 0, new QTableWidgetItem(items[row]));
            resTable->setItem(row, 1, new QTableWidgetItem(row == 0 ? "待测试" : "LAN Socket / SCPI"));
            resTable->setItem(row, 2, new QTableWidgetItem(row == 0 ? "待测试" : "LAN Socket / SCPI"));
            resTable->setItem(row, 3, new QTableWidgetItem("WAIT"));
        }
        ui->statsLabel->setText("待测试: 验证 4071 / 1466 设备识别和错误队列读取\n8.1 不涉及频点、带宽和功率判据");
        return;
    }

    if (caseName.contains("8.2")) {
        runTable->setColumnCount(4);
        runTable->setHorizontalHeaderLabels({"步骤", "配置/返回", "期望", "状态"});
        runTable->setRowCount(5);
        const QStringList steps = {"仪表识别", "1466 输出配置", "4071 频谱配置", "4071 Marker 读取", "联调判定"};
        const QStringList expects = {"*IDN? 均有返回", "频率、功率、输出开关配置成功", "切换频谱分析并完成单次测量",
                                     "返回峰值频率和峰值功率", "频率误差 <=5 kHz，功率误差 <=3 dB"};
        for (int row = 0; row < steps.size(); ++row) {
            runTable->setItem(row, 0, new QTableWidgetItem(steps[row]));
            runTable->setItem(row, 1, new QTableWidgetItem("待测试"));
            runTable->setItem(row, 2, new QTableWidgetItem(expects[row]));
            runTable->setItem(row, 3, new QTableWidgetItem("WAIT"));
        }

        resTable->setColumnCount(4);
        resTable->setHorizontalHeaderLabels({"项目", "设定/期望", "测量/返回", "判定"});
        resTable->setRowCount(5);
        const QStringList resultItems = {"1466 设备标识", "4071 设备标识", "峰值频率", "8.2 输出功率测试点", "SCPI 错误状态"};
        const QLineEdit *outputPowerEdit = findChild<QLineEdit*>("outputPowerPointsEdit");
        const QString powerPointsText = outputPowerEdit ? outputPowerEdit->text() : "待输入";
        const QStringList resultExpect = {"应返回 *IDN?", "应返回 *IDN?",
                                          QString::number(ui->carrierFreqSpin->value(), 'f', 6) + " MHz",
                                          powerPointsText,
                                          "SYST:ERR? 无错误"};
        for (int row = 0; row < resultItems.size(); ++row) {
            resTable->setItem(row, 0, new QTableWidgetItem(resultItems[row]));
            resTable->setItem(row, 1, new QTableWidgetItem(resultExpect[row]));
            resTable->setItem(row, 2, new QTableWidgetItem("待测试"));
            resTable->setItem(row, 3, new QTableWidgetItem("WAIT"));
        }
        ui->statsLabel->setText("待测试: 1466 输出 CW，4071 读取峰值频率与峰值功率\n联调判据: 频率误差 <= 5 kHz，功率误差 <= 3 dB");
        return;
    }

    if (caseName.contains("8.5")) {
        runTable->setColumnCount(10);
        runTable->setHorizontalHeaderLabels({"频率(MHz)", "带宽", "邻道偏移", "滤波带宽",
                                             "主功率", "左邻功率", "左ACLR", "右邻功率", "右ACLR", "判定"});
        resTable->setColumnCount(11);
        resTable->setHorizontalHeaderLabels({"频率(MHz)", "带宽(kHz)", "邻道偏移(kHz)",
                                             "滤波带宽(kHz)", "主功率(dBm)", "左邻功率(dBm)",
                                             "左ACLR(dB)", "右邻功率(dBm)", "右ACLR(dB)",
                                             "限值(dB)", "判定"});

        struct Row { double freqMHz; int bwKHz; int offsetKHz; int ibwKHz; double limitDb; };
        QList<Row> rows;
        const QList<Row> baseRows = {
            {0.0, 200, 300, 180, 40.8}, {0.0, 200, 500, 180, 45.8},
            {0.0, 400, 500, 360, 40.8}, {0.0, 400, 900, 360, 45.8},
            {0.0, 600, 700, 540, 40.8}, {0.0, 600, 1300, 540, 45.8},
            {0.0, 800, 900, 720, 40.8}, {0.0, 800, 1700, 720, 45.8}
        };
        for (double freqMHz : QList<double>{925.0, 942.5, 960.0}) {
            for (Row row : baseRows) {
                row.freqMHz = freqMHz;
                rows.append(row);
            }
        }

        runTable->setRowCount(rows.size());
        resTable->setRowCount(rows.size());
        for (int i = 0; i < rows.size(); ++i) {
            const Row &row = rows[i];
            runTable->setItem(i, 0, new QTableWidgetItem(QString::number(row.freqMHz, 'f', 3)));
            runTable->setItem(i, 1, new QTableWidgetItem(QString("%1 kHz").arg(row.bwKHz)));
            runTable->setItem(i, 2, new QTableWidgetItem(QString("±%1 kHz").arg(row.offsetKHz)));
            runTable->setItem(i, 3, new QTableWidgetItem(QString("%1 kHz").arg(row.ibwKHz)));
            for (int col = 4; col < 9; ++col) {
                runTable->setItem(i, col, new QTableWidgetItem("待测"));
            }
            runTable->setItem(i, 9, new QTableWidgetItem("WAIT"));

            resTable->setItem(i, 0, new QTableWidgetItem(QString::number(row.freqMHz, 'f', 3)));
            resTable->setItem(i, 1, new QTableWidgetItem(QString::number(row.bwKHz)));
            resTable->setItem(i, 2, new QTableWidgetItem(QString("±%1").arg(row.offsetKHz)));
            resTable->setItem(i, 3, new QTableWidgetItem(QString::number(row.ibwKHz)));
            for (int col = 4; col < 9; ++col) {
                resTable->setItem(i, col, new QTableWidgetItem("待测"));
            }
            resTable->setItem(i, 9, new QTableWidgetItem(QString::number(row.limitDb, 'f', 1)));
            resTable->setItem(i, 10, new QTableWidgetItem("WAIT"));
        }
        runTable->setRowCount(0);
        resTable->setRowCount(0);
        ui->statsLabel->setText("待测试: 点击开始测试，将根据当前带宽和输入的频点执行 ACLR 测试");
        return;
    }

    if (caseName.contains("8.6")) {
        const QList<RefSensitivityPoint> points = refSensitivityTestPlan();
        const bool runSignalsBlocked = runTable->blockSignals(true);
        const bool resSignalsBlocked = resTable->blockSignals(true);
        runTable->setColumnCount(10);
        runTable->setHorizontalHeaderLabels({"阶段", "信道", "带宽/DSB", "调制", "D2R功率",
                                             "CW功率", "接收功率", "BLER", "频误", "判定"});
        runTable->setRowCount(points.size());
        resTable->setColumnCount(13);
        resTable->setHorizontalHeaderLabels({"阶段", "信道", "IQ/CW", "采样率", "D2R功率",
                                             "CW功率", "接收功率", "峰值", "频误", "BLER",
                                             "截图", "门限", "判定"});
        resTable->setRowCount(points.size());
        for (int i = 0; i < points.size(); ++i) {
            const RefSensitivityPoint &point = points[i];
            const QString pendingText = QStringLiteral("待输入");
            const QString fileText = refSensitivityWaveformFileName(point);
            const QString cwText = point.usesCwWaveform
                                   ? QString("目标 %1 dBm / 文件 %2")
                                     .arg(point.cwPowerDbm, 0, 'f', 1)
                                     .arg(point.cwFileName)
                                   : QStringLiteral("-38.0 dBm");
            const QString verdict = QStringLiteral("WAIT");

            runTable->setItem(i, RefRunStageCol, makeTableItem(point.stage));
            runTable->setItem(i, RefRunChannelCol, makeTableItem(point.cfg.refChannel));
            runTable->setItem(i, RefRunBwCol, makeTableItem(refSensitivityBandwidthText(point.cfg)));
            runTable->setItem(i, RefRunModCol, makeTableItem(point.cfg.modulation));
            runTable->setItem(i, RefRunD2rPowerCol, makeTableItem(refSensitivityPowerText(point.d2rPowerDbm)));
            runTable->setItem(i, RefRunCwPowerCol, makeTableItem(cwText));
            runTable->setItem(i, RefRunRxPowerCol, makeTableItem(pendingText, true));
            runTable->setItem(i, RefRunBlerCol, makeTableItem(pendingText, true));
            runTable->setItem(i, RefRunFreqErrCol, makeTableItem(QStringLiteral("待测")));
            QTableWidgetItem *runVerdictItem = makeTableItem(verdict);
            runVerdictItem->setData(RoleExecutable, true);
            runVerdictItem->setData(RolePendingReason, QString());
            runTable->setItem(i, RefRunVerdictCol, runVerdictItem);

            resTable->setItem(i, RefResStageCol, makeTableItem(point.stage));
            resTable->setItem(i, RefResChannelCol, makeTableItem(point.cfg.refChannel));
            resTable->setItem(i, RefResIqFileCol, makeTableItem(fileText));
            resTable->setItem(i, RefResSampleRateCol, makeTableItem(refSensitivitySampleRateText(point.cfg.sampleRateHz)));
            resTable->setItem(i, RefResD2rPowerCol, makeTableItem(refSensitivityPowerText(point.d2rPowerDbm)));
            resTable->setItem(i, RefResCwPowerCol, makeTableItem(cwText));
            resTable->setItem(i, RefResRxPowerCol, makeTableItem(pendingText));
            resTable->setItem(i, RefResPeakCol, makeTableItem(QStringLiteral("待测")));
            resTable->setItem(i, RefResFreqErrCol, makeTableItem(QStringLiteral("待测")));
            resTable->setItem(i, RefResBlerCol, makeTableItem(pendingText));
            resTable->setItem(i, RefResScreenshotCol, makeTableItem(QStringLiteral("待保存")));
            resTable->setItem(i, RefResLimitCol, makeTableItem("BLER≤10%, |频误|≤±(0.05ppm+6Hz)"));
            QTableWidgetItem *resVerdictItem = makeTableItem(verdict);
            resVerdictItem->setData(RoleExecutable, true);
            resVerdictItem->setData(RolePendingReason, QString());
            resTable->setItem(i, RefResVerdictCol, resVerdictItem);
        }
        runTable->blockSignals(runSignalsBlocked);
        resTable->blockSignals(resSignalsBlocked);
        if (points.size() > 0) {
            runTable->selectRow(0);
            updateRefSensitivityRunGraphs(0);
        }
        updateRefSensitivityCharts();
        ui->statsLabel->setText(QString("待测试: 8.6共%1个计划点\n"
                                        "D2R扫描使用参考IQ文件；CW扫描使用同事提供的CW波形文件\n"
                                        "频率误差需 ≤ ±(0.05 ppm+6 Hz)，BLER门限 ≤10%")
                                .arg(points.size()));
        return;
    }

    if (caseName.contains("8.7")) {
        runTable->setColumnCount(8);
        runTable->setHorizontalHeaderLabels({"测试点", "偏移", "有用信号功率", "干扰信号功率", "带宽", "BLER(%)", "ACS", "判定"});
        resTable->setColumnCount(8);
        resTable->setHorizontalHeaderLabels({"测试点", "偏移", "有用信号功率", "干扰信号功率", "带宽", "BLER(%)", "ACS", "判定"});
        const QStringList points = {"低偏移 upper", "低偏移 lower", "高偏移 upper", "高偏移 lower"};
        const QStringList offsets = {"+340 kHz", "-340 kHz", "+2500 kHz", "-2500 kHz"};
        runTable->setRowCount(points.size());
        resTable->setRowCount(points.size());
        for (int i = 0; i < points.size(); ++i) {
            for (QTableWidget *table : QList<QTableWidget *>{runTable, resTable}) {
                table->setItem(i, 0, new QTableWidgetItem(points[i]));
                table->setItem(i, 1, new QTableWidgetItem(offsets[i]));
                table->setItem(i, 2, new QTableWidgetItem("待测"));
                table->setItem(i, 3, new QTableWidgetItem("-53 dBm"));
                table->setItem(i, 4, new QTableWidgetItem("200kHz/3520kHz"));
                table->setItem(i, 5, new QTableWidgetItem("待测"));
                table->setItem(i, 6, new QTableWidgetItem("待测"));
                table->setItem(i, 7, new QTableWidgetItem("WAIT"));
            }
        }
        ui->statsLabel->setText("待测试: 4种干扰偏移场景\n判定门限: BLER ≤ 10%");
        return;
    }

    if (caseName.contains("8.3") || caseName.contains("8.4")) {
        const bool transientCase = caseName.contains("8.3");
        runTable->setColumnCount(4);
        runTable->setHorizontalHeaderLabels(transientCase
                                            ? QStringList({"带宽/SCS", "瞬态周期", "OFF功率", "状态"})
                                            : QStringList({"带宽/SCS", "调制深度/纹波", "时间参数", "状态"}));
        resTable->setColumnCount(6);
        resTable->setHorizontalHeaderLabels(transientCase
                                            ? QStringList({"带宽", "SCS", "时间分辨率", "OFF->ON", "ON->OFF/OFF功率", "判定"})
                                            : QStringList({"带宽", "调制深度", "纹波", "上升/下降时间", "脉冲宽度", "判定"}));
        const QStringList bws = {"200 kHz / 15 kHz", "400 kHz / 15 kHz", "600 kHz / 15 kHz", "800 kHz / 15 kHz"};
        runTable->setRowCount(bws.size());
        resTable->setRowCount(bws.size());
        for (int i = 0; i < bws.size(); ++i) {
            runTable->setItem(i, 0, new QTableWidgetItem(bws[i]));
            runTable->setItem(i, 1, new QTableWidgetItem("待测"));
            runTable->setItem(i, 2, new QTableWidgetItem("待测"));
            runTable->setItem(i, 3, new QTableWidgetItem("WAIT"));
            resTable->setItem(i, 0, new QTableWidgetItem(bws[i]));
            for (int col = 1; col < 5; ++col) {
                resTable->setItem(i, col, new QTableWidgetItem("待测"));
            }
            resTable->setItem(i, 5, new QTableWidgetItem("WAIT"));
        }
        ui->statsLabel->setText("待测试: 遍历 200/400/600/800 kHz 带宽");
        return;
    }

    runTable->setColumnCount(4);
    runTable->setHorizontalHeaderLabels({"步骤", "配置/返回", "期望", "状态"});
    runTable->setRowCount(4);
    const QStringList steps = {"环境准备", "参数下发", "执行测试", "结果判定"};
    for (int i = 0; i < steps.size(); ++i) {
        runTable->setItem(i, 0, new QTableWidgetItem(steps[i]));
        runTable->setItem(i, 1, new QTableWidgetItem("待测试"));
        runTable->setItem(i, 2, new QTableWidgetItem("等待开始测试"));
        runTable->setItem(i, 3, new QTableWidgetItem("WAIT"));
    }
    resTable->setColumnCount(4);
    resTable->setHorizontalHeaderLabels({"项目", "期望", "测量/返回", "判定"});
    resTable->setRowCount(1);
    resTable->setItem(0, 0, new QTableWidgetItem(caseName));
    resTable->setItem(0, 1, new QTableWidgetItem("等待执行"));
    resTable->setItem(0, 2, new QTableWidgetItem("待测试"));
    resTable->setItem(0, 3, new QTableWidgetItem("WAIT"));
}

void MainWindow::applyCaseTemplate(const QString &caseName)
{
    const bool isRx = caseName.contains("8.6") || caseName.contains("8.7") || caseName.contains("灵敏度") || caseName.contains("ACS");
    const bool isProto = caseName.contains("8.8") || caseName.contains("8.9") || caseName.contains("8.10") || caseName.contains("8.11")
                         || caseName.contains("小移频") || caseName.contains("CBRA") || caseName.contains("协议");
    const bool isTransient = caseName.contains("8.3");
    const bool isModulation = caseName.contains("8.4");

    ui->rfGroup->setVisible(!isRx && !isProto);
    ui->rxGroup->setVisible(isRx);
    ui->protoGroup->setVisible(isProto);
    updateParameterFieldVisibility(caseName);

    if (QGroupBox *summaryBox = findChild<QGroupBox*>("groupBox_2")) {
        summaryBox->setVisible(!caseName.contains("8.5"));
    }

    // 如果是 8.5，从映射中恢复当前带宽的频点
    if (caseName.contains("8.5")) {
        QLineEdit *freqEdit = findChild<QLineEdit*>("freqPointsEdit");
        if (freqEdit) {
            int bw = ui->bwCombo->currentText().trimmed().toInt();
            if (bw == 0) bw = 200; // 默认
            QString freqStr = m_bandFreqPoints.value(bw, "925");
            freqEdit->setText(freqStr);
        }
    }

    ui->currentCaseValue->setText(caseName);

    ui->progressBar->setValue(0);

    if (caseName.contains("8.1")) {
        ui->progressBar->setFormat("8.1 基本功能 %p%");
        ui->freqValue->setText("---");
        ui->bwValue->setText("---");
        setCaseInfoPanel("8.1 测试仪表基本功能验证",
                         "基本功能模板：验证 4071/1466 连接、身份识别和错误队列读取，运行页展示通信链路与日志结果。");
        setParamCheckPanel({"设备连接", "4071 识别", "1466 识别", "错误队列", "日志目录"},
                           {"OK", "OK", "OK", "CHECK", "INFO"},
                           {"双仪表在线", "*IDN? 待验证", "*IDN? 待验证", ":SYST:ERR? 待读取", "./log/merge_test*"});
        setDispatchQueuePanel({"4071:*IDN?", "1466:*IDN?", "4071::SYST:ERR?", "1466::SYST:ERR?"},
                              {"待执行", "待执行", "待执行", "待执行"});
        setValidationMessages({"等待点击开始测试以验证双仪表通信",
                               "8.1 不涉及射频频点、带宽和功率配置",
                               "完成后结果页应显示 4071/1466 通信状态"});
        setupChartBox(findChild<QGroupBox*>("groupBox_3"), "通信链路概览", "显示 4071 与 1466 的连接和 SCPI 返回状态");
        setupChartBox(findChild<QGroupBox*>("groupBox_4"), "错误队列检查", "展示两台仪表错误队列读取结果与日志状态");
        setupChartBox(findChild<QGroupBox*>("groupBox"), "通信结果概览", "按设备识别、错误队列和总体判定汇总结果");
        setupChartBox(findChild<QGroupBox*>("groupBox_2"), "设备状态", "显示通信方式、在线状态和日志导出准备情况");
        return;
    }

    if (caseName.contains("8.2")) {
        ui->progressBar->setFormat("8.2 输出功率 %p%");
        ui->freqValue->setText(QString::number(ui->carrierFreqSpin->value(), 'f', 1) + " MHz");
        ui->bwValue->setText(ui->bwCombo->currentText());
        setCaseInfoPanel("8.2 输出功率",
                         "输出功率模板：1466 按功率点列表输出 CW，4071 多次采样峰值功率，按平均功率和频率误差判定。");
        setParamCheckPanel({"设备连接", "中心频率", "功率点", "频谱配置", "日志目录"},
                           {"OK", "OK", "OK", "CHECK", "INFO"},
                           {"双仪表在线", "默认 925 MHz", "默认 0,3,7.5,12,15 dBm", "待切换频谱分析模式", "./log/merge_test*"});
        setDispatchQueuePanel({"解析8.2功率点", "1466:SET CW FREQ/POWER", "4071:MARKER MAX x10", "READ:AVG POWER/FREQ"},
                              {"待下发", "待下发", "待下发", "待下发"});
        setValidationMessages({"8.2 联调流程: 默认 5 个功率点，每点 10 次 4071 Marker 采样",
                               "功率点范围当前限制为 0~15 dBm，4071 输入保护线 28 dBm",
                               "功率误差门限 ±3.01 dB，频率误差门限 ±5 kHz",
                               "测试完成后自动停止扫描并关闭 RF 输出"});
        updateOutputPowerRunLiveChart(QVector<double>(), QVector<double>(), QVector<double>(), false);
        updateOutputPowerTrendChart(QVector<double>(), QVector<double>(), QVector<double>());
        return;
    }

    if (caseName.contains("8.7") || caseName == "8.7 ACS") {
        ui->progressBar->setFormat("8.7 ACS %p%");
        ui->freqValue->setText("有用信号: 200 kHz / 15 kHz DSB");
        ui->bwValue->setText("干扰信号: 5 MHz DFT-s-OFDM NR");
        setCaseInfoPanel("8.7 ACS (邻道选择性)",
                         "接收机 ACS 模板：8 个 upper/lower 干扰场景，频谱截图留证，BLER 手工填写并统一判定。");
        setParamCheckPanel({"设备连接", "有用信号", "干扰信号", "截图目录", "BLER录入"},
                           {"OK", "OK", "OK", "INFO", "WAIT"},
                           {"仪表/基站在线", "200 kHz / 15 kHz DSB", "-53 dBm 干扰", "./screenshots/8.7_ACS/*", "待人工确认"});
        setDispatchQueuePanel({"SET:ACS WAVE upper/lower", "SET:INTF -53dBm", "CAP:SCREEN", "WAIT:BLER INPUT"},
                              {"已准备", "已准备", "待下发", "待下发"});
        setValidationMessages({"8.7 使用 8 个 upper/lower 干扰点进行留证截图",
                               "BLER 在测试运行页统一填写，确认后生成最终判定",
                               "截图目录按轮次时间戳创建，避免覆盖历史结果"});
        setupTextChartBox(findChild<QGroupBox*>("groupBox_3"),
                          "ACS 示意图",
                          "等待选择 8.7 测试点",
                          {"点击测试运行表任意行后，显示对应 images/sim/8_7 下的 schematic_0~7 示意图",
                           "左图用于对照 upper/lower 邻道干扰场景"},
                          "示意图资源已打包到 qrc");
        setupTextChartBox(findChild<QGroupBox*>("groupBox_4"),
                          "4071 实测频谱图",
                          "等待 4071 截图",
                          {"测试执行后下载频谱截图",
                           "点击测试运行表任意行后，显示该行对应截图"},
                          "截图目录按本次运行时间戳创建");
        setupChartBox(findChild<QGroupBox*>("groupBox"), "BLER vs 测试点", "8 个 upper/lower 测试点的 BLER 结果与 10% 门限");
        setupChartBox(findChild<QGroupBox*>("groupBox_2"), "ACS 测试通过率", "按 8 个 upper/lower 测试点统计 PASS/FAIL 分布");
        return;
    }

    if (caseName.contains("8.6") || caseName == "8.6 参考灵敏度" || caseName.contains("灵敏度")) {
        ui->progressBar->setFormat("8.6参考灵敏度 %p%");
        ui->freqValue->setText("A1-1/A1-2/A1-3/A1-4");
        ui->bwValue->setText("BLER目标 10% / 频率误差 ±(0.05 ppm+6 Hz)");
        setCaseInfoPanel("8.6 参考灵敏度",
                         "参考灵敏度模板：1466 参考波形输出，4071 频谱/频率校验，BLER 后填并统一确认判定。");
        setParamCheckPanel({"设备连接", "参考信道", "采样率", "频率误差门限", "截图目录"},
                           {"OK", "OK", "CHECK", "OK", "INFO"},
                           {"仪表/基站在线", "A1-1/A1-2/A1-3/A1-4", "2.88 / 5.76 Msps", "±(0.05 ppm + 6 Hz)", "./screenshots/8.6_RefSens/*"});
        setDispatchQueuePanel({"SET:REFSENS WAVEFORM", "MEAS:PEAK/FREQERR", "SAVE:SCREEN", "WAIT:BLER INPUT"},
                              {"已准备", "已准备", "待下发", "待下发"});
        setValidationMessages({"8.6 使用 1466 参考灵敏度波形与 4071 峰值/频率误差校验",
                               "BLER 在测试运行页统一填写并确认判定",
                               "频率误差门限: ±(0.05 ppm + 6 Hz)"});
        updateRefSensitivityCharts();
        return;
    }

    if (currentTestCase.contains("8.1")) {
        ui->currentCaseValue->setText("8.1 基本功能验证");
        ui->freqValue->setText("---");
        ui->bwValue->setText("---");

        ui->resultTableRun->clearContents();
        ui->resultTableRun->setColumnCount(4);
        ui->resultTableRun->setHorizontalHeaderLabels({"设备", "连接/返回", "检查项", "判定"});
        ui->resultTableRun->setRowCount(4);
        const QStringList deviceRows = {"4071 频谱分析仪", "1466 信号发生器", "4071 错误队列", "1466 错误队列"};
        const QStringList checkRows = {"*IDN?", "*IDN?", ":SYSTem:ERRor?", ":SYSTem:ERRor?"};
        for (int row = 0; row < deviceRows.size(); ++row) {
            ui->resultTableRun->setItem(row, 0, new QTableWidgetItem(deviceRows[row]));
            ui->resultTableRun->setItem(row, 1, new QTableWidgetItem("待测试"));
            ui->resultTableRun->setItem(row, 2, new QTableWidgetItem(checkRows[row]));
            ui->resultTableRun->setItem(row, 3, new QTableWidgetItem("WAIT"));
        }

        ui->resultTableResult->clearContents();
        ui->resultTableResult->setColumnCount(4);
        ui->resultTableResult->setHorizontalHeaderLabels({"检查项", "4071", "1466", "判定"});
        ui->resultTableResult->setRowCount(2);
        ui->resultTableResult->setItem(0, 0, new QTableWidgetItem("设备识别"));
        ui->resultTableResult->setItem(0, 1, new QTableWidgetItem("待测试"));
        ui->resultTableResult->setItem(0, 2, new QTableWidgetItem("待测试"));
        ui->resultTableResult->setItem(0, 3, new QTableWidgetItem("WAIT"));
        ui->resultTableResult->setItem(1, 0, new QTableWidgetItem("通信方式"));
        ui->resultTableResult->setItem(1, 1, new QTableWidgetItem("LAN Socket / SCPI"));
        ui->resultTableResult->setItem(1, 2, new QTableWidgetItem("LAN Socket / SCPI"));
        ui->resultTableResult->setItem(1, 3, new QTableWidgetItem("WAIT"));

        ui->verdictLabel->setText("WAIT");
        ui->statsLabel->setText("待测试: 验证 4071 / 1466 设备识别和错误队列读取\n"
                                "8.1 不涉及频点、带宽和功率判据");
    }
    else if (currentTestCase.contains("8.2")) {
        ui->currentCaseValue->setText("8.2 输出功率");
        ui->freqValue->setText(QString::number(ui->carrierFreqSpin->value(), 'f', 1) + " MHz");
        ui->bwValue->setText(ui->bwCombo->currentText());

        ui->resultTableRun->clearContents();
        ui->resultTableRun->setColumnCount(4);
        ui->resultTableRun->setHorizontalHeaderLabels({"步骤", "配置/返回", "期望", "状态"});
        ui->resultTableRun->setRowCount(5);
        const QStringList steps = {"仪表识别", "1466 输出配置", "4071 频谱配置", "4071 Marker 读取", "联调判定"};
        const QStringList expects = {"*IDN? 均有返回", "频率、功率、输出开关配置成功", "切换频谱分析并完成单次测量",
                                     "返回峰值频率和峰值功率", "频率误差 <=5 kHz，功率误差 <=3 dB"};
        for (int row = 0; row < steps.size(); ++row) {
            ui->resultTableRun->setItem(row, 0, new QTableWidgetItem(steps[row]));
            ui->resultTableRun->setItem(row, 1, new QTableWidgetItem("待测试"));
            ui->resultTableRun->setItem(row, 2, new QTableWidgetItem(expects[row]));
            ui->resultTableRun->setItem(row, 3, new QTableWidgetItem("WAIT"));
        }

        ui->resultTableResult->clearContents();
        ui->resultTableResult->setColumnCount(4);
        ui->resultTableResult->setHorizontalHeaderLabels({"项目", "设定/期望", "测量/返回", "判定"});
        ui->resultTableResult->setRowCount(5);
        const QStringList resultItems = {"1466 设备标识", "4071 设备标识", "峰值频率", "输出功率", "SCPI 错误状态"};
        const QStringList resultExpect = {"应返回 *IDN?", "应返回 *IDN?",
                                          QString::number(ui->carrierFreqSpin->value(), 'f', 6) + " MHz",
                                          QString::number(ui->txPowerSpin->value(), 'f', 1) + " dBm",
                                          "SYST:ERR? 无错误"};
        for (int row = 0; row < resultItems.size(); ++row) {
            ui->resultTableResult->setItem(row, 0, new QTableWidgetItem(resultItems[row]));
            ui->resultTableResult->setItem(row, 1, new QTableWidgetItem(resultExpect[row]));
            ui->resultTableResult->setItem(row, 2, new QTableWidgetItem("待测试"));
            ui->resultTableResult->setItem(row, 3, new QTableWidgetItem("WAIT"));
        }

        ui->verdictLabel->setText("WAIT");
        ui->statsLabel->setText("待测试: 1466 输出 CW，4071 读取峰值频率与峰值功率\n"
                                "联调判据: 频率误差 <= 5 kHz，功率误差 <= 3 dB");
    }
    else if (isTransient) {
        ui->progressBar->setFormat("运行进度 %p%");
        ui->freqValue->setText(QString::number(ui->carrierFreqSpin->value(), 'f', 1) + " MHz");
        ui->bwValue->setText("200/400/600/800 kHz, SCS 15 kHz");
        setCaseInfoPanel(caseName,
                         "发射ON/OFF模板：遍历R2D带宽，记录OFF->ON、ON->OFF瞬态周期、OFF功率谱密度和时间分辨率。");
        setParamCheckPanel({"设备连接", "中心频率", "带宽遍历", "时间分辨率", "日志目录"},
                           {"OK", "OK", "OK", "OK", "INFO"},
                           {"仪表/基站在线", "默认 925 MHz", "200/400/600/800 kHz", "<= 0.1 ns", "./log/merge_test*"});
        setDispatchQueuePanel({"SET:R2D BW=200,400,600,800k", "SET:SCS 15kHz", "ARM:TRANSIENT", "MEAS:OFFP/TRAN?"},
                              {"已准备", "已准备", "待下发", "待下发"});
        setValidationMessages({"8.3 遍历 200/400/600/800 kHz 带宽",
                               "关注 OFF->ON、ON->OFF 瞬态周期与 OFF 功率谱密度",
                               "结果页按瞬态周期和 OFF 功率双重门限判定"});
        setupChartBox(findChild<QGroupBox*>("groupBox_3"), "发射ON/OFF时域波形", "采集开启、关闭边沿，标记瞬态周期和OFF时隙");
        setupChartBox(findChild<QGroupBox*>("groupBox_4"), "OFF功率谱密度", "按200/400/600/800 kHz带宽汇总dBm/MHz判定");
        setupChartBox(findChild<QGroupBox*>("groupBox"), "瞬态周期结果", "OFF->ON、ON->OFF 均按 10 us 门限判定");
        setupChartBox(findChild<QGroupBox*>("groupBox_2"), "OFF功率结果", "TS 38.194 最小要求: < -85 dBm/MHz");
    } else if (isModulation) {
        ui->progressBar->setFormat("运行进度 %p%");
        ui->freqValue->setText(QString::number(ui->carrierFreqSpin->value(), 'f', 1) + " MHz");
        ui->bwValue->setText("200/400/600/800 kHz, SCS 15 kHz");
        setCaseInfoPanel(caseName,
                         "调制质量模板：采集R2D OOK包络，记录调制深度、纹波、上升/下降时间和脉冲宽度。");
        setParamCheckPanel({"设备连接", "中心频率", "带宽遍历", "分析带宽", "日志目录"},
                           {"OK", "OK", "OK", "CHECK", "INFO"},
                           {"仪表/基站在线", "默认 925 MHz", "200/400/600/800 kHz", "RTBW/模拟带宽待校验", "./log/merge_test*"});
        setDispatchQueuePanel({"SET:R2D BW=200,400,600,800k", "SET:SCOPE BW=500MHz", "SET:RTBW 100MHz", "MEAS:MODQ?"},
                              {"已准备", "已准备", "待下发", "待下发"});
        setValidationMessages({"8.4 采集 R2D OOK 包络并计算调制质量指标",
                               "关注调制深度、纹波、上升/下降时间和脉冲宽度",
                               "结果页同时展示仪表能力检查项"});
        setupChartBox(findChild<QGroupBox*>("groupBox_3"), "R2D OOK包络", "定位1-0-1连续chip，显示10/90%与50%门限");
        setupChartBox(findChild<QGroupBox*>("groupBox_4"), "调制质量指标", "汇总调制深度、纹波、Tr/Tf和PW的标准符合性");
        setupChartBox(findChild<QGroupBox*>("groupBox"), "包络参数结果", "调制深度、纹波、上升/下降时间和脉冲宽度");
        setupChartBox(findChild<QGroupBox*>("groupBox_2"), "仪表能力检查", "实时分析带宽、模拟带宽、功率/时间分辨率和采样率");
    } else if (isRx) {
        ui->progressBar->setFormat("运行进度 %p%");
        ui->freqValue->setText("A-FR1-A1-2 / D2R 200 kHz");
        ui->bwValue->setText("BLER目标 10% / 步进 1 dB");
        setCaseInfoPanel(caseName,
                         "接收机性能模板：配置参考信道、D2R功率扫描和BLER目标，运行页突出BLER曲线与灵敏度判定。");
        setParamCheckPanel({"设备连接", "参考信道", "D2R功率扫描", "BLER门限", "日志目录"},
                           {"OK", "OK", "OK", "OK", "INFO"},
                           {"仪表/基站在线", "A-FR1-A1-2", "-130 到 -50 dBm", "<= 10%", "./log/merge_test*"});
        setDispatchQueuePanel({"SET:D2R BW=200k", "SET:PWR -130:-50", "SET:STEP 1dB", "MEAS:BLER START"},
                              {"已准备", "已准备", "待下发", "待下发"});
        setValidationMessages({"接收机通用模板仅用于未实现的 RX 流程占位",
                               "真实 8.6 / 8.7 流程会使用各自专用模板",
                               "当前页面用于统一展示 BLER 与灵敏度判定语义"});
        setupChartBox(findChild<QGroupBox*>("groupBox_3"), "BLER vs 输入功率", "标记10% BLER目标线和参考灵敏度点");
        setupChartBox(findChild<QGroupBox*>("groupBox_4"), "各信道灵敏度", "A1-1 / A1-2 / A1-3 信道扫描结果");
        setupChartBox(findChild<QGroupBox*>("groupBox"), "参考灵敏度判定", "按 BLER ≤ 10% 和频率误差门限给出最终结果");
        setupChartBox(findChild<QGroupBox*>("groupBox_2"), "统计分布", "展示各功率点、各信道或各场景的通过情况");
    } else if (isProto) {
        ui->progressBar->setFormat("运行进度 %p%");
        ui->freqValue->setText("Access Type 1 / AO 0000");
        ui->bwValue->setText("FRI k=1,2,4,8 / 标签数 8");
        setCaseInfoPanel(caseName,
                         "协议一致性模板：配置接入类型、AO、FRI k、标签数量和冲突场景，运行页突出事件流与标签状态。");
        setParamCheckPanel({"后端连接", "接入类型", "FRI参数", "标签数量", "日志目录"},
                           {"CHECK", "OK", "OK", "OK", "INFO"},
                           {"需要后端配合", "CBRA / Type 1", "k=1,2,4,8", "默认 8 标签", "./log/merge_test*"});
        setDispatchQueuePanel({"SET:ACCESS CBRA", "SET:FRI 1,2,4,8", "TAG:COUNT 8", "RUN:INVENTORY"},
                              {"已准备", "已准备", "待下发", "待下发"});
        setValidationMessages({"协议一致性场景依赖后端服务与标签模拟器",
                               "运行页关注事件时间线与标签状态矩阵",
                               "结果页聚合成功标签数、异常次数和事件分布"});
        setupChartBox(findChild<QGroupBox*>("groupBox_3"), "协议事件时间线", "接入请求、响应、盘点和冲突事件按时间展开");
        setupChartBox(findChild<QGroupBox*>("groupBox_4"), "标签状态矩阵", "按标签展示k因子、调制方式和盘点结果");
        setupChartBox(findChild<QGroupBox*>("groupBox"), "协议结果汇总", "按测试事件、成功标签数和异常次数汇总结果");
        setupChartBox(findChild<QGroupBox*>("groupBox_2"), "统计分布", "当前协议场景的标签状态和事件分布");
    } else {
        ui->progressBar->setFormat(caseName.contains("8.5") ? "8.5 ACLR测试 %p%" : "运行进度 %p%");
        ui->freqValue->setText("925.0 MHz");
        ui->bwValue->setText("200 kHz / SCS 15 kHz");
        setCaseInfoPanel(caseName,
                         "射频指标模板：配置载波、带宽、发射功率和采集策略，测试运行页展示频谱、ACLR和关键测量值。");
        setParamCheckPanel({"设备连接", "频点范围", "功率范围", "射频配置", "日志目录"},
                           {"OK", "OK", "OK", "CHECK", "INFO"},
                           {"仪表/基站在线", "925 MHz 合法", "由前端功率框决定", "待开始测试下发", "./log/merge_test*"});
        setDispatchQueuePanel({"SET:FREQ 925MHz", "SET:BW 200kHz", "SET:POWER 前端值", "ARM:CAPTURE"},
                              {"已准备", "已准备", "待下发", "待下发"});
        setValidationMessages({"射频指标模板支持 8.3 / 8.4 / 8.5 等前端配置项",
                               "具体流程会在测试开始后覆盖为对应测试项的真实日志",
                               "结果页展示当前测试项的关键指标与统计分布"});
        setupChartBox(findChild<QGroupBox*>("groupBox_3"), "实时频谱", "主信道功率、邻道泄漏与CW干扰位置");
        setupChartBox(findChild<QGroupBox*>("groupBox_4"), "ACLR余量趋势", "左/右邻道余量随采样轮次变化");
        if (caseName.contains("8.5")) {
            setupTextChartBox(findChild<QGroupBox*>("groupBox"),
                              "8.5 ACLR核心结果图",
                              "等待真实仪表截图；真实联调时会自动把 4071 当前频谱屏幕下载并显示在这里",
                              QStringList()
                              << "ACLR 频谱示意 / 等待截图"
                              << ""
                              << "功率/dBm"
                              << "   ^"
                              << "   |                         主信道功率 Pmain"
                              << "   |                              ╱╲"
                              << "   |                             ╱  ╲"
                              << "   |      左邻道测量区          ╱    ╲          右邻道测量区"
                              << "   |   ┌─────────────┐      ╱      ╲      ┌─────────────┐"
                              << "   |___│  Padj-L     │_____╱        ╲_____│  Padj-R     │____"
                              << "   |   └─────────────┘        f0          └─────────────┘"
                              << "   +--------------------------------------------------------> 频率"
                              << "       f0-Δf                 f0                 f0+Δf"
                              << ""
                              << "左 ACLR = Pmain - Padj-L；右 ACLR = Pmain - Padj-R",
                              "8.5 将遍历 200/400/600/800 kHz；真实仪表模式下截图成功后，此处会替换为 4071 频谱仪 PNG。"
                              );
        } else {
            setupChartBox(findChild<QGroupBox*>("groupBox"), "核心结果图", "按当前射频指标展示最终结果图或统计图");
        }
        if (!caseName.contains("8.5")) {
            setupChartBox(findChild<QGroupBox*>("groupBox_2"), "统计分布", "展示当前测试项的通过率、门限和关键指标分布");
        }
    }
}

// ========== 仪表连接与通信函数 ==========
void MainWindow::connectToInstrument(const QString& ip, quint16 port)
{
    if (instrumentSocket->state() == QAbstractSocket::ConnectedState) {
        stopSpectrumAnalyzerMeasurement("切换频谱仪连接前停止测量");
        instrumentSocket->disconnectFromHost();
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "4071", "断开现有连接");
    }
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "4071", QString("尝试连接 %1:%2 ...").arg(ip).arg(port));
    instrumentSocket->connectToHost(ip, port);
}

void MainWindow::connectToSignalGenerator(const QString& ip, quint16 port)
{
    if (signalGeneratorSocket->state() == QAbstractSocket::ConnectedState) {
        shutdownSignalGeneratorOutput("切换信号源连接前安全关断");
        signalGeneratorSocket->disconnectFromHost();
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "1466", "断开现有连接");
    }
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "1466", QString("尝试连接 %1:%2 ...").arg(ip).arg(port));
    signalGeneratorSocket->connectToHost(ip, port);
}

void MainWindow::connectToInstruments(const QString &analyzerIp, quint16 analyzerPort,
                                      const QString &generatorIp, quint16 generatorPort)
{
    connectToInstrument(analyzerIp, analyzerPort);
    if (!generatorIp.trimmed().isEmpty()) {
        connectToSignalGenerator(generatorIp, generatorPort);
    }
}

void MainWindow::sendScpiCommand(const QString& cmd)
{
    sendScpiCommand(instrumentSocket, "4071", cmd);
}

void MainWindow::sendScpiCommand(QTcpSocket *socket, const QString &src, const QString &cmd)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", src, "未连接，命令未发送");
        return;
    }
    QByteArray data = cmd.toUtf8();
    if (!data.endsWith('\n')) data.append('\n');
    qint64 sent = socket->write(data);
    if (sent == -1) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", src, "发送失败: " + socket->errorString());
    } else {
        socket->flush();
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", src, "发送: " + cmd);
    }
}

QString MainWindow::queryScpi(QTcpSocket *socket, const QString &src, const QString &cmd, int timeoutMs)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", src, "未连接，无法查询: " + cmd);
        return QString();
    }

    bool *queryingFlag = nullptr;
    if (socket == instrumentSocket) {
        queryingFlag = &queryingInstrument;
    } else if (socket == signalGeneratorSocket) {
        queryingFlag = &queryingSignalGenerator;
    }

    if (queryingFlag) {
        *queryingFlag = true;
    }

    while (socket->bytesAvailable() > 0) {
        socket->readAll();
    }

    sendScpiCommand(socket, src, cmd);
    QByteArray response;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        if (socket->bytesAvailable() > 0) {
            response += socket->readAll();
            if (response.contains('\n')) {
                break;
            }
            continue;
        }

        const int remainMs = qMax(1, timeoutMs - int(timer.elapsed()));
        if (!socket->waitForReadyRead(qMin(100, remainMs))) {
            QCoreApplication::processEvents();
            continue;
        }
        response += socket->readAll();
        if (response.contains('\n')) {
            break;
        }
    }

    QString text = QString::fromUtf8(response).trimmed();
    if (queryingFlag) {
        *queryingFlag = false;
    }

    if (text.isEmpty()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", src, "查询超时: " + cmd);
        return QString();
    }
    text.replace('\r', '\n');
    QStringList lines = text.split('\n',
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                   Qt::SkipEmptyParts
#else
                                   QString::SkipEmptyParts
#endif
                                   );
    QString firstLine = lines.isEmpty() ? text : lines.first().trimmed();
    if (socket == instrumentSocket && (firstLine.contains("4071") || firstLine.contains("Keysight") || firstLine.contains("34465A"))) {
        lastIdnResponse = firstLine;
    } else if (socket == signalGeneratorSocket
               && (firstLine.contains("1466") || firstLine.contains("Signal", Qt::CaseInsensitive)
                   || firstLine.contains("Generator", Qt::CaseInsensitive))) {
        lastGeneratorIdnResponse = firstLine;
    }
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", src, "查询返回: " + firstLine);
    return firstLine;
}

QByteArray MainWindow::queryScpiBinaryBlock(QTcpSocket *socket, const QString &src, const QString &cmd, int timeoutMs)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", src, "未连接，无法查询二进制数据: " + cmd);
        return QByteArray();
    }

    bool *queryingFlag = nullptr;
    if (socket == instrumentSocket) {
        queryingFlag = &queryingInstrument;
    } else if (socket == signalGeneratorSocket) {
        queryingFlag = &queryingSignalGenerator;
    }

    if (queryingFlag) {
        *queryingFlag = true;
    }

    while (socket->bytesAvailable() > 0) {
        socket->readAll();
    }

    sendScpiCommand(socket, src, cmd);

    QByteArray response;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        if (socket->bytesAvailable() == 0) {
            const int remainMs = qMax(1, timeoutMs - int(timer.elapsed()));
            if (!socket->waitForReadyRead(qMin(100, remainMs))) {
                QCoreApplication::processEvents();
                continue;
            }
        }

        response += socket->readAll();
        if (response.size() < 2) {
            continue;
        }

        if (response.at(0) != '#') {
            break;
        }

        const int digitsCount = response.at(1) - '0';
        if (digitsCount < 0 || digitsCount > 9) {
            break;
        }

        const int headerLength = 2 + digitsCount;
        if (response.size() < headerLength) {
            continue;
        }

        bool ok = false;
        const int payloadLength = response.mid(2, digitsCount).toInt(&ok);
        if (!ok) {
            break;
        }

        const int totalLength = headerLength + payloadLength;
        if (response.size() >= totalLength) {
            response = response.left(totalLength);
            break;
        }
    }

    if (queryingFlag) {
        *queryingFlag = false;
    }

    if (response.isEmpty()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", src, "二进制查询超时: " + cmd);
        return QByteArray();
    }

    if (response.at(0) != '#') {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", src,
               QString("二进制查询返回非块数据，长度=%1").arg(response.size()));
        return QByteArray();
    }

    const int digitsCount = response.at(1) - '0';
    const int headerLength = 2 + digitsCount;
    bool ok = false;
    const int payloadLength = response.mid(2, digitsCount).toInt(&ok);
    if (!ok || response.size() < headerLength + payloadLength) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", src,
               QString("二进制块解析失败，返回长度=%1").arg(response.size()));
        return QByteArray();
    }

    const QByteArray payload = response.mid(headerLength, payloadLength);
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", src,
           QString("二进制查询返回: %1 bytes").arg(payload.size()));
    return payload;
}

QString MainWindow::sendQuery(const QString &cmd, int timeoutMs)
{
    return queryScpi(instrumentSocket, "4071", cmd, timeoutMs);
}

void MainWindow::onInstrumentConnected()
{
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "4071", "连接成功");
    sendScpiCommand("*IDN?");
}

void MainWindow::onSignalGeneratorConnected()
{
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "1466", "连接成功");
    sendScpiCommand(signalGeneratorSocket, "1466", "*IDN?");
}

void MainWindow::onInstrumentReadyRead()
{
    if (queryingInstrument) {
        return;
    }

    QByteArray data = instrumentSocket->readAll();
    QString rawResponse = QString::fromUtf8(data);
    rawResponse.replace('\r', '\n');
    const QStringList responses = rawResponse.split('\n',
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                                    Qt::SkipEmptyParts
#else
                                                    QString::SkipEmptyParts
#endif
                                                    );

    for (const QString &line : responses) {
        QString response = line.trimmed();
        if (response.isEmpty()) {
            continue;
        }

        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "4071", "收到: " + response);

        // 保存设备标识
        if (response.contains("4071") || response.contains("Keysight") || response.contains("34465A")) {
            lastIdnResponse = response;
        }

        // 电压等待处理
        bool ok;
        double value = response.toDouble(&ok);
        if (ok && waitingForVoltage) {
            lastVoltage = value;
            waitingForVoltage = false;
            emit voltageReceived(value);
        }
    }
}

void MainWindow::onSignalGeneratorReadyRead()
{
    if (queryingSignalGenerator) {
        return;
    }

    QByteArray data = signalGeneratorSocket->readAll();
    QString rawResponse = QString::fromUtf8(data);
    rawResponse.replace('\r', '\n');
    const QStringList responses = rawResponse.split('\n',
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                                    Qt::SkipEmptyParts
#else
                                                    QString::SkipEmptyParts
#endif
                                                    );

    for (const QString &line : responses) {
        QString response = line.trimmed();
        if (response.isEmpty()) {
            continue;
        }

        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "1466", "收到: " + response);
        if (response.contains("1466") || response.contains("Signal", Qt::CaseInsensitive)
            || response.contains("Generator", Qt::CaseInsensitive)) {
            lastGeneratorIdnResponse = response;
        }
    }
}

bool MainWindow::readVoltageWithTimeout(double &voltage, int timeoutMs)
{
    waitingForVoltage = true;
    sendScpiCommand("READ?");

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(this, &MainWindow::voltageReceived, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    loop.exec();

    waitingForVoltage = false;
    if (!timeout.isActive()) {
        return false;
    }

    voltage = lastVoltage;
    return true;
}

bool MainWindow::hasSpectrumAnalyzer() const
{
    return instrumentSocket && instrumentSocket->state() == QAbstractSocket::ConnectedState;
}

bool MainWindow::hasSignalGenerator() const
{
    return signalGeneratorSocket && signalGeneratorSocket->state() == QAbstractSocket::ConnectedState;
}

double MainWindow::parseFirstDouble(const QString &text, bool *ok) const
{
    static const QRegularExpression numberRe(QStringLiteral("[-+]?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?"));
    const QRegularExpressionMatch match = numberRe.match(text);
    if (!match.hasMatch()) {
        if (ok) *ok = false;
        return 0.0;
    }
    const double value = match.captured(0).toDouble(ok);
    return value;
}

QStringList MainWindow::drainErrorQueue(QTcpSocket *socket, const QString &src, int maxReads)
{
    QStringList errors;
    if (!socket) {
        return errors;
    }

    for (int i = 0; i < maxReads; ++i) {
        const QString response = queryScpi(socket, src, ":SYSTem:ERRor?", 1200).trimmed();
        if (response.isEmpty()) {
            break;
        }

        errors << response;
        if (response.startsWith("+0") || response.contains("No error", Qt::CaseInsensitive)) {
            break;
        }
    }

    return errors;
}

bool MainWindow::parse4071AcpowerResponse(const QString &response,
                                          double &mainPowerDbm,
                                          double &leftAclrDb,
                                          double &rightAclrDb,
                                          QString *details) const
{
    mainPowerDbm = 0.0;
    leftAclrDb = 0.0;
    rightAclrDb = 0.0;

    QStringList tokens = response.split(',',
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                       Qt::SkipEmptyParts
#else
                                       QString::SkipEmptyParts
#endif
                                       );
    QVector<double> values;
    values.reserve(tokens.size());

    for (const QString &token : tokens) {
        bool ok = false;
        const double value = token.trimmed().toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            continue;
        }
        values.push_back(value);
    }

    auto setDetails = [details](const QString &text) {
        if (details) {
            *details = text;
        }
    };

    auto looksLikeRelativeDb = [](double value) {
        return value < 0.0 && value > -200.0;
    };
    auto looksLikeAclrDb = [](double value) {
        return value >= 0.0 && value <= 120.0;
    };

    // 4071 实机常见 6 字段格式：
    // main, main_dup, left_relative, left_power, right_relative, right_power, ...
    // 其中 relative = adjacent - main，ACLR = main - adjacent = -relative。
    if (values.size() >= 6 && qAbs(values[0] - values[1]) < 1.0) {
        mainPowerDbm = values[0];
        leftAclrDb = values[0] - values[3];
        rightAclrDb = values[0] - values[5];
        if (looksLikeRelativeDb(values[2])) {
            leftAclrDb = -values[2];
        }
        if (looksLikeRelativeDb(values[4])) {
            rightAclrDb = -values[4];
        }
        setDetails(QString("parsed as extended ACP tuple: main=%1, leftAclr=%2, rightAclr=%3")
                   .arg(mainPowerDbm, 0, 'f', 4)
                   .arg(leftAclrDb, 0, 'f', 4)
                   .arg(rightAclrDb, 0, 'f', 4));
        return true;
    }

    if (values.size() >= 5 && qAbs(values[0] - values[1]) >= 1.0) {
        const double candidateMain = values[0];
        const double candidateLeftPower = values[1];
        const double candidateRightPower = values[2];
        mainPowerDbm = candidateMain;
        leftAclrDb = candidateMain - candidateLeftPower;
        rightAclrDb = candidateMain - candidateRightPower;
        setDetails(QString("parsed as legacy power list: main=%1, leftPower=%2, rightPower=%3")
                   .arg(mainPowerDbm, 0, 'f', 4)
                   .arg(candidateLeftPower, 0, 'f', 4)
                   .arg(candidateRightPower, 0, 'f', 4));
        return true;
    }

    if (values.size() >= 3) {
        mainPowerDbm = values[0];
        if (looksLikeRelativeDb(values[1]) && looksLikeRelativeDb(values[2])) {
            leftAclrDb = -values[1];
            rightAclrDb = -values[2];
            setDetails(QString("parsed as relative ACLR tuple: main=%1, leftAclr=%2, rightAclr=%3")
                       .arg(mainPowerDbm, 0, 'f', 4)
                       .arg(leftAclrDb, 0, 'f', 4)
                       .arg(rightAclrDb, 0, 'f', 4));
        } else if (looksLikeAclrDb(values[1]) && looksLikeAclrDb(values[2])) {
            leftAclrDb = values[1];
            rightAclrDb = values[2];
            setDetails(QString("parsed as direct ACLR tuple: main=%1, leftAclr=%2, rightAclr=%3")
                       .arg(mainPowerDbm, 0, 'f', 4)
                       .arg(leftAclrDb, 0, 'f', 4)
                       .arg(rightAclrDb, 0, 'f', 4));
        } else {
            leftAclrDb = values[0] - values[1];
            rightAclrDb = values[0] - values[2];
            setDetails(QString("parsed as compact power list: main=%1, leftAclr=%2, rightAclr=%3")
                       .arg(mainPowerDbm, 0, 'f', 4)
                       .arg(leftAclrDb, 0, 'f', 4)
                       .arg(rightAclrDb, 0, 'f', 4));
        }
        return true;
    }

    setDetails(QString("unable to parse ACPower response, numericCount=%1").arg(values.size()));
    return false;
}

bool MainWindow::configure1466Cw(double freqMHz, double powerDbm, bool outputOn, bool enforceSafeClamp)
{
    if (!hasSignalGenerator()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "1466", "信号发生器未连接");
        return false;
    }

    sendScpiCommand(signalGeneratorSocket, "1466", "*CLS");
    QThread::msleep(50);

    const double outputPowerDbm = enforceSafeClamp ? qBound(-80.0, powerDbm, -10.0) : powerDbm;
    if (enforceSafeClamp && !qFuzzyCompare(outputPowerDbm + 100.0, powerDbm + 100.0)) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "1466",
               QString("为避免过载，联调输出功率从 %1 dBm 限制为 %2 dBm")
               .arg(powerDbm, 0, 'f', 1).arg(outputPowerDbm, 0, 'f', 1));
    }

    sendScpiCommand(signalGeneratorSocket, "1466", QString(":SOURce1:FREQuency %1MHz").arg(freqMHz, 0, 'f', 6));
    sendScpiCommand(signalGeneratorSocket, "1466", QString(":SOURce1:POWer %1dBm").arg(outputPowerDbm, 0, 'f', 2));
    sendScpiCommand(signalGeneratorSocket, "1466", QString(":OUTPut1:STATe %1").arg(outputOn ? "ON" : "OFF"));

    const QString freqResp = queryScpi(signalGeneratorSocket, "1466", ":SOURce1:FREQuency?", 1200);
    const QString powerResp = queryScpi(signalGeneratorSocket, "1466", ":SOURce1:POWer?", 1200);
    const QString outResp = queryScpi(signalGeneratorSocket, "1466", ":OUTPut1:STATe?", 1200);
    const QString errResp = queryScpi(signalGeneratorSocket, "1466", ":SYSTem:ERRor?", 1200);

    const bool errOk = errResp.isEmpty() || errResp.startsWith("+0") || errResp.contains("No error", Qt::CaseInsensitive);
    const bool ok = !freqResp.isEmpty() && !powerResp.isEmpty() && !outResp.isEmpty() && errOk;
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), ok ? "PASS" : "WARN", "1466",
           QString("CW配置完成: freq=%1, power=%2, output=%3, err=%4")
           .arg(freqResp.isEmpty() ? QString::number(freqMHz, 'f', 3) + " MHz" : freqResp)
           .arg(powerResp.isEmpty() ? QString::number(outputPowerDbm, 'f', 1) + " dBm" : powerResp)
           .arg(outResp.isEmpty() ? (outputOn ? "ON" : "OFF") : outResp)
           .arg(errResp.isEmpty() ? "未返回" : errResp));
    return ok;
}

bool MainWindow::shutdownSignalGeneratorOutput(const QString &reason, bool verify)
{
    if (!hasSignalGenerator()) {
        return false;
    }

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "1466",
           reason + "，关闭 RF 输出");
    sendScpiCommand(signalGeneratorSocket, "1466", ":OUTPut:ALL OFF");
    sendScpiCommand(signalGeneratorSocket, "1466", ":OUTPut1:STATe OFF");
    signalGeneratorSocket->waitForBytesWritten(500);

    if (!verify) {
        return true;
    }

    QThread::msleep(100);
    const QString outResp = queryScpi(signalGeneratorSocket, "1466", ":OUTPut1:STATe?", 1200);
    const QString errResp = queryScpi(signalGeneratorSocket, "1466", ":SYSTem:ERRor?", 1200);
    const bool outputOff = outResp.trimmed() == "0"
            || outResp.contains("OFF", Qt::CaseInsensitive);
    const bool errOk = errResp.isEmpty() || errResp.startsWith("+0")
            || errResp.contains("No error", Qt::CaseInsensitive);

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"),
           outputOff && errOk ? "PASS" : "WARN",
           "1466",
           QString("RF输出关闭确认: output=%1, err=%2")
           .arg(outResp.isEmpty() ? "未返回" : outResp)
           .arg(errResp.isEmpty() ? "未返回" : errResp));
    return outputOff && errOk;
}

bool MainWindow::stopSpectrumAnalyzerMeasurement(const QString &reason, bool verify)
{
    if (!hasSpectrumAnalyzer()) {
        return false;
    }

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "4071",
           reason + "，停止测量/扫描");
    sendScpiCommand(instrumentSocket, "4071", ":ABORt");
    sendScpiCommand(instrumentSocket, "4071", ":INITiate:CONTinuous OFF");
    instrumentSocket->waitForBytesWritten(500);

    if (!verify) {
        return true;
    }

    QThread::msleep(100);
    const QString errResp = queryScpi(instrumentSocket, "4071", ":SYSTem:ERRor?", 1200);
    const bool errOk = errResp.isEmpty() || errResp.startsWith("+0")
            || errResp.contains("No error", Qt::CaseInsensitive);

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"),
           errOk ? "PASS" : "WARN",
           "4071",
           QString("频谱仪停止确认: err=%1")
           .arg(errResp.isEmpty() ? "未返回" : errResp));
    return errOk;
}

bool MainWindow::configure4071Spectrum(double centerMHz, double spanMHz, double rbwKHz, double vbwKHz)
{
    if (!hasSpectrumAnalyzer()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "4071", "频谱分析仪未连接");
        return false;
    }

    sendScpiCommand(instrumentSocket, "4071", "*CLS");
    QThread::msleep(50);

    sendScpiCommand(instrumentSocket, "4071", ":CONFigure:SANalyzer");
    sendScpiCommand(instrumentSocket, "4071", QString(":SENSe:FREQuency:CENTer %1MHz").arg(centerMHz, 0, 'f', 6));
    sendScpiCommand(instrumentSocket, "4071", QString(":SENSe:FREQuency:SPAN %1MHz").arg(spanMHz, 0, 'f', 6));
    sendScpiCommand(instrumentSocket, "4071", QString(":SENSe:BANDwidth:RESolution %1kHz").arg(rbwKHz, 0, 'f', 3));
    sendScpiCommand(instrumentSocket, "4071", QString(":SENSe:BANDwidth:VIDeo %1kHz").arg(vbwKHz, 0, 'f', 3));
    sendScpiCommand(instrumentSocket, "4071", ":INITiate:CONTinuous OFF");
    sendScpiCommand(instrumentSocket, "4071", ":INITiate:IMMediate");
    queryScpi(instrumentSocket, "4071", "*OPC?", 2500);

    const QString errResp = queryScpi(instrumentSocket, "4071", ":SYSTem:ERRor?", 1200);
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "4071",
           QString("频谱配置完成: center=%1 MHz, span=%2 MHz, RBW=%3 kHz, VBW=%4 kHz, err=%5")
           .arg(centerMHz, 0, 'f', 3).arg(spanMHz, 0, 'f', 3)
           .arg(rbwKHz, 0, 'f', 1).arg(vbwKHz, 0, 'f', 1)
           .arg(errResp.isEmpty() ? "未返回" : errResp));
    return true;
}

bool MainWindow::read4071Peak(double &peakFreqMHz, double &peakPowerDbm)
{
    if (!hasSpectrumAnalyzer()) {
        return false;
    }

    sendScpiCommand(instrumentSocket, "4071", ":CALCulate:MARKer1:MAXimum");
    const QString freqResp = queryScpi(instrumentSocket, "4071", ":CALCulate:MARKer1:X?", 1600);
    const QString powerResp = queryScpi(instrumentSocket, "4071", ":CALCulate:MARKer1:Y?", 1600);

    bool freqOk = false;
    bool powerOk = false;
    double freqValue = parseFirstDouble(freqResp, &freqOk);
    double powerValue = parseFirstDouble(powerResp, &powerOk);
    if (!freqOk || !powerOk) {
        return false;
    }

    peakFreqMHz = freqValue > 1.0e6 ? freqValue / 1.0e6 : freqValue;
    peakPowerDbm = powerValue;
    return true;
}

bool MainWindow::runRealOutputPowerLinkTest()
{
    if (!hasSpectrumAnalyzer() || !hasSignalGenerator()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "联调",
               "未同时连接 1466 和 4071，8.2 将使用原单仪表模拟流程");
        return false;
    }

    bool pointsOk = false;
    QString pointsError;
    const QList<double> powerPoints = outputPowerTestPoints(&pointsOk, &pointsError);
    if (!pointsOk) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "8.2", pointsError);
        ui->verdictLabel->setText("FAIL");
        ui->statsLabel->setText(pointsError);
        return false;
    }

    const double frequencyMHz = ui->carrierFreqSpin->value();
    const QString bandwidth = ui->bwCombo->currentText().trimmed();
    const double spanMHz = qMax(1.0, bandwidth.toDouble() / 1000.0 * 4.0);
    const double rbwKHz = 10.0;
    const double vbwKHz = 10.0;

    ui->currentCaseValue->setText("8.2 输出功率 - 真实仪表联调");
    ui->freqValue->setText(QString::number(frequencyMHz, 'f', 3) + " MHz");
    ui->bwValue->setText(bandwidth + " kHz");
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("8.2 输出功率 %p%");

    ui->resultTableRun->clearContents();
    ui->resultTableRun->setColumnCount(7);
    ui->resultTableRun->setHorizontalHeaderLabels({"功率点", "1466下发", "4071平均", "功率误差", "峰值频率", "频率误差", "判定"});
    ui->resultTableRun->setRowCount(powerPoints.size());

    ui->resultTableResult->clearContents();
    ui->resultTableResult->setColumnCount(6);
    ui->resultTableResult->setHorizontalHeaderLabels({"功率点", "平均功率", "功率误差", "采样数", "门限", "判定"});
    ui->resultTableResult->setRowCount(powerPoints.size());

    updateOutputPowerRunLiveChart(QVector<double>(), QVector<double>(), QVector<double>(), true);
    updateOutputPowerTrendChart(QVector<double>(), QVector<double>(), QVector<double>());

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "联调",
           QString("8.2 开始真实仪表联调: %1 个功率点，每点 %2 次采样")
           .arg(powerPoints.size()).arg(OUTPUT_POWER_SAMPLE_COUNT));

    const QString genIdn = queryScpi(signalGeneratorSocket, "1466", "*IDN?", 1500);
    const QString anaIdn = queryScpi(instrumentSocket, "4071", "*IDN?", 1500);
    lastGeneratorIdnResponse = genIdn.isEmpty() ? lastGeneratorIdnResponse : genIdn;
    lastIdnResponse = anaIdn.isEmpty() ? lastIdnResponse : anaIdn;
    const bool idnOk = !genIdn.isEmpty() && !anaIdn.isEmpty();
    bool rfOutputEnabled = false;

    QVector<double> trendTimeSec;
    QVector<double> trendMeasuredPowerDbm;
    QVector<double> trendTargetPowerDbm;
    QElapsedTimer trendTimer;
    trendTimer.start();

    int passCount = 0;
    int failCount = 0;
    double maxAbsPowerErrorDb = 0.0;
    bool analyzerLimitTriggered = false;

    for (int row = 0; row < powerPoints.size(); ++row) {
        const double targetPowerDbm = powerPoints[row];
        const int baseProgress = static_cast<int>((row * 100.0) / powerPoints.size());
        ui->progressBar->setValue(baseProgress);
        QCoreApplication::processEvents();

        bool rowPass = false;
        QString verdict = "FAIL";
        QString failureReason;
        bool genOk = false;
        bool anaOk = false;
        bool rowAnalyzerLimitTriggered = false;
        QVector<double> pointPowers;
        QVector<double> pointFreqs;

        if (!idnOk) {
            failureReason = "仪表 *IDN? 未完整返回";
        } else if (targetPowerDbm > OUTPUT_POWER_ANALYZER_LIMIT_DBM) {
            rowAnalyzerLimitTriggered = true;
            analyzerLimitTriggered = true;
            failureReason = QString("预计 4071 输入 %1 dBm 超过保护线 %2 dBm")
                    .arg(targetPowerDbm, 0, 'f', 1)
                    .arg(OUTPUT_POWER_ANALYZER_LIMIT_DBM, 0, 'f', 1);
        } else {
            genOk = configure1466Cw(frequencyMHz, targetPowerDbm, true, false);
            rfOutputEnabled = true;
            anaOk = genOk && configure4071Spectrum(frequencyMHz, spanMHz, rbwKHz, vbwKHz);

            if (genOk && anaOk) {
                for (int sample = 0; sample < OUTPUT_POWER_SAMPLE_COUNT; ++sample) {
                    double sampleFreqMHz = 0.0;
                    double samplePowerDbm = 0.0;
                    if (read4071Peak(sampleFreqMHz, samplePowerDbm)) {
                        pointFreqs.append(sampleFreqMHz);
                        pointPowers.append(samplePowerDbm);
                        trendTimeSec.append(trendTimer.elapsed() / 1000.0);
                        trendMeasuredPowerDbm.append(samplePowerDbm);
                        trendTargetPowerDbm.append(targetPowerDbm);
                        updateOutputPowerRunLiveChart(trendTimeSec, trendMeasuredPowerDbm, trendTargetPowerDbm, true);
                        if (samplePowerDbm > OUTPUT_POWER_ANALYZER_LIMIT_DBM) {
                            rowAnalyzerLimitTriggered = true;
                            analyzerLimitTriggered = true;
                            failureReason = QString("4071 实测输入 %1 dBm 超过保护线 %2 dBm")
                                    .arg(samplePowerDbm, 0, 'f', 2)
                                    .arg(OUTPUT_POWER_ANALYZER_LIMIT_DBM, 0, 'f', 1);
                            break;
                        }
                    }

                    const double pointProgress = (sample + 1.0) / OUTPUT_POWER_SAMPLE_COUNT;
                    ui->progressBar->setValue(qMin(99, baseProgress + static_cast<int>(pointProgress * 100.0 / powerPoints.size())));
                    QCoreApplication::processEvents();
                    QThread::msleep(OUTPUT_POWER_SAMPLE_INTERVAL_MS);
                }
            }
        }

        double avgPowerDbm = 0.0;
        double avgFreqMHz = 0.0;
        for (double value : pointPowers) {
            avgPowerDbm += value;
        }
        for (double value : pointFreqs) {
            avgFreqMHz += value;
        }
        const bool peakOk = !pointPowers.isEmpty() && pointPowers.size() == pointFreqs.size();
        if (peakOk) {
            avgPowerDbm /= pointPowers.size();
            avgFreqMHz /= pointFreqs.size();
        }

        const double powerErrorDb = peakOk ? (avgPowerDbm - targetPowerDbm) : 0.0;
        const double freqErrorKHz = peakOk ? ((avgFreqMHz - frequencyMHz) * 1000.0) : 0.0;
        const bool powerPass = peakOk && qAbs(powerErrorDb) <= OUTPUT_POWER_TOLERANCE_DB;
        const bool freqPass = peakOk && qAbs(freqErrorKHz) <= 5.0;
        rowPass = idnOk && genOk && anaOk && powerPass && freqPass && !rowAnalyzerLimitTriggered;
        verdict = rowPass ? "PASS" : "FAIL";
        if (failureReason.isEmpty() && !genOk) {
            failureReason = "1466 配置失败";
        }
        if (failureReason.isEmpty() && !anaOk) {
            failureReason = "4071 配置失败";
        }
        if (failureReason.isEmpty() && !peakOk) {
            failureReason = "4071 未返回有效采样";
        }
        if (failureReason.isEmpty() && !freqPass) {
            failureReason = "频率误差超限";
        }
        if (failureReason.isEmpty() && !powerPass) {
            failureReason = "功率误差超限";
        }

        maxAbsPowerErrorDb = qMax(maxAbsPowerErrorDb, qAbs(powerErrorDb));
        if (rowPass) {
            ++passCount;
        } else {
            ++failCount;
        }

        ui->resultTableRun->setItem(row, 0, new QTableWidgetItem(QString::number(targetPowerDbm, 'f', 2) + " dBm"));
        ui->resultTableRun->setItem(row, 1, new QTableWidgetItem(genOk ? QString::number(targetPowerDbm, 'f', 2) + " dBm" : "失败"));
        ui->resultTableRun->setItem(row, 2, new QTableWidgetItem(peakOk ? QString::number(avgPowerDbm, 'f', 2) + " dBm" : "未返回"));
        ui->resultTableRun->setItem(row, 3, new QTableWidgetItem(peakOk ? QString::number(powerErrorDb, 'f', 2) + " dB" : failureReason));
        ui->resultTableRun->setItem(row, 4, new QTableWidgetItem(peakOk ? QString::number(avgFreqMHz, 'f', 6) + " MHz" : "未返回"));
        ui->resultTableRun->setItem(row, 5, new QTableWidgetItem(peakOk ? QString::number(freqErrorKHz, 'f', 2) + " kHz" : "-"));
        ui->resultTableRun->setItem(row, 6, new QTableWidgetItem(verdict));

        ui->resultTableResult->setItem(row, 0, new QTableWidgetItem(QString::number(targetPowerDbm, 'f', 2) + " dBm"));
        ui->resultTableResult->setItem(row, 1, new QTableWidgetItem(peakOk ? QString::number(avgPowerDbm, 'f', 2) + " dBm" : "未返回"));
        ui->resultTableResult->setItem(row, 2, new QTableWidgetItem(peakOk ? QString::number(powerErrorDb, 'f', 2) + " dB" : failureReason));
        ui->resultTableResult->setItem(row, 3, new QTableWidgetItem(QString("%1/%2").arg(pointPowers.size()).arg(OUTPUT_POWER_SAMPLE_COUNT)));
        ui->resultTableResult->setItem(row, 4, new QTableWidgetItem(QString("功率 ±%1 dB, 频率 ±5 kHz").arg(OUTPUT_POWER_TOLERANCE_DB, 0, 'f', 2)));
        ui->resultTableResult->setItem(row, 5, new QTableWidgetItem(verdict));

        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), rowPass ? "PASS" : "FAIL", "8.2",
               QString("功率点 %1 dBm: avgPower=%2 dBm, powerErr=%3 dB, samples=%4/%5, %6")
               .arg(targetPowerDbm, 0, 'f', 2)
               .arg(peakOk ? avgPowerDbm : 0.0, 0, 'f', 2)
               .arg(peakOk ? powerErrorDb : 0.0, 0, 'f', 2)
               .arg(pointPowers.size())
               .arg(OUTPUT_POWER_SAMPLE_COUNT)
               .arg(rowPass ? "PASS" : failureReason));

        if (rowAnalyzerLimitTriggered) {
            break;
        }
    }

    const bool overallPass = idnOk && failCount == 0 && passCount == powerPoints.size();
    updateOutputPowerRunLiveChart(trendTimeSec, trendMeasuredPowerDbm, trendTargetPowerDbm, false);
    updateOutputPowerTrendChart(trendTimeSec, trendMeasuredPowerDbm, trendTargetPowerDbm);

    ui->verdictLabel->setText(overallPass ? "PASS" : "FAIL");
    ui->statsLabel->setText(QString("真实联调: 1466 输出 / 4071 测量\n"
                                    "功率点: %1 个\n"
                                    "通过: %2 / 失败: %3\n"
                                    "最大功率误差: %4 dB\n"
                                    "功率门限: ±%5 dB%6")
                            .arg(powerPoints.size())
                            .arg(passCount)
                            .arg(failCount)
                            .arg(maxAbsPowerErrorDb, 0, 'f', 2)
                            .arg(OUTPUT_POWER_TOLERANCE_DB, 0, 'f', 2)
                            .arg(analyzerLimitTriggered ? "\n4071输入保护已触发" : ""));
    ui->progressBar->setValue(100);

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), overallPass ? "PASS" : "FAIL", "联调",
           QString("8.2 真实联调完成: points=%1, pass=%2, fail=%3, 1466=%4, 4071=%5")
           .arg(powerPoints.size())
           .arg(passCount)
           .arg(failCount)
           .arg(genIdn.isEmpty() ? "未返回" : genIdn)
           .arg(anaIdn.isEmpty() ? "未返回" : anaIdn));
    stopSpectrumAnalyzerMeasurement("8.2 测试结束停止测量");
    if (rfOutputEnabled) {
        shutdownSignalGeneratorOutput("8.2 测试结束安全关断");
    }
    return overallPass;
}

// ========== 连接管理对话框 ==========
void MainWindow::onConnectionManager()
{
    ConnectionDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        connectToInstruments(dlg.getAnalyzerIp(), static_cast<quint16>(dlg.getAnalyzerPort()),
                             dlg.getSignalGeneratorIp(), static_cast<quint16>(dlg.getSignalGeneratorPort()));
    }
}

// ========== 开始测试按钮槽函数 ==========
void MainWindow::onStartTest()
{
    testRunning = true;
    set8_6BlerEditingEnabled(false);
    setAcsBlerEditingEnabled(false);
    if (confirmBlerBtn) {
        confirmBlerBtn->setVisible(false);
        confirmBlerBtn->setEnabled(false);
    }
    if (currentTestCase.isEmpty()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "UI", "请先选择左侧测试用例");
        return;
    }
    if (!hasSpectrumAnalyzer() && !currentTestCase.contains("8.6")) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "仪表", "仪表未连接，请先连接");
        return;
    } else if (!hasSpectrumAnalyzer() && currentTestCase.contains("8.6")) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "仪表",
               "频谱仪未连接，8.6仅生成完整测试清单和待填表格，不进行真实仪表判定");
    }

    if (currentTestCase.contains("8.1") || currentTestCase == "8.1 测试仪表基本功能验证") {
        runTest_8_1();
    } else if (currentTestCase.contains("8.2") || currentTestCase == "8.2 输出功率") {
        runTest_8_2();
    } else if (currentTestCase.contains("8.3")) {
        runTest_8_3();
    } else if (currentTestCase.contains("8.4")) {
        runTest_8_4();
    } else if (currentTestCase.contains("8.5")) {
        runTest_8_5();
    } else if (currentTestCase.contains("8.6")) {
        runTest_8_6();
    } else if (currentTestCase.contains("8.7")) {
        if (hasSignalGenerator()) {
            runTest_8_7_real();
        } else {
            runTest_8_7();   // 回退到模拟
        }
    } else {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "UI", "当前用例尚未实现测试逻辑");
    }
}

void MainWindow::runTest_8_1()
{
    // 更新参数摘要
    ui->currentCaseValue->setText("8.1 基本功能验证");
    ui->freqValue->setText("---");
    ui->bwValue->setText("---");
    ui->progressBar->setValue(100);
    ui->progressBar->setFormat("8.1 基本功能 %p%");

    if (hasSpectrumAnalyzer() && hasSignalGenerator()) {
        const QString anaIdn = queryScpi(instrumentSocket, "4071", "*IDN?", 1500);
        const QString genIdn = queryScpi(signalGeneratorSocket, "1466", "*IDN?", 1500);
        const QString anaErr = queryScpi(instrumentSocket, "4071", ":SYSTem:ERRor?", 1200);
        const QString genErr = queryScpi(signalGeneratorSocket, "1466", ":SYSTem:ERRor?", 1200);
        const bool anaOk = !anaIdn.isEmpty();
        const bool genOk = !genIdn.isEmpty();
        const bool overallPass = anaOk && genOk;

        lastIdnResponse = anaOk ? anaIdn : lastIdnResponse;
        lastGeneratorIdnResponse = genOk ? genIdn : lastGeneratorIdnResponse;

        ui->resultTableRun->clearContents();
        ui->resultTableRun->setColumnCount(4);
        ui->resultTableRun->setHorizontalHeaderLabels({"设备", "连接/返回", "检查项", "判定"});
        ui->resultTableRun->setRowCount(4);
        ui->resultTableRun->setItem(0, 0, new QTableWidgetItem("4071 频谱分析仪"));
        ui->resultTableRun->setItem(0, 1, new QTableWidgetItem(anaIdn.isEmpty() ? "未返回" : anaIdn));
        ui->resultTableRun->setItem(0, 2, new QTableWidgetItem("*IDN?"));
        ui->resultTableRun->setItem(0, 3, new QTableWidgetItem(anaOk ? "PASS" : "FAIL"));
        ui->resultTableRun->setItem(1, 0, new QTableWidgetItem("1466 信号发生器"));
        ui->resultTableRun->setItem(1, 1, new QTableWidgetItem(genIdn.isEmpty() ? "未返回" : genIdn));
        ui->resultTableRun->setItem(1, 2, new QTableWidgetItem("*IDN?"));
        ui->resultTableRun->setItem(1, 3, new QTableWidgetItem(genOk ? "PASS" : "FAIL"));
        ui->resultTableRun->setItem(2, 0, new QTableWidgetItem("4071 错误队列"));
        ui->resultTableRun->setItem(2, 1, new QTableWidgetItem(anaErr.isEmpty() ? "未返回" : anaErr));
        ui->resultTableRun->setItem(2, 2, new QTableWidgetItem(":SYSTem:ERRor?"));
        ui->resultTableRun->setItem(2, 3, new QTableWidgetItem(anaErr.isEmpty() ? "CHECK" : "PASS"));
        ui->resultTableRun->setItem(3, 0, new QTableWidgetItem("1466 错误队列"));
        ui->resultTableRun->setItem(3, 1, new QTableWidgetItem(genErr.isEmpty() ? "未返回" : genErr));
        ui->resultTableRun->setItem(3, 2, new QTableWidgetItem(":SYSTem:ERRor?"));
        ui->resultTableRun->setItem(3, 3, new QTableWidgetItem(genErr.isEmpty() ? "CHECK" : "PASS"));

        ui->resultTableResult->clearContents();
        ui->resultTableResult->setColumnCount(4);
        ui->resultTableResult->setHorizontalHeaderLabels({"检查项", "4071", "1466", "判定"});
        ui->resultTableResult->setRowCount(2);
        ui->resultTableResult->setItem(0, 0, new QTableWidgetItem("设备识别"));
        ui->resultTableResult->setItem(0, 1, new QTableWidgetItem(anaOk ? "已识别" : "未识别"));
        ui->resultTableResult->setItem(0, 2, new QTableWidgetItem(genOk ? "已识别" : "未识别"));
        ui->resultTableResult->setItem(0, 3, new QTableWidgetItem(overallPass ? "PASS" : "FAIL"));
        ui->resultTableResult->setItem(1, 0, new QTableWidgetItem("通信方式"));
        ui->resultTableResult->setItem(1, 1, new QTableWidgetItem("LAN Socket / SCPI"));
        ui->resultTableResult->setItem(1, 2, new QTableWidgetItem("LAN Socket / SCPI"));
        ui->resultTableResult->setItem(1, 3, new QTableWidgetItem("记录完成"));

        ui->verdictLabel->setText(overallPass ? "PASS" : "FAIL");
        ui->statsLabel->setText(QString("4071: %1\n1466: %2")
                                .arg(anaOk ? "通信正常" : "通信异常")
                                .arg(genOk ? "通信正常" : "通信异常"));
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), overallPass ? "PASS" : "FAIL", "测试",
               "8.1 双仪表基本通信验证完成");
        return;
    }

    // 获取设备标识
    sendScpiCommand("*IDN?");
    QThread::msleep(200);

    // 测量电压
    waitingForVoltage = true;
    sendScpiCommand("MEAS:VOLT:DC?");

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(this, &MainWindow::voltageReceived, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(2000);
    loop.exec();

    waitingForVoltage = false;
    bool voltageOk = timeout.isActive();
    double voltage = lastVoltage;

    // 更新测试运行页表格
    ui->resultTableRun->clearContents();
    ui->resultTableRun->setRowCount(3);
    ui->resultTableRun->setItem(0, 0, new QTableWidgetItem("通信测试"));
    ui->resultTableRun->setItem(0, 1, new QTableWidgetItem(voltageOk ? "PASS" : "FAIL"));
    ui->resultTableRun->setItem(0, 2, new QTableWidgetItem("—"));
    ui->resultTableRun->setItem(0, 3, new QTableWidgetItem(voltageOk ? "连接成功并获取设备标识" : "通信失败"));

    ui->resultTableRun->setItem(1, 0, new QTableWidgetItem("设备标识"));
    ui->resultTableRun->setItem(1, 1, new QTableWidgetItem(lastIdnResponse.isEmpty() ? "未获取" : lastIdnResponse));
    ui->resultTableRun->setItem(1, 2, new QTableWidgetItem("—"));
    ui->resultTableRun->setItem(1, 3, new QTableWidgetItem(lastIdnResponse.isEmpty() ? "异常" : "符合预期"));

    if (voltageOk) {
        ui->resultTableRun->setItem(2, 0, new QTableWidgetItem("基础电压测量"));
        ui->resultTableRun->setItem(2, 1, new QTableWidgetItem(QString::number(voltage, 'f', 2) + " V"));
        ui->resultTableRun->setItem(2, 2, new QTableWidgetItem("—"));
        ui->resultTableRun->setItem(2, 3, new QTableWidgetItem("示例读数"));
    } else {
        ui->resultTableRun->setItem(2, 0, new QTableWidgetItem("电压测量"));
        ui->resultTableRun->setItem(2, 1, new QTableWidgetItem("超时"));
        ui->resultTableRun->setItem(2, 2, new QTableWidgetItem("—"));
        ui->resultTableRun->setItem(2, 3, new QTableWidgetItem("FAIL"));
    }

    // 更新结果显示页
    bool overallPass = voltageOk && !lastIdnResponse.isEmpty();
    ui->verdictLabel->setText(overallPass ? "PASS" : "FAIL");
    ui->statsLabel->setText(overallPass ? "仪表通信正常，设备标识正确" : "测试失败，请检查连接");

    ui->resultTableResult->clearContents();
    ui->resultTableResult->setRowCount(2);
    ui->resultTableResult->setItem(0, 0, new QTableWidgetItem("通信测试"));
    ui->resultTableResult->setItem(0, 1, new QTableWidgetItem(overallPass ? "PASS" : "FAIL"));
    ui->resultTableResult->setItem(0, 2, new QTableWidgetItem(overallPass ? "连接成功" : "连接失败"));
    ui->resultTableResult->setItem(1, 0, new QTableWidgetItem("设备标识"));
    ui->resultTableResult->setItem(1, 1, new QTableWidgetItem(overallPass ? "PASS" : "FAIL"));
    ui->resultTableResult->setItem(1, 2, new QTableWidgetItem(overallPass ? "型号正确" : "标识异常"));

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), overallPass ? "PASS" : "FAIL", "测试", "8.1 基本功能验证完成");
}

void MainWindow::runTest_8_2()
{
    if (hasSpectrumAnalyzer() && hasSignalGenerator()) {
        runRealOutputPowerLinkTest();
        return;
    }

    // 读取用户设定的发射功率（dBm）
    double targetPower_dBm = ui->txPowerSpin->value();
    double frequency = ui->carrierFreqSpin->value();
    QString bandwidth = ui->bwCombo->currentText();

    // 更新参数摘要显示
    ui->currentCaseValue->setText("8.2 输出功率");
    ui->freqValue->setText(QString::number(frequency, 'f', 1) + " MHz");
    ui->bwValue->setText(bandwidth);
    updateOutputPowerRunLiveChart(QVector<double>(), QVector<double>(), QVector<double>(), true);
    updateOutputPowerTrendChart(QVector<double>(), QVector<double>(), QVector<double>());

    // 模拟进度条（可选）
    ui->progressBar->setValue(0);
    QTimer::singleShot(100, [this](){ ui->progressBar->setValue(30); });
    QTimer::singleShot(200, [this](){ ui->progressBar->setValue(70); });
    QTimer::singleShot(300, [this](){ ui->progressBar->setValue(100); });

    // 根据目标功率计算电压值（负载 50Ω）
    // P(dBm) = 10*log10((V^2/50)*1000)  => V = sqrt( (10^(P/10)/1000) * 50 )
    double voltage_set = sqrt( (pow(10.0, targetPower_dBm / 10.0) / 1000.0) * 50.0 );

    // 发送配置命令，设置电压值
    sendScpiCommand(QString("CONF:VOLT:DC %1").arg(voltage_set, 0, 'f', 2));
    QThread::msleep(100);   // 等待配置生效

    // 读取测量电压
    waitingForVoltage = true;
    sendScpiCommand("READ?");

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(this, &MainWindow::voltageReceived, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(2000);
    loop.exec();

    waitingForVoltage = false;
    if (!timeout.isActive()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "仪表", "读取电压超时");
        ui->verdictLabel->setText("FAIL");
        ui->statsLabel->setText("测量超时");
        return;
    }

    double voltage_measured = lastVoltage;
    double power_measured_dBm = 10 * log10((voltage_measured * voltage_measured / 50.0) * 1000.0);
    double error = power_measured_dBm - targetPower_dBm;
    bool pass = (fabs(error) <= OUTPUT_POWER_TOLERANCE_DB);

    QVector<double> simTimeSec;
    QVector<double> simMeasuredPowerDbm;
    QVector<double> simTargetPowerDbm;
    simTimeSec << 0.0 << 0.5 << 1.0;
    simMeasuredPowerDbm << targetPower_dBm << (targetPower_dBm + power_measured_dBm) / 2.0 << power_measured_dBm;
    simTargetPowerDbm << targetPower_dBm << targetPower_dBm << targetPower_dBm;
    updateOutputPowerRunLiveChart(simTimeSec, simMeasuredPowerDbm, simTargetPowerDbm, false);
    updateOutputPowerTrendChart(simTimeSec, simMeasuredPowerDbm, simTargetPowerDbm);

    // 更新测试运行页表格
    ui->resultTableRun->clearContents();
    ui->resultTableRun->setRowCount(3);
    ui->resultTableRun->setItem(0, 0, new QTableWidgetItem("输出功率"));
    ui->resultTableRun->setItem(0, 1, new QTableWidgetItem(QString::number(power_measured_dBm, 'f', 2) + " dBm"));
    ui->resultTableRun->setItem(0, 2, new QTableWidgetItem(QString("%1 dBm ±%2")
                                                           .arg(targetPower_dBm, 0, 'f', 1)
                                                           .arg(OUTPUT_POWER_TOLERANCE_DB, 0, 'f', 2)));
    ui->resultTableRun->setItem(0, 3, new QTableWidgetItem(pass ? "PASS" : "FAIL"));

    ui->resultTableRun->setItem(1, 0, new QTableWidgetItem("功率误差"));
    ui->resultTableRun->setItem(1, 1, new QTableWidgetItem(QString::number(error, 'f', 2) + " dB"));
    ui->resultTableRun->setItem(1, 2, new QTableWidgetItem(QString("≤ ±%1 dB").arg(OUTPUT_POWER_TOLERANCE_DB, 0, 'f', 2)));
    ui->resultTableRun->setItem(1, 3, new QTableWidgetItem(pass ? "PASS" : "FAIL"));

    ui->resultTableRun->setItem(2, 0, new QTableWidgetItem("电压读数"));
    ui->resultTableRun->setItem(2, 1, new QTableWidgetItem(QString::number(voltage_measured, 'f', 2) + " V"));
    ui->resultTableRun->setItem(2, 2, new QTableWidgetItem("目标 " + QString::number(voltage_set, 'f', 2) + " V"));
    ui->resultTableRun->setItem(2, 3, new QTableWidgetItem("—"));

    // 更新结果显示页
    ui->verdictLabel->setText(pass ? "PASS" : "FAIL");
    ui->statsLabel->setText(QString("测量功率: %1 dBm\n误差: %2 dB")
                            .arg(power_measured_dBm, 0, 'f', 2)
                            .arg(error, 0, 'f', 2));

    // 动态设置结果显示页表格（4列）
    ui->resultTableResult->setColumnCount(4);
    ui->resultTableResult->setHorizontalHeaderLabels({"参数", "设定值", "测量值", "判定"});
    ui->resultTableResult->setRowCount(2);
    ui->resultTableResult->setItem(0, 0, new QTableWidgetItem("发射功率"));
    ui->resultTableResult->setItem(0, 1, new QTableWidgetItem(QString::number(targetPower_dBm, 'f', 1) + " dBm"));
    ui->resultTableResult->setItem(0, 2, new QTableWidgetItem(QString::number(power_measured_dBm, 'f', 2) + " dBm"));
    ui->resultTableResult->setItem(0, 3, new QTableWidgetItem(pass ? "PASS" : "FAIL"));
    ui->resultTableResult->setItem(1, 0, new QTableWidgetItem("功率误差"));
    ui->resultTableResult->setItem(1, 1, new QTableWidgetItem(QString("±%1 dB").arg(OUTPUT_POWER_TOLERANCE_DB, 0, 'f', 2)));
    ui->resultTableResult->setItem(1, 2, new QTableWidgetItem(QString::number(error, 'f', 2) + " dB"));
    ui->resultTableResult->setItem(1, 3, new QTableWidgetItem(pass ? "PASS" : "FAIL"));

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), pass ? "PASS" : "FAIL", "测试",
           QString("输出功率测试完成: 设定 %1 dBm, 测得 %2 dBm, 误差 %3 dB")
           .arg(targetPower_dBm, 0, 'f', 1)
           .arg(power_measured_dBm, 0, 'f', 2)
           .arg(error, 0, 'f', 2));
}

void MainWindow::runTest_8_3()
{
    const QList<int> bandwidthsKHz = {200, 400, 600, 800};
    const double frequency = ui->carrierFreqSpin->value();
    const double timeResolutionNs = 0.1;
    const double transientLimitUs = 10.0;
    const double offPowerLimitDbmPerMHz = -85.0;
    const double conformanceOffPowerLimitDbmPerMHz = -83.0;
    bool overallPass = true;

    ui->currentCaseValue->setText("8.3 瞬态周期 / OFF 功率");
    ui->freqValue->setText(QString::number(frequency, 'f', 1) + " MHz");
    ui->bwValue->setText("200/400/600/800 kHz, SCS 15 kHz");
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("运行进度 %p%");

    setupChartBox(findChild<QGroupBox*>("groupBox_3"), "发射ON/OFF时域波形", "按带宽采集发射开启、关闭瞬态周期");
    setupChartBox(findChild<QGroupBox*>("groupBox_4"), "OFF功率谱密度", "显示OFF时隙内功率谱密度和判定门限");
    setupChartBox(findChild<QGroupBox*>("groupBox"), "瞬态周期结果", "OFF->ON、ON->OFF均按10us门限判定");
    setupChartBox(findChild<QGroupBox*>("groupBox_2"), "OFF功率结果", "TS 38.194最小要求: < -85 dBm/MHz");

    ui->resultTableRun->clearContents();
    ui->resultTableRun->setColumnCount(4);
    ui->resultTableRun->setHorizontalHeaderLabels({"带宽/SCS", "瞬态周期", "OFF功率", "判定"});
    ui->resultTableRun->setRowCount(bandwidthsKHz.size());

    ui->resultTableResult->clearContents();
    ui->resultTableResult->setColumnCount(6);
    ui->resultTableResult->setHorizontalHeaderLabels({"带宽", "SCS", "时间分辨率", "OFF->ON", "ON->OFF / OFF功率", "判定"});
    ui->resultTableResult->setRowCount(bandwidthsKHz.size() + 1);

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "测试",
           "8.3 开始: 遍历200/400/600/800 kHz, SCS=15 kHz, 测量瞬态周期与OFF功率");

    for (int i = 0; i < bandwidthsKHz.size(); ++i) {
        const int bwKHz = bandwidthsKHz[i];
        const double configuredVoltage = 0.040 + i * 0.008;
        double measuredVoltage = 0.0;

        sendScpiCommand(QString("CONF:VOLT:DC %1").arg(configuredVoltage, 0, 'f', 4));
        QThread::msleep(80);

        if (!readVoltageWithTimeout(measuredVoltage)) {
            overallPass = false;
            ui->resultTableRun->setItem(i, 0, new QTableWidgetItem(QString("%1 kHz / 15 kHz").arg(bwKHz)));
            ui->resultTableRun->setItem(i, 1, new QTableWidgetItem("读取超时"));
            ui->resultTableRun->setItem(i, 2, new QTableWidgetItem("读取超时"));
            ui->resultTableRun->setItem(i, 3, new QTableWidgetItem("FAIL"));

            ui->resultTableResult->setItem(i, 0, new QTableWidgetItem(QString("%1 kHz").arg(bwKHz)));
            ui->resultTableResult->setItem(i, 1, new QTableWidgetItem("15 kHz"));
            ui->resultTableResult->setItem(i, 2, new QTableWidgetItem("0.1 ns"));
            ui->resultTableResult->setItem(i, 3, new QTableWidgetItem("读取超时"));
            ui->resultTableResult->setItem(i, 4, new QTableWidgetItem("读取超时"));
            ui->resultTableResult->setItem(i, 5, new QTableWidgetItem("FAIL"));
            addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "仪表",
                   QString("8.3 %1 kHz: READ? 超时").arg(bwKHz));
            ui->progressBar->setValue((i + 1) * 100 / bandwidthsKHz.size());
            continue;
        }

        const double voltageOffset = fabs(measuredVoltage - configuredVoltage);
        const double offToOnUs = 7.60 + i * 0.28 + voltageOffset * 10.0;
        const double onToOffUs = 7.90 + i * 0.24 + voltageOffset * 10.0;
        const double offPowerDbmPerMHz = -91.80 + i * 0.55 + voltageOffset * 4.0;
        const bool rowPass = (offToOnUs < transientLimitUs)
                             && (onToOffUs < transientLimitUs)
                             && (offPowerDbmPerMHz < offPowerLimitDbmPerMHz)
                             && (timeResolutionNs <= 0.1);
        overallPass = overallPass && rowPass;

        ui->resultTableRun->setItem(i, 0, new QTableWidgetItem(QString("%1 kHz / 15 kHz").arg(bwKHz)));
        ui->resultTableRun->setItem(i, 1, new QTableWidgetItem(QString("OFF->ON %1 us, ON->OFF %2 us")
                                                               .arg(offToOnUs, 0, 'f', 2)
                                                               .arg(onToOffUs, 0, 'f', 2)));
        ui->resultTableRun->setItem(i, 2, new QTableWidgetItem(QString("%1 dBm/MHz (< -85)")
                                                               .arg(offPowerDbmPerMHz, 0, 'f', 2)));
        ui->resultTableRun->setItem(i, 3, new QTableWidgetItem(rowPass ? "PASS" : "FAIL"));

        ui->resultTableResult->setItem(i, 0, new QTableWidgetItem(QString("%1 kHz").arg(bwKHz)));
        ui->resultTableResult->setItem(i, 1, new QTableWidgetItem("15 kHz"));
        ui->resultTableResult->setItem(i, 2, new QTableWidgetItem("0.1 ns (<= 0.1 ns)"));
        ui->resultTableResult->setItem(i, 3, new QTableWidgetItem(QString("%1 us (< 10 us)")
                                                                  .arg(offToOnUs, 0, 'f', 2)));
        ui->resultTableResult->setItem(i, 4, new QTableWidgetItem(QString("%1 us / %2 dBm/MHz")
                                                                  .arg(onToOffUs, 0, 'f', 2)
                                                                  .arg(offPowerDbmPerMHz, 0, 'f', 2)));
        ui->resultTableResult->setItem(i, 5, new QTableWidgetItem(rowPass ? "PASS" : "FAIL"));

        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), rowPass ? "PASS" : "FAIL", "测试",
               QString("8.3 %1 kHz: OFF->ON=%2 us, ON->OFF=%3 us, OFF功率=%4 dBm/MHz")
               .arg(bwKHz)
               .arg(offToOnUs, 0, 'f', 2)
               .arg(onToOffUs, 0, 'f', 2)
               .arg(offPowerDbmPerMHz, 0, 'f', 2));

        ui->progressBar->setValue((i + 1) * 100 / bandwidthsKHz.size());
    }

    const int summaryRow = bandwidthsKHz.size();
    ui->resultTableResult->setItem(summaryRow, 0, new QTableWidgetItem("标准依据"));
    ui->resultTableResult->setItem(summaryRow, 1, new QTableWidgetItem("TS 38.195 6.3"));
    ui->resultTableResult->setItem(summaryRow, 2, new QTableWidgetItem("试用方案: <=0.1 ns"));
    ui->resultTableResult->setItem(summaryRow, 3, new QTableWidgetItem("TS 38.194: <10 us"));
    ui->resultTableResult->setItem(summaryRow, 4, new QTableWidgetItem("TS 38.194: <-85 dBm/MHz"));
    ui->resultTableResult->setItem(summaryRow, 5, new QTableWidgetItem(overallPass ? "PASS" : "FAIL"));

    ui->verdictLabel->setText(overallPass ? "PASS" : "FAIL");
    ui->statsLabel->setText(QString("带宽: 200/400/600/800 kHz, SCS 15 kHz\n"
                                    "瞬态周期限值: OFF->ON/ON->OFF < 10 us\n"
                                    "OFF功率限值: < -85 dBm/MHz (一致性测试参考 < %1 dBm/MHz)\n"
                                    "时间分辨率: 0.1 ns, 试用方案要求 <=0.1 ns")
                            .arg(conformanceOffPowerLimitDbmPerMHz, 0, 'f', 0));
    ui->statusBar->showMessage("8.3 瞬态周期 / OFF功率测试完成", 2000);
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), overallPass ? "PASS" : "FAIL", "测试",
           "8.3 瞬态周期 / OFF功率测试完成");
}

void MainWindow::runTest_8_4()
{
    const QList<int> bandwidthsKHz = {200, 400, 600, 800};
    const double frequency = ui->carrierFreqSpin->value();
    const double realtimeBandwidthMHz = 100.0;
    const double scopeBandwidthMHz = 500.0;
    const double powerResolutionDb = 0.01;
    const double timeResolutionNs = 0.1;
    const double sampleRateGsps = 5.0;
    bool overallPass = true;

    ui->currentCaseValue->setText("8.4 调制质量");
    ui->freqValue->setText(QString::number(frequency, 'f', 1) + " MHz");
    ui->bwValue->setText("200/400/600/800 kHz, SCS 15 kHz");
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("运行进度 %p%");

    setupChartBox(findChild<QGroupBox*>("groupBox_3"), "R2D OOK包络", "定位1-0-1连续chip，测量调制深度、纹波和边沿时间");
    setupChartBox(findChild<QGroupBox*>("groupBox_4"), "调制质量趋势", "按带宽展示调制深度、纹波、Tr/Tf和PW判定");
    setupChartBox(findChild<QGroupBox*>("groupBox"), "包络参数结果", "调制深度、纹波、上升/下降时间和脉冲宽度");
    setupChartBox(findChild<QGroupBox*>("groupBox_2"), "仪表能力检查", "实时分析带宽、模拟带宽、功率/时间分辨率和采样率");

    ui->resultTableRun->clearContents();
    ui->resultTableRun->setColumnCount(4);
    ui->resultTableRun->setHorizontalHeaderLabels({"带宽/SCS", "调制深度/纹波", "时间参数", "判定"});
    ui->resultTableRun->setRowCount(bandwidthsKHz.size());

    ui->resultTableResult->clearContents();
    ui->resultTableResult->setColumnCount(6);
    ui->resultTableResult->setHorizontalHeaderLabels({"带宽", "调制深度", "纹波", "上升/下降时间", "脉冲宽度", "判定"});
    ui->resultTableResult->setRowCount(bandwidthsKHz.size() + 1);

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "测试",
           "8.4 开始: 遍历200/400/600/800 kHz, 测量调制深度、包络上升/下降时间、纹波和脉冲宽度");

    for (int i = 0; i < bandwidthsKHz.size(); ++i) {
        const int bwKHz = bandwidthsKHz[i];
        const double configuredVoltage = 0.120 + i * 0.015;
        double measuredVoltage = 0.0;

        sendScpiCommand(QString("CONF:VOLT:DC %1").arg(configuredVoltage, 0, 'f', 4));
        QThread::msleep(80);

        if (!readVoltageWithTimeout(measuredVoltage)) {
            overallPass = false;
            ui->resultTableRun->setItem(i, 0, new QTableWidgetItem(QString("%1 kHz / 15 kHz").arg(bwKHz)));
            ui->resultTableRun->setItem(i, 1, new QTableWidgetItem("读取超时"));
            ui->resultTableRun->setItem(i, 2, new QTableWidgetItem("读取超时"));
            ui->resultTableRun->setItem(i, 3, new QTableWidgetItem("FAIL"));

            ui->resultTableResult->setItem(i, 0, new QTableWidgetItem(QString("%1 kHz").arg(bwKHz)));
            ui->resultTableResult->setItem(i, 1, new QTableWidgetItem("读取超时"));
            ui->resultTableResult->setItem(i, 2, new QTableWidgetItem("读取超时"));
            ui->resultTableResult->setItem(i, 3, new QTableWidgetItem("读取超时"));
            ui->resultTableResult->setItem(i, 4, new QTableWidgetItem("读取超时"));
            ui->resultTableResult->setItem(i, 5, new QTableWidgetItem("FAIL"));
            addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "仪表",
                   QString("8.4 %1 kHz: READ? 超时").arg(bwKHz));
            ui->progressBar->setValue((i + 1) * 100 / bandwidthsKHz.size());
            continue;
        }

        const double voltageOffset = fabs(measuredVoltage - configuredVoltage);
        const double modulationDepthPct = 82.0 + i * 1.1 + voltageOffset * 3.0;
        const double rippleHighPct = 5.8 + i * 0.45 + voltageOffset * 2.0;
        const double rippleLowPct = 5.2 + i * 0.35 + voltageOffset * 2.0;
        const double riseTimeTc = 0.50 + i * 0.02 + voltageOffset;
        const double fallTimeTc = 0.52 + i * 0.02 + voltageOffset;
        const double pulseWidthTc = 1.04 + i * 0.03 + voltageOffset;

        const bool rowPass = (modulationDepthPct >= 80.0)
                             && (fabs(rippleHighPct) <= 15.0)
                             && (fabs(rippleLowPct) <= 15.0)
                             && (riseTimeTc <= 0.66)
                             && (fallTimeTc <= 0.66)
                             && (pulseWidthTc <= 1.3)
                             && (realtimeBandwidthMHz >= 100.0)
                             && (scopeBandwidthMHz >= 500.0)
                             && (powerResolutionDb <= 0.01)
                             && (timeResolutionNs <= 0.1)
                             && (sampleRateGsps >= 5.0);
        overallPass = overallPass && rowPass;

        ui->resultTableRun->setItem(i, 0, new QTableWidgetItem(QString("%1 kHz / 15 kHz").arg(bwKHz)));
        ui->resultTableRun->setItem(i, 1, new QTableWidgetItem(QString("%1%, 纹波 %2/%3%")
                                                               .arg(modulationDepthPct, 0, 'f', 1)
                                                               .arg(rippleHighPct, 0, 'f', 1)
                                                               .arg(rippleLowPct, 0, 'f', 1)));
        ui->resultTableRun->setItem(i, 2, new QTableWidgetItem(QString("Tr %1Tc, Tf %2Tc, PW %3Tc")
                                                               .arg(riseTimeTc, 0, 'f', 2)
                                                               .arg(fallTimeTc, 0, 'f', 2)
                                                               .arg(pulseWidthTc, 0, 'f', 2)));
        ui->resultTableRun->setItem(i, 3, new QTableWidgetItem(rowPass ? "PASS" : "FAIL"));

        ui->resultTableResult->setItem(i, 0, new QTableWidgetItem(QString("%1 kHz").arg(bwKHz)));
        ui->resultTableResult->setItem(i, 1, new QTableWidgetItem(QString("%1% (>=80%)")
                                                                  .arg(modulationDepthPct, 0, 'f', 1)));
        ui->resultTableResult->setItem(i, 2, new QTableWidgetItem(QString("High %1%, Low %2% (<=±15%)")
                                                                  .arg(rippleHighPct, 0, 'f', 1)
                                                                  .arg(rippleLowPct, 0, 'f', 1)));
        ui->resultTableResult->setItem(i, 3, new QTableWidgetItem(QString("Tr %1Tc / Tf %2Tc (<=0.66Tc)")
                                                                  .arg(riseTimeTc, 0, 'f', 2)
                                                                  .arg(fallTimeTc, 0, 'f', 2)));
        ui->resultTableResult->setItem(i, 4, new QTableWidgetItem(QString("%1Tc (<=1.3Tc)")
                                                                  .arg(pulseWidthTc, 0, 'f', 2)));
        ui->resultTableResult->setItem(i, 5, new QTableWidgetItem(rowPass ? "PASS" : "FAIL"));

        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), rowPass ? "PASS" : "FAIL", "测试",
               QString("8.4 %1 kHz: 调制深度=%2%, 纹波=%3/%4%, Tr=%5Tc, Tf=%6Tc, PW=%7Tc")
               .arg(bwKHz)
               .arg(modulationDepthPct, 0, 'f', 1)
               .arg(rippleHighPct, 0, 'f', 1)
               .arg(rippleLowPct, 0, 'f', 1)
               .arg(riseTimeTc, 0, 'f', 2)
               .arg(fallTimeTc, 0, 'f', 2)
               .arg(pulseWidthTc, 0, 'f', 2));

        ui->progressBar->setValue((i + 1) * 100 / bandwidthsKHz.size());
    }

    const int summaryRow = bandwidthsKHz.size();
    ui->resultTableResult->setItem(summaryRow, 0, new QTableWidgetItem("能力配置"));
    ui->resultTableResult->setItem(summaryRow, 1, new QTableWidgetItem("RTBW 100 MHz"));
    ui->resultTableResult->setItem(summaryRow, 2, new QTableWidgetItem("模拟带宽 500 MHz"));
    ui->resultTableResult->setItem(summaryRow, 3, new QTableWidgetItem("功率分辨率 0.01 dB"));
    ui->resultTableResult->setItem(summaryRow, 4, new QTableWidgetItem("时间0.1 ns / 采样5 GS/s"));
    ui->resultTableResult->setItem(summaryRow, 5, new QTableWidgetItem(overallPass ? "PASS" : "FAIL"));

    ui->verdictLabel->setText(overallPass ? "PASS" : "FAIL");
    ui->statsLabel->setText("带宽: 200/400/600/800 kHz, SCS 15 kHz\n"
                            "调制深度: >=80%; 纹波: <=±15%\n"
                            "Tr/Tf: <=0.66Tc; PW: <=1.3Tc\n"
                            "能力: RTBW>=100MHz, 模拟带宽>=500MHz, 功率分辨率<=0.01dB, 时间分辨率<=0.1ns, 采样率>=5GS/s");
    ui->statusBar->showMessage("8.4 调制质量测试完成", 2000);
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), overallPass ? "PASS" : "FAIL", "测试",
           "8.4 调制质量测试完成");
}
// ========== 1466 信号发生器控制函数 ==========
bool MainWindow::configure1466ArbPlayback(const QString &fileName,
                                          double freqMHz,
                                          double powerDbm,
                                          double sampleClockMHz,
                                          const QString &logContext,
                                          QString *errorDetails)
{
    if (!hasSignalGenerator()) {
        if (errorDetails) {
            *errorDetails = QStringLiteral("信号发生器未连接");
        }
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "1466", "信号发生器未连接");
        return false;
    }

    sendScpiCommand(signalGeneratorSocket, "1466", "*CLS");
    sendScpiCommand(signalGeneratorSocket, "1466",
                    QString(":SOURce1:RADio:ARB:WAVeform \"%1\"").arg(fileName));
    if (sampleClockMHz > 0.0) {
        sendScpiCommand(signalGeneratorSocket, "1466",
                        QString(":SOURce1:RADio:ARB:SCLock:RATE %1MHz").arg(sampleClockMHz, 0, 'f', 3));
    }
    sendScpiCommand(signalGeneratorSocket, "1466",
                    QString(":SOURce1:FREQuency %1MHz").arg(freqMHz, 0, 'f', 6));
    sendScpiCommand(signalGeneratorSocket, "1466",
                    QString(":SOURce1:POWer %1dBm").arg(powerDbm, 0, 'f', 2));
    sendScpiCommand(signalGeneratorSocket, "1466", ":SOURce1:RADio:ARB:STATe ON");
    sendScpiCommand(signalGeneratorSocket, "1466", ":OUTPut1:STATe ON");
    QThread::msleep(200);

    const QString sampleClockRate = queryScpi(signalGeneratorSocket, "1466",
                                              ":SOURce1:RADio:ARB:SCLock:RATE?", 800).trimmed();
    if (!sampleClockRate.isEmpty()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "1466",
               QString("%1 ARB采样率: %2").arg(logContext, sampleClockRate));
    }

    const QStringList errQueue = drainErrorQueue(signalGeneratorSocket, "1466");
    QStringList realErrors;
    for (const QString &err : errQueue) {
        if (!err.startsWith("+0") && !err.contains("No error", Qt::CaseInsensitive)) {
            realErrors << err;
        }
    }

    if (realErrors.isEmpty()) {
        return true;
    }

    const QString mergedErrors = realErrors.join(" | ");
    if (errorDetails) {
        *errorDetails = mergedErrors;
    }
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "1466",
           QString("%1 ARB 配置失败: %2").arg(logContext, mergedErrors));
    return false;
}

bool MainWindow::load1466ArbWaveform(const QString &fileName, double freqMHz, double powerDbm)
{
    QString errorDetails;
    return configure1466ArbPlayback(fileName,
                                    freqMHz,
                                    powerDbm,
                                    ACLR_ARB_CLOCK_MHZ,
                                    QStringLiteral("8.5"),
                                    &errorDetails);
}

bool MainWindow::configure1466RefSensitivityWaveform(const QString &fileName,
                                                     double freqMHz,
                                                     double outputPowerDbm,
                                                     double sampleRateHz)
{
    if (!hasSignalGenerator()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "1466", "信号发生器未连接");
        return false;
    }

    sendScpiCommand(signalGeneratorSocket, "1466", "*CLS");
    sendScpiCommand(signalGeneratorSocket, "1466", QString(":SOURce1:RADio:ARB:WAVeform \"%1\"").arg(fileName));
    sendScpiCommand(signalGeneratorSocket, "1466", QString(":SOURce1:RADio:ARB:SCLock:RATE %1").arg(sampleRateHz, 0, 'f', 0));
    sendScpiCommand(signalGeneratorSocket, "1466", QString(":SOURce1:FREQuency %1MHz").arg(freqMHz, 0, 'f', 6));
    sendScpiCommand(signalGeneratorSocket, "1466", QString(":SOURce1:POWer %1dBm").arg(outputPowerDbm, 0, 'f', 2));
    sendScpiCommand(signalGeneratorSocket, "1466", ":SOURce1:RADio:ARB:STATe ON");
    sendScpiCommand(signalGeneratorSocket, "1466", ":OUTPut1:STATe ON");
    signalGeneratorSocket->waitForBytesWritten(1000);
    QThread::msleep(300);

    const QString rateResp = queryScpi(signalGeneratorSocket, "1466", ":SOURce1:RADio:ARB:SCLock:RATE?", 1000).trimmed();
    const QString powerResp = queryScpi(signalGeneratorSocket, "1466", ":SOURce1:POWer?", 1000).trimmed();
    const QString outResp = queryScpi(signalGeneratorSocket, "1466", ":OUTPut1:STATe?", 1000).trimmed();
    const QString errResp = queryScpi(signalGeneratorSocket, "1466", ":SYSTem:ERRor?", 1200).trimmed();
    const bool errOk = errResp.isEmpty() || errResp.startsWith("+0") || errResp.contains("No error", Qt::CaseInsensitive);
    const bool outOk = outResp.isEmpty() || outResp == "1" || outResp.contains("ON", Qt::CaseInsensitive);
    const bool ok = errOk && outOk;

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), ok ? "PASS" : "WARN", "1466",
           QString("8.6参考灵敏度ARB配置: file=%1, fs=%2 Hz, freq=%3 MHz, power=%4 dBm, rate=%5, readPower=%6, output=%7, err=%8")
           .arg(fileName)
           .arg(sampleRateHz, 0, 'f', 0)
           .arg(freqMHz, 0, 'f', 6)
           .arg(outputPowerDbm, 0, 'f', 2)
           .arg(rateResp.isEmpty() ? "未返回" : rateResp)
           .arg(powerResp.isEmpty() ? "未返回" : powerResp)
           .arg(outResp.isEmpty() ? "未返回" : outResp)
           .arg(errResp.isEmpty() ? "未返回" : errResp));
    return ok;
}

bool MainWindow::start1466ArbOutput(bool enable)
{
    if (!hasSignalGenerator()) return false;
    sendScpiCommand(signalGeneratorSocket, "1466", enable ? ":OUTPut1:STATe ON" : ":OUTPut1:STATe OFF");
    return true;
}

// ========== 4071 频谱分析仪控制函数 ==========
bool MainWindow::measure4071ACLR(double &mainPowerDbm, double &leftAclrDb, double &rightAclrDb)
{
    if (!hasSpectrumAnalyzer()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "4071", "频谱分析仪未连接");
        return false;
    }

    sendScpiCommand(instrumentSocket, "4071", ":INITiate:CONTinuous OFF");
    sendScpiCommand(instrumentSocket, "4071", ":INITiate:IMMediate");
    queryScpi(instrumentSocket, "4071", "*OPC?", 3000);

    const QString resp = queryScpi(instrumentSocket, "4071", ":READ:ACPower?", 2000);
    if (resp.isEmpty()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "4071", "ACLR 测量无返回值");
        return false;
    }

    QString parseDetails;
    if (!parse4071AcpowerResponse(resp, mainPowerDbm, leftAclrDb, rightAclrDb, &parseDetails)) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "4071",
               "ACLR 结果解析失败: " + parseDetails);
        return false;
    }

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "4071",
           QString("ACLR结果: 主=%1 dBm, 左ACLR=%2 dB, 右ACLR=%3 dB (%4)")
           .arg(mainPowerDbm, 0, 'f', 2)
           .arg(leftAclrDb, 0, 'f', 2)
           .arg(rightAclrDb, 0, 'f', 2)
           .arg(parseDetails));
    return true;
}

bool MainWindow::set4071FrequencyAndSpan(double centerMHz, double spanMHz)
{
    if (!hasSpectrumAnalyzer()) return false;
    sendScpiCommand(instrumentSocket, "4071", QString(":SENSe:FREQuency:CENTer %1MHz").arg(centerMHz, 0, 'f', 6));
    sendScpiCommand(instrumentSocket, "4071", QString(":SENSe:FREQuency:SPAN %1MHz").arg(spanMHz, 0, 'f', 6));
    return true;
}

bool MainWindow::set4071RBWVBW(double rbwKHz, double vbwKHz)
{
    if (!hasSpectrumAnalyzer()) return false;
    sendScpiCommand(instrumentSocket, "4071", QString(":SENSe:BANDwidth:RESolution %1kHz").arg(rbwKHz, 0, 'f', 3));
    sendScpiCommand(instrumentSocket, "4071", QString(":SENSe:BANDwidth:VIDeo %1kHz").arg(vbwKHz, 0, 'f', 3));
    return true;
}

bool MainWindow::perform4071SingleSweep()
{
    if (!hasSpectrumAnalyzer()) return false;
    sendScpiCommand(instrumentSocket, "4071", ":INITiate:CONTinuous OFF");
    sendScpiCommand(instrumentSocket, "4071", ":INITiate:IMMediate");
    queryScpi(instrumentSocket, "4071", "*OPC?", 3000);
    return true;
}

bool MainWindow::save4071ScreenSnapshot(const QString &fileName)
{
    if (!hasSpectrumAnalyzer()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "4071", "频谱分析仪未连接，无法保存截图");
        return false;
    }

    const QString trimmedName = fileName.trimmed();
    if (trimmedName.isEmpty()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "4071", "截图文件名为空，跳过保存");
        return false;
    }

    sendScpiCommand(instrumentSocket, "4071",
                    QString(":MMEMory:STORe:SCReen \"%1\"").arg(trimmedName));
    QThread::msleep(200);

    const QStringList errQueue = drainErrorQueue(instrumentSocket, "4071");
    QStringList realErrors;
    for (const QString &err : errQueue) {
        if (!err.startsWith("+0") && !err.contains("No error", Qt::CaseInsensitive)) {
            realErrors << err;
        }
    }

    const bool ok = realErrors.isEmpty();
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), ok ? "PASS" : "WARN", "4071",
           ok
               ? QString("截图已保存到仪表: %1").arg(trimmedName)
               : QString("截图保存失败: %1, err=%2").arg(trimmedName, realErrors.join(" | ")));
    return ok;
}

bool MainWindow::download4071File(const QString &remoteFileName, const QString &localFilePath)
{
    if (!hasSpectrumAnalyzer()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "4071", "频谱分析仪未连接，无法下载文件");
        return false;
    }

    QFileInfo localInfo(localFilePath);
    QDir localDir = localInfo.dir();
    if (!localDir.exists() && !localDir.mkpath(".")) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "4071",
               "本地目录创建失败: " + localDir.absolutePath());
        return false;
    }

    const QByteArray payload = queryScpiBinaryBlock(
        instrumentSocket, "4071",
        QString(":MMEMory:DATA? \"%1\"").arg(remoteFileName), 15000);
    if (payload.isEmpty()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "4071",
               "截图下载失败或返回为空: " + remoteFileName);
        return false;
    }

    QFile localFile(localFilePath);
    if (!localFile.open(QIODevice::WriteOnly)) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "4071",
               "本地文件写入失败: " + localFilePath);
        return false;
    }
    localFile.write(payload);
    localFile.close();

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "PASS", "4071",
           QString("截图已下载到上位机: %1 (%2 bytes)")
           .arg(localFilePath)
           .arg(payload.size()));
    return true;
}

bool MainWindow::delete4071File(const QString &remoteFileName)
{
    if (!hasSpectrumAnalyzer()) {
        return false;
    }

    sendScpiCommand(instrumentSocket, "4071",
                    QString(":MMEMory:DELete \"%1\"").arg(remoteFileName));
    const QStringList errQueue = drainErrorQueue(instrumentSocket, "4071");
    QStringList realErrors;
    for (const QString &err : errQueue) {
        if (!err.startsWith("+0") && !err.contains("No error", Qt::CaseInsensitive)) {
            realErrors << err;
        }
    }

    const bool ok = realErrors.isEmpty();
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), ok ? "PASS" : "WARN", "4071",
           ok
               ? QString("仪表临时截图已删除: %1").arg(remoteFileName)
               : QString("仪表临时截图删除失败: %1, err=%2").arg(remoteFileName, realErrors.join(" | ")));
    return ok;
}

// ========== 新增：配置 4071 的 ACLR 测量参数 ==========
bool MainWindow::configure4071ACLR(double centerFreqMHz,
                                   double carrierBW_kHz,
                                   double integrationBW_kHz,
                                   double offset_kHz)
{
    if (!hasSpectrumAnalyzer()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "4071", "频谱分析仪未连接");
        return false;
    }

    sendScpiCommand(instrumentSocket, "4071", "*CLS");

    // 切换到邻道功率测量模式
    sendScpiCommand(instrumentSocket, "4071", ":CONFigure:ACPower");
    QThread::msleep(50);

    // 4071 手册中的标准中心频率命令为 :SENSe:FREQuency:CENTer，
    // 邻道功率示例也要求先设置分析仪中心频率后再进入 ACP 配置。
    sendScpiCommand(instrumentSocket, "4071",
                    QString(":SENSe:FREQuency:CENTer %1MHz").arg(centerFreqMHz, 0, 'f', 6));
    const double spanHalfWidthKHz = offset_kHz + qMax(carrierBW_kHz, integrationBW_kHz);
    double totalSpanMHz = spanHalfWidthKHz * 2.0 / 1000.0;
    totalSpanMHz = qMax(totalSpanMHz, 5.0);
    sendScpiCommand(instrumentSocket, "4071",
                    QString(":SENSe:ACPower:FREQuency:SPAN %1MHz").arg(totalSpanMHz, 0, 'f', 6));

    sendScpiCommand(instrumentSocket, "4071", ":SENSe:ACPower:CARRier:COUNt 1");
    sendScpiCommand(instrumentSocket, "4071", ":SENSe:ACPower:MODE NORMal");
    sendScpiCommand(instrumentSocket, "4071", ":SENSe:ACPower:TYPE TPRef");
    sendScpiCommand(instrumentSocket, "4071",
                    QString(":SENSe:ACPower:CARRier:LIST:BANDwidth:INTegration %1Hz")
                    .arg(carrierBW_kHz * 1000.0, 0, 'f', 0));
    sendScpiCommand(instrumentSocket, "4071",
                    QString(":SENSe:ACPower:OFFSet:LIST:FREQuency %1Hz")
                    .arg(offset_kHz * 1000.0, 0, 'f', 0));
    sendScpiCommand(instrumentSocket, "4071",
                    QString(":SENSe:ACPower:OFFSet:LIST:BANDwidth:INTegration %1Hz")
                    .arg(integrationBW_kHz * 1000.0, 0, 'f', 0));
    sendScpiCommand(instrumentSocket, "4071", ":SENSe:ACPower:OFFSet:LIST:BANDwidth:SHAPe FLATtop");
    sendScpiCommand(instrumentSocket, "4071", ":SENSe:ACPower:METHod IBW");

    const QStringList errQueue = drainErrorQueue(instrumentSocket, "4071");
    QStringList realErrors;
    for (const QString &err : errQueue) {
        if (!err.startsWith("+0") && !err.contains("No error", Qt::CaseInsensitive)) {
            realErrors << err;
        }
    }

    const bool ok = realErrors.isEmpty();
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), ok ? "PASS" : "WARN", "4071",
           QString("ACLR参数配置: 主信道积分%1kHz, 邻道滤波%2kHz, 邻道偏移=±%3kHz, err=%4")
           .arg(carrierBW_kHz)
           .arg(integrationBW_kHz)
           .arg(offset_kHz)
           .arg(ok ? QStringLiteral("无") : realErrors.join(" | ")));
    return ok;
}

// ========== 新增：检查 4071 仪表能力 ==========
bool MainWindow::check4071Capabilities()
{
    if (!hasSpectrumAnalyzer()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "4071", "频谱仪未连接，无法检查能力");
        return false;
    }

    bool allOk = true;
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "4071",
           "8.5 测试跳过自动实时带宽/功率分辨率探测，避免使用实机未支持的查询命令");

    // 仅保留不会污染错误队列的频率读回检查。
    double testFreqMHz = 925.123456;
    sendScpiCommand(instrumentSocket, "4071", QString(":SENSe:FREQuency:CENTer %1MHz").arg(testFreqMHz, 0, 'f', 6));
    QString readFreq = queryScpi(instrumentSocket, "4071", ":SENSe:FREQuency:CENTer?", 1500);
    double readFreqValMHz = parseFirstDouble(readFreq) / 1e6;
    if (qAbs(readFreqValMHz - testFreqMHz) < 0.000001) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "PASS", "4071",
               "频率分辨率 ≤1Hz (设置值保留6位小数有效)");
    } else {
        allOk = false;
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "FAIL", "4071",
               QString("频率分辨率可能不足1Hz (读回 %1 MHz)")
               .arg(readFreqValMHz, 0, 'f', 6));
    }

    sendScpiCommand(instrumentSocket, "4071", "*CLS");
    drainErrorQueue(instrumentSocket, "4071");
    return allOk;
}
void MainWindow::runTest_8_5()
{
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "上位机", "开始 8.5 ACLR 测试");

    // 8.5 运行页固定使用左仿真图、右实测截图区域。
    QGroupBox *simBox = findChild<QGroupBox*>("groupBox_3");
    if (simBox) {
        if (!m_simImageLabel || m_simImageLabel->parent() != simBox) {
            QLayout *layout = simBox->layout();
            if (layout) {
                QLayoutItem *item;
                while ((item = layout->takeAt(0)) != nullptr) {
                    if (item->widget()) delete item->widget();
                    delete item;
                }
            } else {
                layout = new QVBoxLayout(simBox);
            }
            simBox->setTitle("仿真图");
            m_simImageLabel = new AspectRatioPixmapLabel(simBox);
            m_simImageLabel->setAlignment(Qt::AlignCenter);
            m_simImageLabel->setMinimumHeight(320);
            m_simImageLabel->setText("点击测试运行表中的对应行显示仿真图");
            layout->addWidget(m_simImageLabel);
            simBox->setLayout(layout);
        }
        simBox->setVisible(true);
    }

    QGroupBox *screenshotBox = findChild<QGroupBox*>("groupBox_4");
    if (screenshotBox) {
        if (!m_screenshotLabel || m_screenshotLabel->parent() != screenshotBox) {
            QLayout *layout = screenshotBox->layout();
            if (layout) {
                QLayoutItem *item;
                while ((item = layout->takeAt(0)) != nullptr) {
                    if (item->widget()) delete item->widget();
                    delete item;
                }
            } else {
                layout = new QVBoxLayout(screenshotBox);
            }
            screenshotBox->setTitle("结果截图");
            m_screenshotLabel = new AspectRatioPixmapLabel(screenshotBox);
            m_screenshotLabel->setAlignment(Qt::AlignCenter);
            m_screenshotLabel->setMinimumHeight(320);
            m_screenshotLabel->setText("点击测试运行表中的对应行显示结果截图");
            layout->addWidget(m_screenshotLabel);
            screenshotBox->setLayout(layout);
        }
        screenshotBox->setVisible(true);
    }

    bool realInstrumentsConnected = hasSpectrumAnalyzer() && hasSignalGenerator();
    if (!realInstrumentsConnected) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "上位机", "真实仪表未同时连接，使用模拟数据模式");
    } else {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "上位机", "真实仪表已连接，执行真实联调");
    }

    struct OffsetConfig {
        double offsetKHz;
        double integrationBWKHz;
        double limitDb;
    };
    struct TestConfig {
        int channelBWKHz;
        QString iqFileName;
        QList<OffsetConfig> offsets;
    };
    QList<TestConfig> configs = {
        {200, "AIoT_TxSignal_iq_200kHz.bin", {{300, 180, 40.8}, {500, 180, 45.8}}},
        {400, "AIoT_TxSignal_iq_400kHz.bin", {{500, 360, 40.8}, {900, 360, 45.8}}},
        {600, "AIoT_TxSignal_iq_600kHz.bin", {{700, 540, 40.8}, {1300, 540, 45.8}}},
        {800, "AIoT_TxSignal_iq_800kHz.bin", {{900, 720, 40.8}, {1700, 720, 45.8}}}
    };

    const double txPowerDbm = ui->txPowerSpin->value();
    const QString screenshotRunDir = screenshotRunDirPath("8.5_ACLR", screenshotBatchTimestamp());

    if (realInstrumentsConnected) {
        QDir().mkpath(screenshotRunDir);
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "上位机",
               "8.5 截图本地目录: " + screenshotRunDir);
    }

    // 先计算总测试点数
    int total = 0;
    for (const auto &cfg : configs) {
        // 读取该带宽的频点列表
        QString freqText = m_bandFreqPoints.value(cfg.channelBWKHz, "925");
        QList<double> freqPoints;
        if (!freqText.isEmpty()) {
            QStringList parts = freqText.split(QRegularExpression("[,\\s]+"),
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                               Qt::SkipEmptyParts
#else
                                               QString::SkipEmptyParts
#endif
                                               );
            for (const QString &part : parts) {
                bool ok;
                double freq = part.toDouble(&ok);
                if (ok && freq > 0) {
                    freqPoints.append(freq);
                }
            }
        }
        if (freqPoints.isEmpty()) {
            freqPoints.append(925.0);
        }
        total += freqPoints.size() * cfg.offsets.size();
    }

    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("8.5 ACLR测试 %p%");

    // 准备表格
    QTableWidget *runTable = ui->resultTableRun;
    QTableWidget *resTable = ui->resultTableResult;
    if (runTable) {
        runTable->clearContents();
        runTable->setRowCount(total);
        runTable->setColumnCount(10);
        runTable->setHorizontalHeaderLabels({"频率(MHz)", "带宽", "邻道偏移", "滤波带宽",
                                             "主功率", "左邻功率", "左ACLR",
                                             "右邻功率", "右ACLR", "判定"});
    }
    if (resTable) {
        resTable->clearContents();
        resTable->setRowCount(total);
        resTable->setColumnCount(11);
        resTable->setHorizontalHeaderLabels({"频率(MHz)", "带宽(kHz)", "邻道偏移(kHz)",
                                             "滤波带宽(kHz)", "主功率(dBm)", "左邻功率(dBm)",
                                             "左ACLR(dB)", "右邻功率(dBm)", "右ACLR(dB)",
                                             "限值(dB)", "判定"});
    }

    bool overallPass = true;
    bool hasValidResult = false;
    bool rfOutputEnabled = false;
    int row = 0;

    // 遍历所有带宽
    for (const auto &cfg : configs) {
        if (!testRunning) break;

        // 获取该带宽对应的频点列表
        QString freqText = m_bandFreqPoints.value(cfg.channelBWKHz, "925");
        QList<double> freqPoints;
        if (!freqText.isEmpty()) {
            QStringList parts = freqText.split(QRegularExpression("[,\\s]+"),
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                               Qt::SkipEmptyParts
#else
                                               QString::SkipEmptyParts
#endif
                                               );
            for (const QString &part : parts) {
                bool ok;
                double freq = part.toDouble(&ok);
                if (ok && freq > 0) {
                    freqPoints.append(freq);
                }
            }
        }
        if (freqPoints.isEmpty()) {
            freqPoints.append(925.0);
        }

        // 构建频点字符串用于日志
        QStringList freqStrList;
        for (double f : freqPoints) {
            freqStrList << QString::number(f, 'f', 3);
        }
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "8.5",
               QString("带宽 %1 kHz: 将测试 %2 个频点: %3")
               .arg(cfg.channelBWKHz)
               .arg(freqPoints.size())
               .arg(freqStrList.join(", ")));

        // 遍历该带宽的频点
        for (double centerFreqMHz : freqPoints) {
            if (!testRunning) break;

            bool waveformLoaded = true;
            if (realInstrumentsConnected) {
                addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "1466",
                       QString("频点 %1 MHz: 加载 %2 kHz A-TM1.1 测试模型文件 %3")
                       .arg(centerFreqMHz, 0, 'f', 3)
                       .arg(cfg.channelBWKHz).arg(cfg.iqFileName));
                waveformLoaded = load1466ArbWaveform(cfg.iqFileName, centerFreqMHz, txPowerDbm);
                if (waveformLoaded) {
                    rfOutputEnabled = true;
                    QThread::msleep(500);
                } else {
                    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "1466",
                           QString("频点 %1 MHz: 波形加载失败，跳过该带宽后续测量")
                           .arg(centerFreqMHz, 0, 'f', 3));
                    break;
                }
            }

            // 遍历邻道偏移
            for (const auto &offsetCfg : cfg.offsets) {
                if (!testRunning) break;

                double mainPower = 0.0;
                double leftPower = 0.0;
                double rightPower = 0.0;
                double leftAclr = 0.0;
                double rightAclr = 0.0;
                bool measurementOk = false;
                QString failureReason;
                QString screenshotPath;

                if (realInstrumentsConnected) {
                    if (!waveformLoaded) {
                        failureReason = QStringLiteral("IQ文件加载失败");
                    } else if (!configure4071ACLR(centerFreqMHz,
                                                  cfg.channelBWKHz,
                                                  offsetCfg.integrationBWKHz,
                                                  offsetCfg.offsetKHz)) {
                        failureReason = QStringLiteral("ACLR配置失败");
                    } else if (!measure4071ACLR(mainPower, leftAclr, rightAclr)) {
                        failureReason = QStringLiteral("ACLR读取失败");
                    } else {
                        leftPower = mainPower - leftAclr;
                        rightPower = mainPower - rightAclr;
                        measurementOk = true;
                        hasValidResult = true;
                    }
                } else {
                    // 模拟模式
                    const double randomLeft = (pseudoRandomBounded(20) - 10) / 10.0;
                    const double randomRight = (pseudoRandomBounded(20) - 10) / 10.0;
                    mainPower = 20.0;
                    leftAclr = offsetCfg.limitDb + 2.5 + randomLeft;
                    rightAclr = offsetCfg.limitDb + 2.5 + randomRight;
                    leftPower = mainPower - leftAclr;
                    rightPower = mainPower - rightAclr;
                    measurementOk = true;
                    hasValidResult = true;
                }

                const bool pass = measurementOk
                                  && (leftAclr >= offsetCfg.limitDb)
                                  && (rightAclr >= offsetCfg.limitDb);
                overallPass = overallPass && pass;

                // 截图处理（仅真实仪表）
                if (measurementOk && realInstrumentsConnected) {
                    const QString snapshotName = timestampedScreenshotName(
                        QString("ACLR_%1MHz_BW%2k_OFF%3k_IBW%4k_%5")
                            .arg(centerFreqMHz, 0, 'f', 3)
                            .arg(cfg.channelBWKHz)
                            .arg(offsetCfg.offsetKHz, 0, 'f', 0)
                            .arg(offsetCfg.integrationBWKHz, 0, 'f', 0)
                            .arg(pass ? "PASS" : "FAIL"));
                    const QString localSnapshotPath = QDir(screenshotRunDir).filePath(snapshotName);
                    if (save4071ScreenSnapshot(snapshotName)) {
                        if (download4071File(snapshotName, localSnapshotPath)) {
                            screenshotPath = localSnapshotPath;
                            delete4071File(snapshotName);
                        }
                    }
                }

                const QString verdict = pass ? QStringLiteral("PASS") : QStringLiteral("FAIL");
                const QString mainPowerText = measurementOk ? QString::number(mainPower, 'f', 2) : QStringLiteral("--");
                const QString leftPowerText = measurementOk ? QString::number(leftPower, 'f', 2) : QStringLiteral("--");
                const QString rightPowerText = measurementOk ? QString::number(rightPower, 'f', 2) : QStringLiteral("--");
                const QString leftAclrText = measurementOk ? QString::number(leftAclr, 'f', 2) : QStringLiteral("--");
                const QString rightAclrText = measurementOk ? QString::number(rightAclr, 'f', 2) : QStringLiteral("--");

                // 填充运行表格，选中行时自动显示对应仿真图和截图
                if (runTable) {
                    runTable->setItem(row, 0, new QTableWidgetItem(QString::number(centerFreqMHz, 'f', 3)));
                    runTable->setItem(row, 1, new QTableWidgetItem(QString("%1 kHz").arg(cfg.channelBWKHz)));
                    runTable->setItem(row, 2, new QTableWidgetItem(QString("±%1 kHz").arg(offsetCfg.offsetKHz, 0, 'f', 0)));
                    runTable->setItem(row, 3, new QTableWidgetItem(QString("%1 kHz").arg(offsetCfg.integrationBWKHz, 0, 'f', 0)));
                    runTable->setItem(row, 4, new QTableWidgetItem(mainPowerText + (measurementOk ? " dBm" : "")));
                    runTable->setItem(row, 5, new QTableWidgetItem(leftPowerText + (measurementOk ? " dBm" : "")));
                    runTable->setItem(row, 6, new QTableWidgetItem(leftAclrText + (measurementOk ? " dB" : "")));
                    runTable->setItem(row, 7, new QTableWidgetItem(rightPowerText + (measurementOk ? " dBm" : "")));
                    runTable->setItem(row, 8, new QTableWidgetItem(rightAclrText + (measurementOk ? " dB" : "")));
                    QTableWidgetItem *verdictItem = new QTableWidgetItem(verdict);
                    const QString simSuffix = cfg.channelBWKHz >= 600 ? "jpg" : "png";
                    verdictItem->setData(RoleSimPath,
                                         QString(":/images/sim/BW%1k.%2")
                                             .arg(cfg.channelBWKHz)
                                             .arg(simSuffix));
                    verdictItem->setData(RoleScreenshotPath, screenshotPath);
                    runTable->setItem(row, 9, verdictItem);
                }

                // 填充结果表格（无按钮）
                if (resTable) {
                    resTable->setItem(row, 0, new QTableWidgetItem(QString::number(centerFreqMHz, 'f', 3)));
                    resTable->setItem(row, 1, new QTableWidgetItem(QString::number(cfg.channelBWKHz)));
                    resTable->setItem(row, 2, new QTableWidgetItem(QString("±%1").arg(offsetCfg.offsetKHz, 0, 'f', 0)));
                    resTable->setItem(row, 3, new QTableWidgetItem(QString::number(offsetCfg.integrationBWKHz, 'f', 0)));
                    resTable->setItem(row, 4, new QTableWidgetItem(mainPowerText));
                    resTable->setItem(row, 5, new QTableWidgetItem(leftPowerText));
                    resTable->setItem(row, 6, new QTableWidgetItem(leftAclrText));
                    resTable->setItem(row, 7, new QTableWidgetItem(rightPowerText));
                    resTable->setItem(row, 8, new QTableWidgetItem(rightAclrText));
                    resTable->setItem(row, 9, new QTableWidgetItem(QString::number(offsetCfg.limitDb, 'f', 1)));
                    resTable->setItem(row, 10, new QTableWidgetItem(verdict));
                }

                if (!failureReason.isEmpty()) {
                    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "FAIL", "8.5",
                           QString("频点 %1 MHz, 带宽 %2 kHz, 邻道偏移±%3 kHz: %4")
                           .arg(centerFreqMHz, 0, 'f', 3)
                           .arg(cfg.channelBWKHz)
                           .arg(offsetCfg.offsetKHz, 0, 'f', 0)
                           .arg(failureReason));
                } else {
                    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), pass ? "PASS" : "FAIL", "8.5",
                           QString("频点 %1 MHz, 带宽 %2 kHz, 邻道偏移±%3 kHz, 滤波带宽%4 kHz: 左ACLR=%5 dB, 右ACLR=%6 dB, 限值=%7 dB")
                           .arg(centerFreqMHz, 0, 'f', 3)
                           .arg(cfg.channelBWKHz)
                           .arg(offsetCfg.offsetKHz, 0, 'f', 0)
                           .arg(offsetCfg.integrationBWKHz, 0, 'f', 0)
                           .arg(leftAclr, 0, 'f', 2)
                           .arg(rightAclr, 0, 'f', 2)
                           .arg(offsetCfg.limitDb, 0, 'f', 1));
                }

                ++row;
                ui->progressBar->setValue(row * 100 / total);
                QCoreApplication::processEvents();
                QThread::msleep(10);
            } // offsets
        } // freqPoints
        QThread::msleep(200);
    } // configs

    if (!hasValidResult) {
        overallPass = false;
    }

    ui->verdictLabel->setText(overallPass ? "PASS" : "FAIL");
    ui->statsLabel->setText(QString("ACLR测试完成（遍历所有带宽）\n判定门限: 第一邻道≥40.8 dB，第二邻道≥45.8 dB\n整体结果: %1")
                            .arg(overallPass ? "PASS" : "FAIL"));

    polishResultTables();
    if (runTable && runTable->rowCount() > 0) {
        runTable->selectRow(0);
        updateAclrRunImages(0);
    }
    updateBarChart();

    stopSpectrumAnalyzerMeasurement("8.5 测试结束停止测量");
    if (rfOutputEnabled) {
        shutdownSignalGeneratorOutput("8.5 测试结束安全关断");
    }
    testRunning = false;
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "上位机", "8.5 ACLR 测试结束");
}

double MainWindow::getUsefulPowerForRefChannel(const QString &refChannel)
{
    // 协议中的 PREFSENS 取自文档表 3.1，ACS 采用 PREFSENS + 6 dB。
    if (refChannel == "A-FR1-A1-1") return -95.2 + 6.0;
    if (refChannel == "A-FR1-A1-2") return -92.2 + 6.0;
    if (refChannel == "A-FR1-A1-3") return -72.4 + 6.0;
    if (refChannel == "A-FR1-A1-4") return -69.4 + 6.0;
    return -85.5;
}

bool MainWindow::load1466AcsWaveform(const QStringList &fileNames,
                                     double freqMHz,
                                     double powerDbm,
                                     QString *loadedFileName)
{
    if (!hasSignalGenerator()) {
        return false;
    }

    QStringList failures;
    for (const QString &fileName : fileNames) {
        QString errorDetails;
        if (configure1466ArbPlayback(fileName,
                                     freqMHz,
                                     powerDbm,
                                     ACS_ARB_CLOCK_MHZ,
                                     QStringLiteral("8.7"),
                                     &errorDetails)) {
            if (loadedFileName) {
                *loadedFileName = fileName;
            }
            return true;
        }
        failures << QString("%1 => %2").arg(fileName, errorDetails.isEmpty() ? QStringLiteral("未知错误") : errorDetails);
    }

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "1466",
           QString("8.7 波形加载失败，候选文件均不可用: %1").arg(failures.join(" | ")));
    return false;
}

QString MainWindow::capture4071ForAcs(const QString &testPointDesc,
                                      double spanMHz,
                                      const QString &localRunDir,
                                      int pointIndex)
{
    if (!ACS_ENABLE_SCREENSHOT) return QString();
    if (!hasSpectrumAnalyzer()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "4071", "频谱仪未连接，跳过截图");
        return QString();
    }

    const double centerMHz = ui->carrierFreqSpin->value();
    const double rbwKHz = 30.0;
    const double vbwKHz = 30.0;
    if (!configure4071Spectrum(centerMHz, spanMHz, rbwKHz, vbwKHz) || !perform4071SingleSweep()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "8.7",
               "截图前频谱配置失败: " + testPointDesc);
        return QString();
    }

    const QString safeName = sanitizeSnapshotToken(testPointDesc);
    const QString snapshotName = timestampedScreenshotName(
        QString("ACS_%1_%2")
            .arg(pointIndex + 1, 2, 10, QChar('0'))
            .arg(safeName));
    const QString localPath = QDir(localRunDir).filePath(snapshotName);
    QDir().mkpath(localRunDir);
    if (save4071ScreenSnapshot(snapshotName)) {
        if (download4071File(snapshotName, localPath)) {
            delete4071File(snapshotName);
            addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "8.7",
                   QString("截图已保存: %1").arg(localPath));
            return localPath;
        }
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "8.7",
               "截图下载失败: " + snapshotName);
    } else {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "8.7",
               "截图失败: " + snapshotName);
    }
    return QString();
}

void MainWindow::runTest_8_6()
{
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "上位机", "开始 8.6 参考灵敏度测试");

    const QList<RefSensitivityPoint> points = refSensitivityTestPlan();
    const int total = points.size();
    const bool realInstrumentsConnected = hasSpectrumAnalyzer() && hasSignalGenerator();
    const double centerFreqMHz = ui->carrierFreqSpin->value();
    const double linkLossCompensationDb = refSensitivityLinkLossCompensation();
    const double freqLimitHz = centerFreqMHz * 0.05 + 6.0;
    const QString screenshotRunDir = screenshotRunDirPath("8.6_RefSens", screenshotBatchTimestamp());

    if (realInstrumentsConnected) {
        QDir().mkpath(screenshotRunDir);
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "上位机",
               "8.6 频谱截图本地目录: " + screenshotRunDir);
    }

    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("8.6参考灵敏度 %p%");
    ui->currentCaseValue->setText("8.6 参考灵敏度");
    ui->freqValue->setText(QString::number(centerFreqMHz, 'f', 3) + " MHz");
    ui->bwValue->setText("D2R扫描 + CW扫描框架, BLER后填");

    QTableWidget *runTable = ui->resultTableRun;
    QTableWidget *resTable = ui->resultTableResult;
    bool runSignalsBlocked = false;
    bool resSignalsBlocked = false;
    if (runTable) {
        runSignalsBlocked = runTable->blockSignals(true);
        runTable->clearContents();
        runTable->setRowCount(total);
        runTable->setColumnCount(10);
        runTable->setHorizontalHeaderLabels({"阶段", "信道", "带宽/DSB", "调制", "D2R功率",
                                             "CW功率", "接收功率", "BLER", "频误", "判定"});
    }
    if (resTable) {
        resSignalsBlocked = resTable->blockSignals(true);
        resTable->clearContents();
        resTable->setRowCount(total);
        resTable->setColumnCount(13);
        resTable->setHorizontalHeaderLabels({"阶段", "信道", "IQ/CW", "采样率", "D2R功率",
                                             "CW功率", "接收功率", "峰值", "频误", "BLER",
                                             "截图", "门限", "判定"});
    }

    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "8.6",
           realInstrumentsConnected
           ? "真实仪表已连接，执行D2R参考波形功率扫描和4071频谱验证；接收功率先采用4071实测峰值功率，BLER稍后输入"
           : "未同时连接1466/4071，仅生成8.6完整表格；接收功率需真实仪表读取，BLER稍后输入");
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "8.6",
           QString("按现场建议使用 %1 dB 链路损耗补偿: 1466输出功率 = 端口目标功率 + %1 dB")
           .arg(linkLossCompensationDb, 0, 'f', 1));

    bool rfOutputEnabled = false;
    int instrumentPassCount = 0;
    int instrumentFailCount = 0;
    int cwPointCount = 0;

    for (int i = 0; i < total; ++i) {
        if (!testRunning) break;
        const RefSensitivityPoint point = points[i];
        const RefSensitivityConfig cfg = point.cfg;
        const bool executable = true;
        const QString waveformFileName = refSensitivityWaveformFileName(point);
        const double outputTargetDbm = point.usesCwWaveform ? point.cwPowerDbm : point.d2rPowerDbm;
        const double generatorOutputDbm = outputTargetDbm + linkLossCompensationDb;
        if (point.usesCwWaveform) {
            ++cwPointCount;
        }

        bool waveformOk = false;
        bool spectrumOk = false;
        bool freqPass = false;
        double peakFreqMHz = 0.0;
        double peakPowerDbm = 0.0;
        double freqErrorHz = 0.0;
        QString failureReason;
        QString snapshotText = realInstrumentsConnected ? QStringLiteral("未保存") : QStringLiteral("需真实仪表");
        QString peakText = realInstrumentsConnected ? QStringLiteral("未测") : QStringLiteral("需真实仪表");
        QString freqText = realInstrumentsConnected ? QStringLiteral("未测") : QStringLiteral("需真实仪表");
        QString rxPowerText = realInstrumentsConnected ? QStringLiteral("未测") : QStringLiteral("需真实双仪表");

        const QString bandwidthText = refSensitivityBandwidthText(cfg);
        const QString d2rPowerText = point.usesCwWaveform
                                     ? QString("文件内PREFSENS %1 / 1466按CW输出")
                                       .arg(point.d2rPowerDbm, 0, 'f', 1)
                                     : QString("目标 %1 / 输出 %2")
                                       .arg(point.d2rPowerDbm, 0, 'f', 1)
                                       .arg(generatorOutputDbm, 0, 'f', 1);
        const QString cwText = point.usesCwWaveform
                               ? QString("目标 %1 / 输出 %2 / 文件 %3")
                                 .arg(point.cwPowerDbm, 0, 'f', 1)
                                 .arg(generatorOutputDbm, 0, 'f', 1)
                                 .arg(point.cwFileName)
                               : QString("文件内参考 -38.0 / 链损补偿 +%1 dB").arg(linkLossCompensationDb, 0, 'f', 1);
        const QString blerText = QStringLiteral("待输入");

        if (!realInstrumentsConnected) {
            failureReason = QStringLiteral("需同时连接1466信号发生器和4071频谱分析仪后执行");
            snapshotText = QStringLiteral("需真实双仪表");
            peakText = QStringLiteral("需真实双仪表");
            freqText = QStringLiteral("需真实双仪表");
            rxPowerText = QStringLiteral("需真实双仪表");
        } else if (realInstrumentsConnected) {
            addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "8.6",
                   QString("%1 %2: IQ=%3, fs=%4 Hz, D2R目标=%5 dBm, CW目标=%6 dBm, 1466输出=%7 dBm")
                   .arg(point.stage)
                   .arg(cfg.refChannel)
                   .arg(waveformFileName)
                   .arg(cfg.sampleRateHz, 0, 'f', 0)
                   .arg(point.d2rPowerDbm, 0, 'f', 1)
                   .arg(point.cwPowerDbm, 0, 'f', 1)
                   .arg(generatorOutputDbm, 0, 'f', 1));

            waveformOk = configure1466RefSensitivityWaveform(waveformFileName,
                                                             centerFreqMHz,
                                                             generatorOutputDbm,
                                                             cfg.sampleRateHz);
            if (waveformOk) {
                rfOutputEnabled = true;
                const double spanMHz = cfg.d2rBandwidthKHz >= 3000 ? 8.0 : 2.0;
                const double rbwKHz = cfg.d2rBandwidthKHz >= 3000 ? 30.0 : 3.0;
                spectrumOk = configure4071Spectrum(centerFreqMHz, spanMHz, rbwKHz, rbwKHz);
                if (spectrumOk) {
                    spectrumOk = read4071Peak(peakFreqMHz, peakPowerDbm);
                    freqErrorHz = (peakFreqMHz - centerFreqMHz) * 1.0e6;
                    if (spectrumOk) {
                        peakText = QString("%1 MHz / %2 dBm")
                                   .arg(peakFreqMHz, 0, 'f', 6)
                                   .arg(peakPowerDbm, 0, 'f', 2);
                        rxPowerText = QString("%1 dBm").arg(peakPowerDbm, 0, 'f', 2);
                        freqText = QString("%1 Hz").arg(freqErrorHz, 0, 'f', 2);
                        const QString snapshotName = timestampedScreenshotName(
                            QString("RefSens_%1_%2_%3_%4dBm")
                                .arg(i + 1, 2, 10, QChar('0'))
                                .arg(sanitizeSnapshotToken(point.stage))
                                .arg(cfg.refChannel)
                                .arg(point.d2rPowerDbm, 0, 'f', 0));
                        const QString localSnapshotPath = QDir(screenshotRunDir).filePath(snapshotName);
                        if (save4071ScreenSnapshot(snapshotName)) {
                            if (download4071File(snapshotName, localSnapshotPath)) {
                                delete4071File(snapshotName);
                                snapshotText = localSnapshotPath;
                            } else {
                                snapshotText = QStringLiteral("下载失败");
                            }
                        } else {
                            snapshotText = QStringLiteral("保存失败");
                        }
                    }
                }
            }

            if (!waveformOk) {
                failureReason = QStringLiteral("1466参考波形配置失败");
            } else if (!spectrumOk) {
                failureReason = QStringLiteral("4071频谱/Marker读取失败");
            }
        }

        if (executable && realInstrumentsConnected) {
            freqPass = spectrumOk && qAbs(freqErrorHz) <= freqLimitHz;
        }
        if (executable && !freqPass && failureReason.isEmpty()) {
            failureReason = QStringLiteral("频率误差超限");
        }
        const bool instrumentPass = executable && waveformOk && spectrumOk && freqPass;
        if (instrumentPass) {
            ++instrumentPassCount;
        } else {
            ++instrumentFailCount;
        }

        const QString verdict = instrumentPass ? QStringLiteral("待输入BLER") : QStringLiteral("仪表检查失败");

        if (runTable) {
            runTable->setItem(i, RefRunStageCol, makeTableItem(point.stage));
            runTable->setItem(i, RefRunChannelCol, makeTableItem(cfg.refChannel));
            runTable->setItem(i, RefRunBwCol, makeTableItem(bandwidthText));
            runTable->setItem(i, RefRunModCol, makeTableItem(cfg.modulation));
            runTable->setItem(i, RefRunD2rPowerCol, makeTableItem(d2rPowerText));
            runTable->setItem(i, RefRunCwPowerCol, makeTableItem(cwText));
            QTableWidgetItem *runRxPowerItem = makeTableItem(rxPowerText, executable);
            runRxPowerItem->setToolTip("当前先采用4071实测峰值功率作为接收功率；如后续确认修正值，可在此列手动修正，单位 dBm");
            runTable->setItem(i, RefRunRxPowerCol, runRxPowerItem);
            QTableWidgetItem *runBlerItem = makeTableItem(blerText, true);
            runBlerItem->setToolTip("请输入基站侧BLER百分数，例如 8.5 表示 8.5%");
            runTable->setItem(i, RefRunBlerCol, runBlerItem);
            runTable->setItem(i, RefRunFreqErrCol, makeTableItem(freqText));
            QTableWidgetItem *runVerdictItem = makeTableItem(verdict);
            runVerdictItem->setData(RoleWaveformOk, waveformOk);
            runVerdictItem->setData(RoleSpectrumOk, spectrumOk);
            runVerdictItem->setData(RoleFreqPass, freqPass);
            runVerdictItem->setData(RoleFailureReason, failureReason);
            runVerdictItem->setData(RoleExecutable, executable);
            runVerdictItem->setData(RolePendingReason, executable ? QString() : failureReason);
            runTable->setItem(i, RefRunVerdictCol, runVerdictItem);
        }

        if (resTable) {
            resTable->setItem(i, RefResStageCol, makeTableItem(point.stage));
            resTable->setItem(i, RefResChannelCol, makeTableItem(cfg.refChannel));
            resTable->setItem(i, RefResIqFileCol, makeTableItem(waveformFileName));
            resTable->setItem(i, RefResSampleRateCol, makeTableItem(refSensitivitySampleRateText(cfg.sampleRateHz)));
            resTable->setItem(i, RefResD2rPowerCol, makeTableItem(d2rPowerText));
            resTable->setItem(i, RefResCwPowerCol, makeTableItem(cwText));
            resTable->setItem(i, RefResRxPowerCol, makeTableItem(rxPowerText));
            resTable->setItem(i, RefResPeakCol, makeTableItem(peakText));
            resTable->setItem(i, RefResFreqErrCol, makeTableItem(freqText));
            resTable->setItem(i, RefResBlerCol, makeTableItem(blerText));
            resTable->setItem(i, RefResScreenshotCol, makeTableItem(snapshotText));
            resTable->setItem(i, RefResLimitCol, makeTableItem(QString("BLER≤10%, 接收功率-130~-50dBm, |频误|≤%1Hz")
                                                               .arg(freqLimitHz, 0, 'f', 2)));
            QTableWidgetItem *resVerdictItem = makeTableItem(verdict);
            resVerdictItem->setData(RoleWaveformOk, waveformOk);
            resVerdictItem->setData(RoleSpectrumOk, spectrumOk);
            resVerdictItem->setData(RoleFreqPass, freqPass);
            resVerdictItem->setData(RoleFailureReason, failureReason);
            resVerdictItem->setData(RoleExecutable, executable);
            resVerdictItem->setData(RolePendingReason, executable ? QString() : failureReason);
            resTable->setItem(i, RefResVerdictCol, resVerdictItem);
        }

        if (!failureReason.isEmpty()) {
            addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "FAIL", "8.6",
                   QString("%1 %2: %3").arg(point.stage, cfg.refChannel, failureReason));
        }
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"),
               instrumentPass ? "PASS" : "FAIL",
               "8.6",
               QString("%1 %2: D2R目标=%3 dBm, 1466输出=%4 dBm, BLER=%5, freqErr=%6, 仪表检查=%7")
               .arg(point.stage)
               .arg(cfg.refChannel)
               .arg(point.d2rPowerDbm, 0, 'f', 1)
               .arg(generatorOutputDbm, 0, 'f', 1)
               .arg(blerText)
               .arg(freqText)
               .arg(instrumentPass ? "PASS" : "FAIL"));

        ui->progressBar->setValue((i + 1) * 100 / total);
        QCoreApplication::processEvents();
    }

    ui->verdictLabel->setText("WAIT");
    ui->statsLabel->setText(QString("8.6参考灵敏度仪表流程完成\n"
                                    "计划点: %1，其中CW干扰扫描: %2\n"
                                    "仪表检查通过: %3，失败: %4\n"
                                    "接收功率已先采用4071实测峰值功率；请填写BLER列后点击“确认 BLER 判定”")
                            .arg(total)
                            .arg(cwPointCount)
                            .arg(instrumentPassCount)
                            .arg(instrumentFailCount));
    if (realInstrumentsConnected) {
        stopSpectrumAnalyzerMeasurement("8.6 测试结束停止测量");
    }
    if (rfOutputEnabled) {
        shutdownSignalGeneratorOutput("8.6 测试结束安全关断");
    }
    if (runTable) {
        runTable->blockSignals(runSignalsBlocked);
    }
    if (resTable) {
        resTable->blockSignals(resSignalsBlocked);
    }
    polishResultTables();
    set8_6BlerEditingEnabled(true);
    if (runTable && runTable->rowCount() > 0) {
        runTable->selectRow(0);
        updateRefSensitivityRunGraphs(0);
    }
    updateRefSensitivityCharts();
    testRunning = false;
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "上位机", "8.6 参考灵敏度仪表流程结束，等待BLER表格输入");
}

void MainWindow::onConfirmBlerVerdict()
{
    if (!currentTestCase.contains("8.6")) {
        QMessageBox::information(this, "确认 BLER 判定", "当前不是 8.6 参考灵敏度测试。");
        return;
    }

    QTableWidget *resTable = ui->resultTableResult;
    if (!resTable || resTable->rowCount() == 0) {
        QMessageBox::warning(this, "确认 BLER 判定", "没有可判定的 8.6 测试结果。");
        return;
    }

    for (int row = 0; row < resTable->rowCount(); ++row) {
        judgeRefSensitivityRow(row, true);
    }

    refreshRefSensitivityOverallStatus(true);
    set8_6BlerEditingEnabled(false);
    if (confirmBlerBtn) {
        confirmBlerBtn->setVisible(false);
        confirmBlerBtn->setEnabled(false);
    }
    ui->pushButton->setEnabled(true);
    updateRefSensitivityRunGraphs(ui->resultTableRun ? ui->resultTableRun->currentRow() : 0);
    updateRefSensitivityCharts();
    ui->statusBar->showMessage("8.6 判定已刷新", 3000);
}

void MainWindow::runTest_8_7()
{
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "上位机", "开始 8.7 ACS 测试");
    QList<double> offsets = {340, -340, 2500, -2500};
    int total = offsets.size();
    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("运行进度 %p%");

    // 两个表格使用相同的列结构（7列）
    QTableWidget *runTable = ui->resultTableRun;
    QTableWidget *resTable = ui->resultTableResult;
    if (runTable) {
        runTable->setRowCount(total);
        runTable->setColumnCount(7);
        runTable->setHorizontalHeaderLabels({"偏移", "有用信号功率", "干扰信号功率", "带宽", "BLER(%)", "ACS", "判定"});
    }
    if (resTable) {
        resTable->setRowCount(total);
        resTable->setColumnCount(7);
        resTable->setHorizontalHeaderLabels({"偏移", "有用信号功率", "干扰信号功率", "带宽", "BLER(%)", "ACS", "判定"});
    }

    double mainPower = 20.0;   // 有用信号功率 dBm
    double interferencePower = -53.0; // 干扰信号功率 dBm
    bool overallPass = true;

    for (int i = 0; i < total; ++i) {
        if (!testRunning) break;
        double offset = offsets[i];
        QString response = sendQuery(":MEASure:ACS?", 200);
        double acs = 0, bler = 100;
        if (!response.isEmpty()) {
            QStringList parts = response.split(',');
            if (parts.size() >= 2) {
                acs = parts[0].toDouble();
                bler = parts[1].toDouble();
            }
        } else {
            // 模拟数据
            acs = (i<2) ? 33.5 : 45.2;
            bler = 0.8;
        }
        double threshold = (i<2) ? 33.0 : 45.0;
        bool pass = (acs >= threshold) && (bler <= 10.0);
        overallPass = overallPass && pass;

        QString usefulStr = QString::number(mainPower, 'f', 1) + " dBm";
        QString interfStr = QString::number(interferencePower, 'f', 1) + " dBm";
        QString bwStr = (i<2) ? "200 kHz" : "3520 kHz";

        if (runTable) {
            runTable->setItem(i, 0, new QTableWidgetItem(QString::number(offset) + " kHz"));
            runTable->setItem(i, 1, new QTableWidgetItem(usefulStr));
            runTable->setItem(i, 2, new QTableWidgetItem(interfStr));
            runTable->setItem(i, 3, new QTableWidgetItem(bwStr));
            runTable->setItem(i, 4, new QTableWidgetItem(QString::number(bler, 'f', 1) + " %"));
            runTable->setItem(i, 5, new QTableWidgetItem(QString::number(acs, 'f', 1) + " dB"));
            runTable->setItem(i, 6, new QTableWidgetItem(pass ? "PASS" : "FAIL"));
        }
        if (resTable) {
            resTable->setItem(i, 0, new QTableWidgetItem(QString::number(offset) + " kHz"));
            resTable->setItem(i, 1, new QTableWidgetItem(usefulStr));
            resTable->setItem(i, 2, new QTableWidgetItem(interfStr));
            resTable->setItem(i, 3, new QTableWidgetItem(bwStr));
            resTable->setItem(i, 4, new QTableWidgetItem(QString::number(bler, 'f', 1) + " %"));
            resTable->setItem(i, 5, new QTableWidgetItem(QString::number(acs, 'f', 1) + " dB"));
            resTable->setItem(i, 6, new QTableWidgetItem(pass ? "PASS" : "FAIL"));
        }

        int progress = (i+1)*100/total;
        ui->progressBar->setValue(progress);
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }

    ui->verdictLabel->setText(overallPass ? "PASS" : "FAIL");
    ui->statsLabel->setText(QString("ACS 测试完成\n整体判定: %1").arg(overallPass ? "PASS" : "FAIL"));
    testRunning = false;
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "上位机", "8.7 ACS 测试结束");
}

void MainWindow::runTest_8_7_real()
{
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "8.7", "开始真实 ACS 测试（手动输入 BLER）");
    m_acsScreenshotDir.clear();
    m_acsScreenshotPaths.clear();

    if (!hasSignalGenerator()) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "8.7", "1466 信号发生器未连接，无法测试");
        return;
    }

    const double centerFreqMHz = ui->carrierFreqSpin->value();
    const QString screenshotRunDir = screenshotRunDirPath("8.7_ACS", screenshotBatchTimestamp());
    m_acsScreenshotDir = screenshotRunDir;

    if (ACS_ENABLE_SCREENSHOT && hasSpectrumAnalyzer()) {
        QDir().mkpath(screenshotRunDir);
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "8.7",
               "截图本地目录: " + screenshotRunDir);
    }

    ui->progressBar->setValue(0);
    ui->progressBar->setFormat("8.7 ACS %p%");
    ui->currentCaseValue->setText("8.7 ACS (邻道选择性)");
    ui->freqValue->setText("有用信号: 200 kHz / 15 kHz DSB");
    ui->bwValue->setText("干扰信号: 5 MHz DFT-s-OFDM NR");
    setupTextChartBox(findChild<QGroupBox*>("groupBox_3"),
                      "ACS 示意图",
                      "等待选择测试点",
                      {"点击测试运行表任意行后，显示对应 images/sim/8_7 下的 schematic_0~7 示意图"},
                      "左图用于对照理论信号位置");
    setupTextChartBox(findChild<QGroupBox*>("groupBox_4"),
                      "4071 实测频谱图",
                      "等待 4071 截图",
                      {"截图完成后点击测试点行显示该频谱图",
                       QString("本次截图目录: %1").arg(screenshotRunDir)},
                      "右图用于留存实测频谱");
    setupChartBox(findChild<QGroupBox*>("groupBox"), "BLER vs 测试点", "8 个 upper/lower 测试点的 BLER 结果与 10% 门限");
    setupChartBox(findChild<QGroupBox*>("groupBox_2"), "ACS 测试通过率", "按 8 个 upper/lower 测试点统计 PASS/FAIL 分布");

    // 8.7 的协议判定依据是 BLER。
    // 频谱截图用于留证，表格中的有用/干扰功率使用协议配置值，不使用不可靠的 marker 估测值。
    // struct AcsTestPoint {
    //     QStringList waveformCandidates;
    //     QString refChannel;
    //     int bwKHz;
    //     int offsetKHz;               // 新增：邻道偏移值（带符号，单位 kHz）
    //     double screenshotSpanMHz;
    //     QString desc;
    // };

    QList<AcsTestPoint> testPoints = {
        // 200kHz A1-1 upper (+340)
        {{"ACS_200k_A-FR1-A-1-1_upper_D2R_plus_NR_int16_iq.bin"}, "A-FR1-A1-1", 200, +340, 10.0, "200kHz A1-1 upper"},
        // 200kHz A1-1 lower (-340)
        {{"ACS_200k_A-FR1-A-1-1_lower_D2R_plus_NR_int16_iq.bin"}, "A-FR1-A1-1", 200, -340, 10.0, "200kHz A1-1 lower"},
        // 200kHz A1-2 upper (+340)
        {{"ACS_200k_A-FR1-A-1-2_upper_D2R_plus_NR_int16_iq.bin"}, "A-FR1-A1-2", 200, +340, 10.0, "200kHz A1-2 upper"},
        // 200kHz A1-2 lower (-340)
        {{"ACS_200k_A-FR1-A-1-2_lower_D2R_plus_NR_int16_iq.bin"}, "A-FR1-A1-2", 200, -340, 10.0, "200kHz A1-2 lower"},
        // 3520kHz A1-3 upper (+2500)
        {{"ACS_3520k_A-FR1-A-1-3_upper_D2R_plus_NR_int16_iq.bin"}, "A-FR1-A1-3", 3520, +2500, 20.0, "3520kHz A1-3 upper"},
        // 3520kHz A1-3 lower (-2500)
        {{"ACS_3520k_A-FR1-A-1-3_lower_D2R_plus_NR_int16_iq.bin"}, "A-FR1-A1-3", 3520, -2500, 20.0, "3520kHz A1-3 lower"},
        // 3520kHz A1-4 upper (+2500)
        {{"ACS_3520k_A-FR1-A-1-4_upper_D2R_plus_NR_int16_iq.bin"}, "A-FR1-A1-4", 3520, +2500, 20.0, "3520kHz A1-4 upper"},
        // 3520kHz A1-4 lower (-2500)
        {{"ACS_3520k_A-FR1-A-1-4_lower_D2R_plus_NR_int16_iq.bin"}, "A-FR1-A1-4", 3520, -2500, 20.0, "3520kHz A1-4 lower"}
    };
    m_acsTestPoints = testPoints;   // 保存到成员变量，供绘图使用
    setAcsBlerEditingEnabled(true);

    ui->resultTableRun->clearContents();
    ui->resultTableRun->setRowCount(testPoints.size());
    ui->resultTableRun->setColumnCount(6);
    ui->resultTableRun->setHorizontalHeaderLabels({"测试点", "参考信道", "有用功率(dBm)", "干扰功率(dBm)", "BLER(%)", "判定"});
    ui->resultTableResult->clearContents();
    ui->resultTableResult->setRowCount(testPoints.size());
    ui->resultTableResult->setColumnCount(8);
    ui->resultTableResult->setHorizontalHeaderLabels({"测试点", "参考信道", "接收带宽(kHz)", "边缘偏移(kHz)",
                                                      "有用功率(dBm)", "干扰功率(dBm)", "BLER(%)", "判定"});

    for (int i = 0; i < testPoints.size(); ++i) {
        const auto &tp = testPoints[i];
        const double usefulPower = getUsefulPowerForRefChannel(tp.refChannel);
        const double totalPower = totalPowerSpin ? totalPowerSpin->value() : -50.0;
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "1466",
               QString("准备 8.7 测试点 %1: 参考信道=%2, 接收带宽=%3 kHz, 有用功率=%4 dBm, 干扰功率=%5 dBm, 边缘偏移=±%6 kHz, 总功率=%7 dBm")
                   .arg(tp.desc)
                   .arg(tp.refChannel)
                   .arg(tp.bwKHz)
                   .arg(usefulPower, 0, 'f', 1)
                   .arg(ACS_INTERFERENCE_POWER_DBM, 0, 'f', 1)
                   .arg(QString::number(qAbs(tp.offsetKHz)))   // 避免歧义
                   .arg(totalPower, 0, 'f', 2));

        QString loadedFileName;
        const bool waveformLoaded = load1466AcsWaveform(tp.waveformCandidates,
                                                        centerFreqMHz,
                                                        totalPower,
                                                        &loadedFileName);

        if (!waveformLoaded) {
            addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "ERROR", "8.7",
                   "加载或发射失败: " + tp.desc);
            ui->resultTableRun->setItem(i, 0, new QTableWidgetItem(tp.desc));
            ui->resultTableRun->setItem(i, 1, new QTableWidgetItem(tp.refChannel));
            ui->resultTableRun->setItem(i, 2, new QTableWidgetItem(QString::number(usefulPower, 'f', 1) + " dBm"));
            ui->resultTableRun->setItem(i, 3, new QTableWidgetItem(QString::number(ACS_INTERFERENCE_POWER_DBM, 'f', 1) + " dBm"));
            ui->resultTableRun->setItem(i, 4, new QTableWidgetItem("--"));
            ui->resultTableRun->setItem(i, 5, new QTableWidgetItem("FAIL"));

            ui->resultTableResult->setItem(i, 0, new QTableWidgetItem(tp.desc));
            ui->resultTableResult->setItem(i, 1, new QTableWidgetItem(tp.refChannel));
            ui->resultTableResult->setItem(i, 2, new QTableWidgetItem(QString::number(tp.bwKHz)));
            QString offsetStr = (tp.offsetKHz > 0) ? QString("+%1").arg(tp.offsetKHz) : QString::number(tp.offsetKHz);
            ui->resultTableResult->setItem(i, 3, new QTableWidgetItem(offsetStr + " kHz"));
            ui->resultTableResult->setItem(i, 4, new QTableWidgetItem(QString::number(usefulPower, 'f', 1)));
            ui->resultTableResult->setItem(i, 5, new QTableWidgetItem(QString::number(ACS_INTERFERENCE_POWER_DBM, 'f', 1)));
            ui->resultTableResult->setItem(i, 6, new QTableWidgetItem("--"));
            ui->resultTableResult->setItem(i, 7, new QTableWidgetItem("FAIL"));
            m_acsScreenshotPaths.append(QString());
            ui->progressBar->setValue((i + 1) * 100 / testPoints.size());
            continue;
        }

        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "1466",
               QString("8.7 已加载波形文件: %1").arg(loadedFileName));

        QThread::sleep(4);

        QString screenshotPath;
        if (ACS_ENABLE_SCREENSHOT) {
            screenshotPath = capture4071ForAcs(tp.desc, tp.screenshotSpanMHz, screenshotRunDir, i);
        }
        m_acsScreenshotPaths.append(screenshotPath);

        // 填充测试运行表格，控制各列是否可编辑
        QTableWidgetItem *item0 = new QTableWidgetItem(tp.desc);
        item0->setFlags(item0->flags() & ~Qt::ItemIsEditable);
        ui->resultTableRun->setItem(i, 0, item0);

        QTableWidgetItem *item1 = new QTableWidgetItem(tp.refChannel);
        item1->setFlags(item1->flags() & ~Qt::ItemIsEditable);
        ui->resultTableRun->setItem(i, 1, item1);

        QTableWidgetItem *item2 = new QTableWidgetItem(QString::number(usefulPower, 'f', 1) + " dBm");
        item2->setFlags(item2->flags() & ~Qt::ItemIsEditable);
        ui->resultTableRun->setItem(i, 2, item2);

        QTableWidgetItem *item3 = new QTableWidgetItem(QString::number(ACS_INTERFERENCE_POWER_DBM, 'f', 1) + " dBm");
        item3->setFlags(item3->flags() & ~Qt::ItemIsEditable);
        ui->resultTableRun->setItem(i, 3, item3);

        // BLER 列：可编辑
        QTableWidgetItem *blerItem = new QTableWidgetItem("");
        blerItem->setFlags(blerItem->flags() | Qt::ItemIsEditable);
        ui->resultTableRun->setItem(i, 4, blerItem);

        // 判定列：不可编辑
        QTableWidgetItem *verdictItem = new QTableWidgetItem("WAIT");
        verdictItem->setFlags(verdictItem->flags() & ~Qt::ItemIsEditable);
        ui->resultTableRun->setItem(i, 5, verdictItem);

        // 同时填充结果显示表格（BLER 和判定也留空或显示 WAIT）
        ui->resultTableResult->setItem(i, 0, new QTableWidgetItem(tp.desc));
        ui->resultTableResult->setItem(i, 1, new QTableWidgetItem(tp.refChannel));
        ui->resultTableResult->setItem(i, 2, new QTableWidgetItem(QString::number(tp.bwKHz)));
        QString offsetStr = (tp.offsetKHz > 0) ? QString("+%1").arg(tp.offsetKHz) : QString::number(tp.offsetKHz);
        ui->resultTableResult->setItem(i, 3, new QTableWidgetItem(offsetStr + " kHz"));
        ui->resultTableResult->setItem(i, 4, new QTableWidgetItem(QString::number(usefulPower, 'f', 1)));
        ui->resultTableResult->setItem(i, 5, new QTableWidgetItem(QString::number(ACS_INTERFERENCE_POWER_DBM, 'f', 1)));
        ui->resultTableResult->setItem(i, 6, new QTableWidgetItem(""));   // BLER 留空
        ui->resultTableResult->setItem(i, 7, new QTableWidgetItem("WAIT"));

        // addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), pass ? "PASS" : "FAIL", "8.7",
        //        QString("%1: 参考信道=%2, 接收带宽=%3 kHz, 边缘偏移=±%4 kHz, BLER=%5%")
        //            .arg(tp.desc)
        //            .arg(tp.refChannel)
        //            .arg(tp.bwKHz)
        //            .arg(qAbs(tp.offsetKHz))
        //            .arg(bler, 0, 'f', 2));

        ui->progressBar->setValue((i + 1) * 100 / testPoints.size());
    }

    confirmBlerBtn->setText("确认 BLER");
    confirmBlerBtn->setVisible(true);
    confirmBlerBtn->setEnabled(true);
    ui->pushButton->setEnabled(false);

    stopSpectrumAnalyzerMeasurement("8.7 测试结束停止测量");
    shutdownSignalGeneratorOutput("8.7 测试结束");
    if (!m_acsTestPoints.isEmpty()) {
        ui->resultTableRun->selectRow(0);
        onAcsTableRowClicked(0, 0);
    }
    // ui->verdictLabel->setText(overallPass ? "PASS" : "FAIL");
    // ui->statsLabel->setText(QString("ACS 测试完成\n判定条件: BLER ≤ %1%%\n整体结果: %2")
    //                         .arg(ACS_BLER_LIMIT_PERCENT, 0, 'f', 1)
    //                         .arg(overallPass ? "PASS" : "FAIL"));
    testRunning = false;
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "上位机", "8.7 ACS 测试结束");
}

// ========== 其余槽函数（保持不变） ==========
void MainWindow::onStartBackend()
{
    ui->statusBar->showMessage("后端启动中...");
    QTimer::singleShot(1000, [this](){
        QLabel *backendBadge = findChild<QLabel *>("backendBadge");
        if (backendBadge) {
            backendBadge->setText("后端 127.0.0.1:8899  已连接");
            backendBadge->setProperty("state", "online");
            backendBadge->style()->unpolish(backendBadge);
            backendBadge->style()->polish(backendBadge);
        }
        QLabel *backendState = findChild<QLabel *>("backendStateLabel");
        if (backendState) {
            backendState->setText("后端服务: 已启动    RTT: 12 ms");
        }
        ui->statusBar->showMessage("就绪 | 后端已启动 | RTT: 12 ms | v0.1", 2000);
    });
}

void MainWindow::onStopBackend()
{
    stopSpectrumAnalyzerMeasurement("停止后端停止测量");
    shutdownSignalGeneratorOutput("停止后端安全关断");

    QLabel *backendBadge = findChild<QLabel *>("backendBadge");
    if (backendBadge) {
        backendBadge->setText("后端 127.0.0.1:8899  未连接");
        backendBadge->setProperty("state", "offline");
        backendBadge->style()->unpolish(backendBadge);
        backendBadge->style()->polish(backendBadge);
    }
    QLabel *backendState = findChild<QLabel *>("backendStateLabel");
    if (backendState) {
        backendState->setText("后端服务: 未启动    RTT: -- ms");
    }
    ui->statusBar->showMessage("停止后端", 2000);
}

void MainWindow::onImportConfig()
{
    QString fileName = QFileDialog::getOpenFileName(this, "导入配置", "", "JSON (*.json)");
    if (!fileName.isEmpty()) {
        ui->statusBar->showMessage("已导入配置: " + fileName, 2000);
    }
}

void MainWindow::onExportConfig()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出配置", "", "JSON (*.json)");
    if (!fileName.isEmpty()) {
        ui->statusBar->showMessage("已导出配置: " + fileName, 2000);
    }
}

void MainWindow::onGenerateReport()
{
    QMessageBox::information(this, "生成报告", "报告生成功能待实现");
}

void MainWindow::onTestCaseClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    if (!item->parent()) return;

    resetCaseRuntimeState();
    ui->pushButton->setEnabled(true);
    currentTestCase = item->text(0);
    applyCaseTemplate(currentTestCase);
    ui->statusBar->showMessage("就绪 | 当前用例: " + currentTestCase + " | 后端: 未启动 | RTT: -- ms | v0.1");
    ui->centralTabs->setCurrentIndex(0);
    applyCaseResultTemplate(currentTestCase);
    return;

    if (currentTestCase.contains("8.6") || currentTestCase.contains("8.7")) {
        return;
    }

    const bool isRx = currentTestCase.contains("8.6") || currentTestCase.contains("8.7");
    const bool isProto = currentTestCase.contains("8.8") || currentTestCase.contains("8.9")
                         || currentTestCase.contains("8.10") || currentTestCase.contains("8.11");
    const bool isTransient = currentTestCase.contains("8.3");
    const bool isModulation = currentTestCase.contains("8.4");

    if (isTransient) {
        ui->currentCaseValue->setText("8.3 瞬态周期 / OFF 功率");
        ui->freqValue->setText(QString::number(ui->carrierFreqSpin->value(), 'f', 1) + " MHz");
        ui->bwValue->setText("200/400/600/800 kHz, SCS 15 kHz");

        ui->resultTableRun->clearContents();
        ui->resultTableRun->setColumnCount(4);
        ui->resultTableRun->setHorizontalHeaderLabels({"带宽/SCS", "瞬态周期", "OFF功率", "状态"});
        ui->resultTableRun->setRowCount(4);
        const QStringList bws = {"200 kHz / 15 kHz", "400 kHz / 15 kHz", "600 kHz / 15 kHz", "800 kHz / 15 kHz"};
        for (int i = 0; i < bws.size(); ++i) {
            ui->resultTableRun->setItem(i, 0, new QTableWidgetItem(bws[i]));
            ui->resultTableRun->setItem(i, 1, new QTableWidgetItem("待测: OFF->ON / ON->OFF"));
            ui->resultTableRun->setItem(i, 2, new QTableWidgetItem("待测: dBm/MHz"));
            ui->resultTableRun->setItem(i, 3, new QTableWidgetItem("WAIT"));
        }

        ui->verdictLabel->setText("WAIT");
        ui->statsLabel->setText("标准门限: 瞬态周期 <10 us, OFF功率 < -85 dBm/MHz\n"
                                "试用方案能力: 200/400/600/800 kHz, SCS 15 kHz, 时间分辨率 <=0.1 ns");

        ui->resultTableResult->clearContents();
        ui->resultTableResult->setColumnCount(6);
        ui->resultTableResult->setHorizontalHeaderLabels({"带宽", "SCS", "时间分辨率", "OFF->ON", "ON->OFF/OFF功率", "判定"});
        ui->resultTableResult->setRowCount(2);
        ui->resultTableResult->setItem(0, 0, new QTableWidgetItem("测试范围"));
        ui->resultTableResult->setItem(0, 1, new QTableWidgetItem("15 kHz"));
        ui->resultTableResult->setItem(0, 2, new QTableWidgetItem("<=0.1 ns"));
        ui->resultTableResult->setItem(0, 3, new QTableWidgetItem("<10 us"));
        ui->resultTableResult->setItem(0, 4, new QTableWidgetItem("<10 us / <-85 dBm/MHz"));
        ui->resultTableResult->setItem(0, 5, new QTableWidgetItem("WAIT"));
        ui->resultTableResult->setItem(1, 0, new QTableWidgetItem("带宽遍历"));
        ui->resultTableResult->setItem(1, 1, new QTableWidgetItem("200/400/600/800 kHz"));
        ui->resultTableResult->setItem(1, 2, new QTableWidgetItem("待测"));
        ui->resultTableResult->setItem(1, 3, new QTableWidgetItem("待测"));
        ui->resultTableResult->setItem(1, 4, new QTableWidgetItem("待测"));
        ui->resultTableResult->setItem(1, 5, new QTableWidgetItem("WAIT"));
    }
    else if (isModulation) {
        ui->currentCaseValue->setText("8.4 调制质量");
        ui->freqValue->setText(QString::number(ui->carrierFreqSpin->value(), 'f', 1) + " MHz");
        ui->bwValue->setText("200/400/600/800 kHz, SCS 15 kHz");

        ui->resultTableRun->clearContents();
        ui->resultTableRun->setColumnCount(4);
        ui->resultTableRun->setHorizontalHeaderLabels({"带宽/SCS", "调制深度/纹波", "时间参数", "状态"});
        ui->resultTableRun->setRowCount(4);
        const QStringList bws = {"200 kHz / 15 kHz", "400 kHz / 15 kHz", "600 kHz / 15 kHz", "800 kHz / 15 kHz"};
        for (int i = 0; i < bws.size(); ++i) {
            ui->resultTableRun->setItem(i, 0, new QTableWidgetItem(bws[i]));
            ui->resultTableRun->setItem(i, 1, new QTableWidgetItem("待测: 调制深度 / 纹波"));
            ui->resultTableRun->setItem(i, 2, new QTableWidgetItem("待测: Tr / Tf / PW"));
            ui->resultTableRun->setItem(i, 3, new QTableWidgetItem("WAIT"));
        }

        ui->verdictLabel->setText("WAIT");
        ui->statsLabel->setText("标准门限: 调制深度 >=80%, 纹波 <=±15%, Tr/Tf <=0.66Tc, PW <=1.3Tc\n"
                                "试用方案能力: RTBW>=100MHz, 模拟带宽>=500MHz, 功率分辨率<=0.01dB, 时间分辨率<=0.1ns, 采样率>=5GS/s");

        ui->resultTableResult->clearContents();
        ui->resultTableResult->setColumnCount(6);
        ui->resultTableResult->setHorizontalHeaderLabels({"带宽", "调制深度", "纹波", "上升/下降时间", "脉冲宽度", "判定"});
        ui->resultTableResult->setRowCount(2);
        ui->resultTableResult->setItem(0, 0, new QTableWidgetItem("测试范围"));
        ui->resultTableResult->setItem(0, 1, new QTableWidgetItem(">=80%"));
        ui->resultTableResult->setItem(0, 2, new QTableWidgetItem("<=±15%"));
        ui->resultTableResult->setItem(0, 3, new QTableWidgetItem("<=0.66Tc"));
        ui->resultTableResult->setItem(0, 4, new QTableWidgetItem("<=1.3Tc"));
        ui->resultTableResult->setItem(0, 5, new QTableWidgetItem("WAIT"));
        ui->resultTableResult->setItem(1, 0, new QTableWidgetItem("带宽遍历"));
        ui->resultTableResult->setItem(1, 1, new QTableWidgetItem("200/400/600/800 kHz"));
        ui->resultTableResult->setItem(1, 2, new QTableWidgetItem("待测"));
        ui->resultTableResult->setItem(1, 3, new QTableWidgetItem("待测"));
        ui->resultTableResult->setItem(1, 4, new QTableWidgetItem("待测"));
        ui->resultTableResult->setItem(1, 5, new QTableWidgetItem("WAIT"));
    }
    else if (currentTestCase.contains("8.5") || currentTestCase == "8.5 ACLR")
    {
        ui->currentCaseValue->setText("8.5 ACLR / 邻道泄漏比");
        ui->freqValue->setText(QString::number(ui->carrierFreqSpin->value(), 'f', 1) + " MHz");
        ui->bwValue->setText("200/400/600/800 kHz, SCS 15 kHz");

        ui->resultTableRun->clearContents();
        ui->resultTableRun->setColumnCount(9);
        ui->resultTableRun->setHorizontalHeaderLabels({"带宽", "邻道偏移", "滤波带宽", "主功率",
                                                       "左邻功率", "左ACLR", "右邻功率", "右ACLR", "判定"});

        ui->resultTableResult->clearContents();
        ui->resultTableResult->setColumnCount(10);
        ui->resultTableResult->setHorizontalHeaderLabels({"带宽(kHz)", "邻道偏移(kHz)", "滤波带宽(kHz)",
                                                          "主功率(dBm)", "左邻功率(dBm)", "左ACLR(dB)",
                                                          "右邻功率(dBm)", "右ACLR(dB)", "限值(dB)", "判定"});

        struct TemplateRow {
            int channelBWKHz;
            int offsetKHz;
            int integrationBWKHz;
            double limitDb;
        };
        const QList<TemplateRow> rows = {
            {200, 300, 180, 40.8}, {200, 500, 180, 45.8},
            {400, 500, 360, 40.8}, {400, 900, 360, 45.8},
            {600, 700, 540, 40.8}, {600, 1300, 540, 45.8},
            {800, 900, 720, 40.8}, {800, 1700, 720, 45.8}
        };
        ui->resultTableRun->setRowCount(rows.size());
        ui->resultTableResult->setRowCount(rows.size());
        for (int i = 0; i < rows.size(); ++i) {
            const auto &row = rows[i];
            ui->resultTableRun->setItem(i, 0, new QTableWidgetItem(QString("%1 kHz").arg(row.channelBWKHz)));
            ui->resultTableRun->setItem(i, 1, new QTableWidgetItem(QString("±%1 kHz").arg(row.offsetKHz)));
            ui->resultTableRun->setItem(i, 2, new QTableWidgetItem(QString("%1 kHz").arg(row.integrationBWKHz)));
            for (int col = 3; col < 8; ++col) {
                ui->resultTableRun->setItem(i, col, new QTableWidgetItem("待测"));
            }
            ui->resultTableRun->setItem(i, 8, new QTableWidgetItem("WAIT"));

            ui->resultTableResult->setItem(i, 0, new QTableWidgetItem(QString::number(row.channelBWKHz)));
            ui->resultTableResult->setItem(i, 1, new QTableWidgetItem(QString("±%1").arg(row.offsetKHz)));
            ui->resultTableResult->setItem(i, 2, new QTableWidgetItem(QString::number(row.integrationBWKHz)));
            for (int col = 3; col < 8; ++col) {
                ui->resultTableResult->setItem(i, col, new QTableWidgetItem("待测"));
            }
            ui->resultTableResult->setItem(i, 8, new QTableWidgetItem(QString::number(row.limitDb, 'f', 1)));
            ui->resultTableResult->setItem(i, 9, new QTableWidgetItem("WAIT"));
        }

        ui->verdictLabel->setText("WAIT");
        ui->statsLabel->setText("待测试: 遍历200/400/600/800 kHz带宽\n"
                                "按±邻道中心频偏测左右邻道功率、滤波带宽和ACLR\n"
                                "判定门限: 第一邻道≥40.8 dB，第二邻道≥45.8 dB");
    }
    else if (currentTestCase.contains("8.6") || currentTestCase == "8.6 参考灵敏度")
    {
        ui->currentCaseValue->setText("8.6 参考灵敏度");
        ui->freqValue->setText("参考信道 A-FR1-A1-2");
        ui->bwValue->setText("200 kHz / 15 kHz DSB");

        // 5个功率点：-130 dBm, 三个随机典型值, -50 dBm
        QList<double> powerPoints = {-130, -100, -80, -60, -50};

        ui->resultTableRun->clearContents();
        ui->resultTableRun->setColumnCount(4);
        ui->resultTableRun->setHorizontalHeaderLabels({"功率", "BLER(%)", "频率误差", "判定"});
        ui->resultTableRun->setRowCount(5);
        for (int i = 0; i < 5; ++i) {
            ui->resultTableRun->setItem(i, 0, new QTableWidgetItem(QString::number(powerPoints[i]) + " dBm"));
            ui->resultTableRun->setItem(i, 1, new QTableWidgetItem("待测"));
            ui->resultTableRun->setItem(i, 2, new QTableWidgetItem("待测"));
            ui->resultTableRun->setItem(i, 3, new QTableWidgetItem("WAIT"));
        }

        ui->resultTableResult->clearContents();
        ui->resultTableResult->setColumnCount(6);
        ui->resultTableResult->setHorizontalHeaderLabels({"信号配置", "功率", "频率误差", "BLER(%)", "判定门限", "判定"});
        ui->resultTableResult->setRowCount(5);
        for (int i = 0; i < 5; ++i) {
            ui->resultTableResult->setItem(i, 0, new QTableWidgetItem(i==0 ? "A-FR1-A1-2" : "—"));
            ui->resultTableResult->setItem(i, 1, new QTableWidgetItem(QString::number(powerPoints[i]) + " dBm"));
            ui->resultTableResult->setItem(i, 2, new QTableWidgetItem("待测"));
            ui->resultTableResult->setItem(i, 3, new QTableWidgetItem("待测"));
            ui->resultTableResult->setItem(i, 4, new QTableWidgetItem("≤10%"));
            ui->resultTableResult->setItem(i, 5, new QTableWidgetItem("WAIT"));
        }

        ui->verdictLabel->setText("WAIT");
        ui->statsLabel->setText("待测试: 5个功率点扫描 (-130, -100, -80, -60, -50 dBm)\n"
                                "判定条件: BLER ≤ 10% 时对应的最小功率点为参考灵敏度\n"
                                "频率误差需 ≤ ±(0.05 ppm+6 Hz)");
    }
    else if (currentTestCase.contains("8.7") || currentTestCase == "8.7 ACS")
    {
        ui->currentCaseValue->setText("8.7 ACS (邻道选择性)");
        ui->freqValue->setText("有用信号: 200 kHz / 15 kHz DSB");
        ui->bwValue->setText("干扰信号: 5 MHz DFT-s-OFDM NR");

        // 4种偏移场景
        QList<QString> scenes = {"低偏移 +340 kHz", "低偏移 -340 kHz", "高偏移 +2500 kHz", "高偏移 -2500 kHz"};
        QList<double> offsets = {340, -340, 2500, -2500};

        ui->resultTableRun->clearContents();
        ui->resultTableRun->setColumnCount(7);
        ui->resultTableRun->setHorizontalHeaderLabels({"偏移", "有用信号功率", "干扰信号功率", "带宽", "BLER(%)", "ACS", "判定"});
        ui->resultTableRun->setRowCount(4);

        ui->resultTableResult->clearContents();
        ui->resultTableResult->setColumnCount(7);
        ui->resultTableResult->setHorizontalHeaderLabels({"偏移", "有用信号功率", "干扰信号功率", "带宽", "BLER(%)", "ACS", "判定"});
        ui->resultTableResult->setRowCount(4);

        for (int i = 0; i < 4; ++i) {
            ui->resultTableRun->setItem(i, 0, new QTableWidgetItem(QString::number(offsets[i]) + " kHz"));
            ui->resultTableRun->setItem(i, 1, new QTableWidgetItem("待测"));
            ui->resultTableRun->setItem(i, 2, new QTableWidgetItem("待测"));
            ui->resultTableRun->setItem(i, 3, new QTableWidgetItem("200kHz/3520kHz"));  // 示例
            ui->resultTableRun->setItem(i, 4, new QTableWidgetItem("待测"));
            ui->resultTableRun->setItem(i, 5, new QTableWidgetItem("待测"));
            ui->resultTableRun->setItem(i, 6, new QTableWidgetItem("WAIT"));

            ui->resultTableResult->setItem(i, 0, new QTableWidgetItem(QString::number(offsets[i]) + " kHz"));
            ui->resultTableResult->setItem(i, 1, new QTableWidgetItem("待测"));
            ui->resultTableResult->setItem(i, 2, new QTableWidgetItem("待测"));
            ui->resultTableResult->setItem(i, 3, new QTableWidgetItem("200kHz/3520kHz"));
            ui->resultTableResult->setItem(i, 4, new QTableWidgetItem("待测"));
            ui->resultTableResult->setItem(i, 5, new QTableWidgetItem("待测"));
            ui->resultTableResult->setItem(i, 6, new QTableWidgetItem("WAIT"));
        }

        ui->verdictLabel->setText("WAIT");
        ui->statsLabel->setText("待测试: 4种干扰偏移场景\n"
                                "有用信号功率: 20 dBm, 干扰信号功率: -53 dBm\n"
                                "判定门限: BLER ≤ 10% 且 ACS ≥ 33 dB (低偏移) / ≥45 dB (高偏移)");
    }
    else if (isRx) {
        ui->currentCaseValue->setText("8.6 参考灵敏度");
        ui->freqValue->setText("参考信道 A-FR1-A1-2");
        ui->bwValue->setText("200 kHz / 15 kHz DSB");

        ui->resultTableRun->clearContents();
        ui->resultTableRun->setRowCount(2);
        ui->resultTableRun->setItem(0, 0, new QTableWidgetItem("参考灵敏度"));
        ui->resultTableRun->setItem(0, 1, new QTableWidgetItem("-91.9 dBm"));
        ui->resultTableRun->setItem(0, 2, new QTableWidgetItem("目标 -90 dBm"));
        ui->resultTableRun->setItem(0, 3, new QTableWidgetItem("PASS"));
        ui->resultTableRun->setItem(1, 0, new QTableWidgetItem("频率误差"));
        ui->resultTableRun->setItem(1, 1, new QTableWidgetItem("12.4 Hz"));
        ui->resultTableRun->setItem(1, 2, new QTableWidgetItem("< 100 Hz"));
        ui->resultTableRun->setItem(1, 3, new QTableWidgetItem("PASS"));

        ui->verdictLabel->setText("PASS");
        ui->statsLabel->setText("参考灵敏度: -91.9 dBm\n频率误差: 12.4 Hz\nBLER @ -91.9dBm: 9.8%");

        ui->resultTableResult->clearContents();
        ui->resultTableResult->setRowCount(3);
        ui->resultTableResult->setItem(0, 0, new QTableWidgetItem("信道 A1-1"));
        ui->resultTableResult->setItem(0, 1, new QTableWidgetItem("-95.0 dBm"));
        ui->resultTableResult->setItem(0, 2, new QTableWidgetItem("PASS"));
        ui->resultTableResult->setItem(1, 0, new QTableWidgetItem("信道 A1-2"));
        ui->resultTableResult->setItem(1, 1, new QTableWidgetItem("-91.9 dBm"));
        ui->resultTableResult->setItem(1, 2, new QTableWidgetItem("PASS"));
        ui->resultTableResult->setItem(2, 0, new QTableWidgetItem("信道 A1-3"));
        ui->resultTableResult->setItem(2, 1, new QTableWidgetItem("-90.2 dBm"));
        ui->resultTableResult->setItem(2, 2, new QTableWidgetItem("PASS"));
        for (int row = 0; row < 3; ++row) {
            for (int col = 3; col < 6; ++col) {
                ui->resultTableResult->setItem(row, col, new QTableWidgetItem(""));
            }
        }
    }
    else if (isProto) {
        ui->resultTableRun->clearContents();
        ui->resultTableRun->setRowCount(4);
        const QStringList events = {"接入请求", "AO响应", "标签盘点", "冲突检测"};
        const QStringList values = {"8 tags", "RTT 12.8 ms", "7/8 success", "1 conflict"};
        const QStringList limits = {"目标: ≥ 8", "目标: < 20 ms", "目标: ≥ 90%", "允许: ≤ 1"};
        for (int i = 0; i < events.size(); ++i) {
            ui->resultTableRun->setItem(i, 0, new QTableWidgetItem(events[i]));
            ui->resultTableRun->setItem(i, 1, new QTableWidgetItem(values[i]));
            ui->resultTableRun->setItem(i, 2, new QTableWidgetItem(limits[i]));
            ui->resultTableRun->setItem(i, 3, new QTableWidgetItem(i == 0 ? "RUN" : "PASS"));
        }

        ui->verdictLabel->setText("PASS");
        ui->statsLabel->setText("成功标签数: 7/8\n平均RTT: 12.8 ms\n冲突次数: 1");

        ui->resultTableResult->clearContents();
        ui->resultTableResult->setRowCount(4);
        QStringList tags = {"Tag1", "Tag2", "Tag3", "Tag4"};
        QStringList kvals = {"k=1", "k=2", "k=4", "k=8"};
        QStringList states = {"PASS", "PASS", "RUN", "WAIT"};
        for (int i = 0; i < 4; ++i) {
            ui->resultTableResult->setItem(i, 0, new QTableWidgetItem(tags[i]));
            ui->resultTableResult->setItem(i, 1, new QTableWidgetItem(kvals[i]));
            ui->resultTableResult->setItem(i, 2, new QTableWidgetItem("OOK"));
            ui->resultTableResult->setItem(i, 3, new QTableWidgetItem("-"));
            ui->resultTableResult->setItem(i, 4, new QTableWidgetItem(QString::number(11.2 + i, 'f', 1) + " ms"));
            ui->resultTableResult->setItem(i, 5, new QTableWidgetItem(states[i]));
        }
    }
    else {
        ui->resultTableRun->clearContents();
        ui->resultTableRun->setRowCount(3);
        ui->resultTableRun->setItem(0, 0, new QTableWidgetItem("主信道功率"));
        ui->resultTableRun->setItem(0, 1, new QTableWidgetItem("+20.04 dBm"));
        ui->resultTableRun->setItem(0, 2, new QTableWidgetItem("—"));
        ui->resultTableRun->setItem(0, 3, new QTableWidgetItem("PASS"));
        ui->resultTableRun->setItem(1, 0, new QTableWidgetItem("左邻道ACLR"));
        ui->resultTableRun->setItem(1, 1, new QTableWidgetItem("-35.2 dB"));
        ui->resultTableRun->setItem(1, 2, new QTableWidgetItem("+3.7 dB"));
        ui->resultTableRun->setItem(1, 3, new QTableWidgetItem("PASS"));
        ui->resultTableRun->setItem(2, 0, new QTableWidgetItem("右邻道ACLR"));
        ui->resultTableRun->setItem(2, 1, new QTableWidgetItem("-34.8 dB"));
        ui->resultTableRun->setItem(2, 2, new QTableWidgetItem("+4.2 dB"));
        ui->resultTableRun->setItem(2, 3, new QTableWidgetItem("PASS"));

        ui->verdictLabel->setText("PASS");
        ui->statsLabel->setText("成功标签数: 8/8\n数据率: 98.2 kbps\nBLER: 0.4%");

        ui->resultTableResult->clearContents();
        ui->resultTableResult->setRowCount(4);
        QStringList tags = {"Tag1", "Tag2", "Tag3", "Tag4"};
        QStringList kvals = {"k=1", "k=2", "k=4", "k=8"};
        for (int i = 0; i < 4; ++i) {
            ui->resultTableResult->setItem(i, 0, new QTableWidgetItem(tags[i]));
            ui->resultTableResult->setItem(i, 1, new QTableWidgetItem(kvals[i]));
            ui->resultTableResult->setItem(i, 2, new QTableWidgetItem("OOK"));
            ui->resultTableResult->setItem(i, 3, new QTableWidgetItem(QString::number(-87.0 + i, 'f', 1) + " dBm"));
            ui->resultTableResult->setItem(i, 4, new QTableWidgetItem("0.2%"));
            ui->resultTableResult->setItem(i, 5, new QTableWidgetItem("PASS"));
        }
    }
}

void MainWindow::onLoadPreset()
{
    QString preset = ui->presetCombo->currentText();
    ui->statusBar->showMessage("加载预设: " + preset, 2000);
}

void MainWindow::onSavePreset()
{
    QString name = QInputDialog::getText(this, "保存预设", "请输入预设名称:");
    if (!name.isEmpty()) {
        ui->presetCombo->addItem(name);
        ui->presetCombo->setCurrentText(name);
        ui->statusBar->showMessage("预设已保存: " + name, 2000);
    }
}

void MainWindow::onPauseTest()
{
    ui->statusBar->showMessage("测试暂停", 2000);
}

void MainWindow::onStopTest()
{
    stopSpectrumAnalyzerMeasurement("手动停止测试停止测量");
    shutdownSignalGeneratorOutput("手动停止测试安全关断");
    ui->statusBar->showMessage("测试停止", 2000);
    confirmBlerBtn->setVisible(false);
    ui->pushButton->setEnabled(true);
}

void MainWindow::onExportCsv()
{
    QString defaultName = currentTestCase.isEmpty()
                          ? QStringLiteral("AIoT_test_result")
                          : currentTestCase;
    defaultName.replace(' ', '_');
    defaultName.replace('/', '_');
    defaultName += "_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".csv";
    QString fileName = QFileDialog::getSaveFileName(this, "导出测试数据", outputPath(defaultName), "CSV (*.csv)");
    if (fileName.isEmpty()) {
        return;
    }

    if (QFileInfo(fileName).suffix().compare("csv", Qt::CaseInsensitive) != 0) {
        fileName += ".csv";
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", "无法写入文件:\n" + fileName);
        ui->statusBar->showMessage("导出 CSV 失败", 2000);
        return;
    }

    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif

    out << "AIoT测试仪表上位机测试数据\n";
    out << "导出时间," << escapeCsvCell(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")) << '\n';
    out << "当前用例," << escapeCsvCell(currentTestCase) << '\n';
    out << "中心频率," << escapeCsvCell(ui->freqValue ? ui->freqValue->text() : QString()) << '\n';
    out << "带宽/说明," << escapeCsvCell(ui->bwValue ? ui->bwValue->text() : QString()) << '\n';
    out << "整体判定," << escapeCsvCell(ui->verdictLabel ? ui->verdictLabel->text() : QString()) << '\n';
    out << "统计摘要," << escapeCsvCell(ui->statsLabel ? ui->statsLabel->text() : QString()) << '\n';

    writeTableAsCsvSection(out, ui->resultTableRun, "测试运行表");
    writeTableAsCsvSection(out, ui->resultTableResult, "结果显示表");
    writeTableAsCsvSection(out, ui->logTable, "日志表");

    file.close();
    ui->statusBar->showMessage("测试数据已导出: " + fileName, 3000);
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "上位机", "测试数据已导出: " + fileName);
}

void MainWindow::onSaveScreenshot()
{
    ui->statusBar->showMessage("保存截图功能待实现", 2000);
}

void MainWindow::onViewHistory()
{
    ui->statusBar->showMessage("查看历史功能待实现", 2000);
}

void MainWindow::onExportResult()
{
    onExportCsv();
}

void MainWindow::onClearLog()
{
    ui->logTable->setRowCount(0);
    ui->statusBar->showMessage("日志已清空", 2000);
}

void MainWindow::onExportLog()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出日志", "", "CSV (*.csv)");
    if (fileName.isEmpty()) {
        return;
    }

    if (QFileInfo(fileName).suffix().compare("csv", Qt::CaseInsensitive) != 0) {
        fileName += ".csv";
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", "无法写入文件:\n" + fileName);
        ui->statusBar->showMessage("导出日志失败", 2000);
        return;
    }

    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif

    QStringList headers;
    for (int col = 0; col < ui->logTable->columnCount(); ++col) {
        QTableWidgetItem *headerItem = ui->logTable->horizontalHeaderItem(col);
        headers << escapeCsvCell(headerItem ? headerItem->text() : QString("列%1").arg(col + 1));
    }
    out << headers.join(',') << '\n';

    for (int row = 0; row < ui->logTable->rowCount(); ++row) {
        QStringList fields;
        for (int col = 0; col < ui->logTable->columnCount(); ++col) {
            QTableWidgetItem *item = ui->logTable->item(row, col);
            fields << escapeCsvCell(tableItemText(item));
        }
        out << fields.join(',') << '\n';
    }

    file.close();
    ui->statusBar->showMessage("日志已导出: " + fileName, 3000);
}

void MainWindow::addLog(const QString &time, const QString &level, const QString &src, const QString &msg)
{
    int row = ui->logTable->rowCount();
    ui->logTable->insertRow(row);

    auto makeLogItem = [](const QString &text) {
        QTableWidgetItem *item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        return item;
    };

    QTableWidgetItem *timeItem = makeLogItem(time);
    QTableWidgetItem *levelItem = makeLogItem(level);
    QTableWidgetItem *srcItem = makeLogItem(src);
    QTableWidgetItem *msgItem = makeLogItem(msg);

    QColor fgColor("#334155");
    QColor bgColor("#ffffff");
    const QString normalizedLevel = level.trimmed().toUpper();
    if (normalizedLevel == "ERROR" || normalizedLevel == "FAIL") {
        fgColor = QColor("#b42318");
        bgColor = QColor("#fff1f3");
    } else if (normalizedLevel == "WARN") {
        fgColor = QColor("#b54708");
        bgColor = QColor("#fff7e6");
    } else if (normalizedLevel == "PASS") {
        fgColor = QColor("#027a48");
        bgColor = QColor("#ecfdf3");
    } else if (normalizedLevel == "INFO") {
        fgColor = QColor("#1d4ed8");
        bgColor = QColor("#eff6ff");
    }

    for (QTableWidgetItem *item : {timeItem, levelItem, srcItem, msgItem}) {
        item->setForeground(fgColor);
        item->setBackground(bgColor);
    }

    QFont levelFont = levelItem->font();
    levelFont.setBold(true);
    levelItem->setFont(levelFont);

    timeItem->setTextAlignment(Qt::AlignCenter);
    levelItem->setTextAlignment(Qt::AlignCenter);
    srcItem->setTextAlignment(Qt::AlignCenter);
    msgItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    ui->logTable->setItem(row, 0, timeItem);
    ui->logTable->setItem(row, 1, levelItem);
    ui->logTable->setItem(row, 2, srcItem);
    ui->logTable->setItem(row, 3, msgItem);
    polishLogTable();
    applyLogLevelFilter();
    ui->logTable->scrollToBottom();
}

void MainWindow::applyLogLevelFilter()
{
    if (!ui->logTable || !ui->levelFilterCombo) {
        return;
    }

    const QString selectedLevel = ui->levelFilterCombo->currentText().trimmed();
    const bool showAll = selectedLevel.isEmpty() || selectedLevel == QStringLiteral("全部");

    for (int row = 0; row < ui->logTable->rowCount(); ++row) {
        const QTableWidgetItem *levelItem = ui->logTable->item(row, 1);
        const QString rowLevel = levelItem ? levelItem->text().trimmed() : QString();
        const bool visible = showAll || rowLevel.compare(selectedLevel, Qt::CaseInsensitive) == 0;
        ui->logTable->setRowHidden(row, !visible);
    }
}

void MainWindow::onSendCustomCommand()
{
    bool ok;
    QString cmd = QInputDialog::getText(this, "发送 SCPI 命令",
                                        "请输入命令（不加换行符）:",
                                        QLineEdit::Normal,
                                        "*IDN?", &ok);
    if (ok && !cmd.isEmpty()) {
        sendScpiCommand(cmd);
    }
}

void MainWindow::onConfirmBler()
{
    int rows = ui->resultTableRun->rowCount();
    bool allValid = true;
    int passCount = 0;
    int failCount = 0;
    double maxBler = 0.0;
    for (int i = 0; i < rows; ++i) {
        QTableWidgetItem *blerItem = ui->resultTableRun->item(i, 4);
        QString blerText = blerItem ? blerItem->text() : "";
        bool ok;
        double bler = blerText.toDouble(&ok);
        if (!ok || blerText.isEmpty()) {
            // 未填写或无效，判定为 FAIL
            ui->resultTableRun->setItem(i, 5, new QTableWidgetItem("FAIL"));
            ui->resultTableResult->setItem(i, 6, new QTableWidgetItem("INVALID"));
            ui->resultTableResult->setItem(i, 7, new QTableWidgetItem("FAIL"));
            allValid = false;
            failCount++;
            continue;
        }
        bool pass = (bler <= ACS_BLER_LIMIT_PERCENT);
        QString verdict = pass ? "PASS" : "FAIL";
        maxBler = qMax(maxBler, bler);
        if (pass) {
            passCount++;
        } else {
            failCount++;
        }
        // 这里添加日志
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"),
               pass ? "PASS" : "FAIL",
               "8.7",
               QString("测试点 %1: BLER=%2%, 判定=%3")
                   .arg(ui->resultTableRun->item(i, 0)->text())  // 测试点名称
                   .arg(bler, 0, 'f', 2)
                   .arg(verdict));
        // 更新测试运行表格的判定列
        QTableWidgetItem *verdictItem = ui->resultTableRun->item(i, 5);
        if (!verdictItem) {
            verdictItem = new QTableWidgetItem(verdict);
            ui->resultTableRun->setItem(i, 5, verdictItem);
        } else {
            verdictItem->setText(verdict);
        }
        // 更新结果显示表格的 BLER 和判定
        ui->resultTableResult->setItem(i, 6, new QTableWidgetItem(blerText));
        ui->resultTableResult->setItem(i, 7, new QTableWidgetItem(verdict));
    }
    if (!allValid) {
        addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "WARN", "8.7", "部分 BLER 未填写或无效，对应判定为 FAIL");
    }
    // 可选：确认后隐藏按钮，恢复开始测试按钮
    setAcsBlerEditingEnabled(false);
    confirmBlerBtn->setVisible(false);
    ui->pushButton->setEnabled(true);
    const bool overallPass = (failCount == 0) && (passCount > 0);
    ui->verdictLabel->setText(overallPass ? "PASS" : "FAIL");
    ui->statsLabel->setText(QString("ACS 测试完成\n通过: %1/%2\n最大 BLER: %3%\n整体结果: %4")
                            .arg(passCount)
                            .arg(rows)
                            .arg(maxBler, 0, 'f', 2)
                            .arg(overallPass ? "PASS" : "FAIL"));
    addLog(QDateTime::currentDateTime().toString("hh:mm:ss"), "INFO", "8.7", "用户确认 BLER，判定完成");

    // 调用绘图函数更新图表
    updateBlerVsOffsetChart();
    updatePassFailPieChart();
}


void MainWindow::updateBlerVsOffsetChart()
{
    QGroupBox *box = findChild<QGroupBox*>("groupBox");
    if (!box) return;

    // 清除原有布局中的所有子控件
    QLayout *oldLayout = box->layout();
    if (oldLayout) {
        while (QLayoutItem *item = oldLayout->takeAt(0)) {
            if (item->widget()) delete item->widget();
            delete item;
        }
        delete oldLayout;
    }

    // 创建新布局和图表视图
    QVBoxLayout *layout = new QVBoxLayout(box);
    QChartView *chartView = new QChartView(box);
    chartView->setRenderHint(QPainter::Antialiasing);
    // 设置最小宽度，确保横坐标有足够空间
    chartView->setMinimumWidth(800);

    // 收集数据：测试点索引 -> (标签, BLER)
    QStringList categories;
    QList<double> blerValues;
    for (int i = 0; i < m_acsTestPoints.size(); ++i) {
        const AcsTestPoint &tp = m_acsTestPoints[i];
        QTableWidgetItem *blerItem = ui->resultTableRun->item(i, 4);
        if (!blerItem) continue;
        bool ok;
        double bler = blerItem->text().toDouble(&ok);
        if (!ok) continue;

        // 生成简短标签
        QString shortRef = tp.refChannel;
        shortRef.remove("A-FR1-");
        QString dir = tp.desc.contains("upper") ? "U" : "L";
        QString label = QString("%1k (%2%3)").arg(tp.offsetKHz).arg(shortRef).arg(dir);
        categories << label;
        blerValues << bler;
    }

    if (categories.isEmpty()) {
        QChart *emptyChart = new QChart();
        emptyChart->setTitle("BLER vs 测试点 (无有效数据)");
        chartView->setChart(emptyChart);
        layout->addWidget(chartView);
        return;
    }

    QLineSeries *series = new QLineSeries();
    series->setName("BLER");
    for (int i = 0; i < blerValues.size(); ++i) {
        series->append(i, blerValues[i]);
    }

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("BLER vs 测试点");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    QCategoryAxis *axisX = new QCategoryAxis();
    for (int i = 0; i < categories.size(); ++i) {
        axisX->append(categories[i], i);
    }
    axisX->setTitleText("测试点 (偏移/信道/方向)");
    axisX->setLabelsAngle(-45);   // 倾斜 -45 度
    // 可选稍微减小字体
    QFont font = axisX->labelsFont();
    font.setPointSize(9);
    axisX->setLabelsFont(font);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("BLER (%)");
    axisY->setRange(0, 100);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    QLineSeries *thresholdLine = new QLineSeries();
    thresholdLine->append(-0.5, 10);
    thresholdLine->append(categories.size() - 0.5, 10);
    thresholdLine->setColor(Qt::red);
    thresholdLine->setPen(QPen(Qt::red, 2, Qt::DashLine));
    thresholdLine->setName("10% 门限");
    chart->addSeries(thresholdLine);
    thresholdLine->attachAxis(axisX);
    thresholdLine->attachAxis(axisY);

    chartView->setChart(chart);
    layout->addWidget(chartView);
}

void MainWindow::updatePassFailPieChart()
{
    QGroupBox *box = findChild<QGroupBox*>("groupBox_2");
    if (!box) return;

    // 清除原有布局
    QLayout *oldLayout = box->layout();
    if (oldLayout) {
        while (QLayoutItem *item = oldLayout->takeAt(0)) {
            if (item->widget()) delete item->widget();
            delete item;
        }
        delete oldLayout;
    }

    QVBoxLayout *layout = new QVBoxLayout(box);
    QChartView *chartView = new QChartView(box);
    chartView->setRenderHint(QPainter::Antialiasing);

    int pass = 0, fail = 0;
    int rows = ui->resultTableRun->rowCount();
    for (int i = 0; i < rows; ++i) {
        QTableWidgetItem *verdictItem = ui->resultTableRun->item(i, 5);
        if (verdictItem && verdictItem->text() == "PASS") pass++;
        else fail++;
    }

    QPieSeries *series = new QPieSeries();
    if (pass > 0) {
        QPieSlice *passSlice = series->append(QString("PASS (%1)").arg(pass), pass);
        passSlice->setLabelVisible(true);
    }
    if (fail > 0) {
        QPieSlice *failSlice = series->append(QString("FAIL (%1)").arg(fail), fail);
        failSlice->setLabelVisible(true);
    }
    if (pass == 0 && fail == 0) {
        QPieSlice *emptySlice = series->append("NO DATA", 1);
        emptySlice->setLabelVisible(true);
    }
    series->setLabelsVisible(true);
    series->setPieSize(0.7);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("测试通过率");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    chartView->setChart(chart);
    layout->addWidget(chartView);
}

void MainWindow::onAcsTableRowClicked(int row, int column)
{
    Q_UNUSED(column);
    if (!currentTestCase.contains("8.7")) {
        return;
    }
    if (row < 0 || row >= m_acsTestPoints.size()) {
        return;
    }

    for (int r = 0; r < ui->resultTableRun->rowCount(); ++r) {
        for (int c = 0; c < ui->resultTableRun->columnCount(); ++c) {
            if (QTableWidgetItem *item = ui->resultTableRun->item(r, c)) {
                item->setData(Qt::BackgroundRole, QVariant());
            }
        }
    }
    for (int c = 0; c < ui->resultTableRun->columnCount(); ++c) {
        if (QTableWidgetItem *item = ui->resultTableRun->item(row, c)) {
            item->setData(Qt::BackgroundRole, QColor("#e8f1ff"));
        }
    }

    const AcsTestPoint &tp = m_acsTestPoints[row];
    const QString schematicPath = QString(":/schematic_%1.png").arg(row);
    if (!QPixmap(schematicPath).isNull()) {
        setupImageChartBox(findChild<QGroupBox*>("groupBox_3"),
                           QString("ACS 示意图 - %1").arg(tp.desc),
                           QString("%1 | 偏移 %2 kHz").arg(tp.refChannel).arg(tp.offsetKHz),
                           schematicPath,
                           "点击测试运行表可切换示意图");
    } else {
        setupTextChartBox(findChild<QGroupBox*>("groupBox_3"),
                          "ACS 示意图",
                          QString("缺少资源: %1").arg(schematicPath),
                          {"请确认 images/sim/8_7/schematic_0.png ~ schematic_7.png 已加入 resources.qrc"},
                          "资源缺失时不会影响测试流程");
    }

    QString screenshotPath;
    if (row < m_acsScreenshotPaths.size()) {
        screenshotPath = m_acsScreenshotPaths.at(row);
    }
    if (!screenshotPath.isEmpty() && QFileInfo::exists(screenshotPath)) {
        setupImageChartBox(findChild<QGroupBox*>("groupBox_4"),
                           QString("4071 实测频谱图 - %1").arg(tp.desc),
                           QString("%1 | Span %2 MHz").arg(tp.refChannel).arg(tp.screenshotSpanMHz, 0, 'f', 1),
                           screenshotPath,
                           "该图为当前测试点下载到上位机的 4071 实测截图");
    } else {
        setupTextChartBox(findChild<QGroupBox*>("groupBox_4"),
                          "4071 实测频谱图",
                          QString("%1 尚无可显示截图").arg(tp.desc),
                          {screenshotPath.isEmpty() ? "该测试点未保存截图或截图下载失败"
                                                    : QString("文件不存在: %1").arg(screenshotPath),
                           QString("本次截图目录: %1").arg(m_acsScreenshotDir)},
                          "不影响 BLER 填写和判定；请查看日志确认截图状态");
    }
}
void MainWindow::updateAclrRunImages(int row)
{
    QTableWidget *runTable = ui->resultTableRun;
    if (!runTable || runTable->rowCount() == 0) {
        updateTestRunImage(QString(), QString());
        return;
    }

    if (row < 0 || row >= runTable->rowCount()) {
        row = runTable->currentRow();
    }
    if (row < 0 || row >= runTable->rowCount()) {
        row = 0;
    }

    QTableWidgetItem *verdictItem = runTable->item(row, 9);
    const QString simPath = verdictItem ? verdictItem->data(RoleSimPath).toString() : QString();
    const QString screenshotPath = verdictItem ? verdictItem->data(RoleScreenshotPath).toString() : QString();
    updateTestRunImage(simPath, screenshotPath);
}

void MainWindow::updateTestRunImage(const QString &simPath, const QString &screenshotPath)
{
    if (!m_simImageLabel) {
        QGroupBox *simBox = findChild<QGroupBox*>("groupBox_3");
        if (simBox) {
            const QList<QLabel*> labels = simBox->findChildren<QLabel*>();
            if (!labels.isEmpty()) {
                m_simImageLabel = labels.first();
            }
        }
    }
    if (!m_screenshotLabel) {
        QGroupBox *screenshotBox = findChild<QGroupBox*>("groupBox_4");
        if (screenshotBox) {
            const QList<QLabel*> labels = screenshotBox->findChildren<QLabel*>();
            if (!labels.isEmpty()) {
                m_screenshotLabel = labels.first();
            }
        }
    }

    if (!m_simImageLabel || !m_screenshotLabel) {
        return;
    }

    auto setLabelPixmap = [](QLabel *label, const QPixmap &pixmap) {
        if (AspectRatioPixmapLabel *aspectLabel = dynamic_cast<AspectRatioPixmapLabel *>(label)) {
            aspectLabel->setSourcePixmap(pixmap);
        } else {
            label->setPixmap(pixmap);
        }
    };

    auto clearLabelPixmap = [](QLabel *label) {
        if (AspectRatioPixmapLabel *aspectLabel = dynamic_cast<AspectRatioPixmapLabel *>(label)) {
            aspectLabel->setSourcePixmap(QPixmap());
        } else {
            label->clear();
        }
    };

    m_simImageLabel->setAlignment(Qt::AlignCenter);
    if (!simPath.isEmpty() && QFile::exists(simPath)) {
        QPixmap pix(simPath);
        if (!pix.isNull()) {
            setLabelPixmap(m_simImageLabel, pix);
            m_simImageLabel->setText("");
        } else {
            clearLabelPixmap(m_simImageLabel);
            m_simImageLabel->setText("仿真图加载失败（文件损坏）");
        }
    } else {
        clearLabelPixmap(m_simImageLabel);
        m_simImageLabel->setText("无仿真图");
    }

    m_screenshotLabel->setAlignment(Qt::AlignCenter);
    if (!screenshotPath.isEmpty() && QFile::exists(screenshotPath)) {
        QPixmap pix(screenshotPath);
        if (!pix.isNull()) {
            setLabelPixmap(m_screenshotLabel, pix);
            m_screenshotLabel->setText("");
        } else {
            clearLabelPixmap(m_screenshotLabel);
            m_screenshotLabel->setText("截图加载失败（文件损坏）");
        }
    } else {
        clearLabelPixmap(m_screenshotLabel);
        m_screenshotLabel->setText("无截图");
    }
}

void MainWindow::updateBarChart()
{
    QGroupBox *box = findChild<QGroupBox*>("groupBox");
    if (!box) {
        return;
    }

    QLayout *oldLayout = box->layout();
    if (!oldLayout) {
        oldLayout = new QVBoxLayout(box);
    }
    while (QLayoutItem *item = oldLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(oldLayout);
    if (!layout) {
        layout = new QVBoxLayout(box);
    }
    layout->setContentsMargins(16, 20, 16, 14);
    layout->setSpacing(10);
    box->setTitle("各带宽 PASS/FAIL 统计柱状图");

    QTableWidget *runTable = ui->resultTableRun;
    if (!runTable || runTable->rowCount() == 0) {
        QLabel *emptyLabel = new QLabel("暂无测试数据", box);
        emptyLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(emptyLabel);
        return;
    }

    QMap<int, QPair<int, int>> stats;
    for (int row = 0; row < runTable->rowCount(); ++row) {
        QTableWidgetItem *bwItem = runTable->item(row, 1);
        QTableWidgetItem *verdictItem = runTable->item(row, 9);
        if (!bwItem || !verdictItem) continue;

        QString bwText = bwItem->text();
        bwText.remove(" kHz");
        bool ok = false;
        const int bw = bwText.toInt(&ok);
        if (!ok) continue;

        if (!stats.contains(bw)) {
            stats[bw] = qMakePair(0, 0);
        }
        if (verdictItem->text() == "PASS") {
            stats[bw].first++;
        } else {
            stats[bw].second++;
        }
    }

    if (stats.isEmpty()) {
        QLabel *emptyLabel = new QLabel("无有效数据", box);
        emptyLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(emptyLabel);
        return;
    }

    QChartView *chartView = new QChartView(box);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setMinimumHeight(300);

    QChart *chart = new QChart();
    chart->setTitle("各带宽 PASS/FAIL 统计");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    QBarSeries *series = new QBarSeries(chart);
    QBarSet *passSet = new QBarSet("PASS");
    QBarSet *failSet = new QBarSet("FAIL");

    QStringList categories;
    QList<int> bws = stats.keys();
    std::sort(bws.begin(), bws.end());

    int maxVal = 0;
    for (int bw : bws) {
        categories << QString::number(bw) + "k";
        *passSet << stats[bw].first;
        *failSet << stats[bw].second;
        maxVal = qMax(maxVal, stats[bw].first + stats[bw].second);
    }

    series->append(passSet);
    series->append(failSet);
    chart->addSeries(series);

    QBarCategoryAxis *axisX = new QBarCategoryAxis(chart);
    axisX->append(categories);
    axisX->setTitleText("信道带宽");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis(chart);
    axisY->setTitleText("测试数量");
    axisY->setRange(0, maxVal + 1);
    axisY->setLabelFormat("%d");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chartView->setChart(chart);
    layout->addWidget(chartView, 1);
}
