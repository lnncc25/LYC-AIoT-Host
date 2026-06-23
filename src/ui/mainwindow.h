#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QVector>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QCategoryAxis>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QT_CHARTS_USE_NAMESPACE
#endif

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class Analyzer4071;
class Generator1466;
class InstrumentSession;
class QSerialPort;
class QGroupBox;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;
QT_END_NAMESPACE

struct AcsTestPoint {
    QStringList waveformCandidates;
    QString refChannel;
    int bwKHz;
    int offsetKHz;               // 偏移值（带符号）
    double screenshotSpanMHz;
    QString desc;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectionManager();
    void onStartBackend();
    void onStopBackend();
    void onImportConfig();
    void onExportConfig();
    void onGenerateReport();
    void onTestCaseClicked(QTreeWidgetItem *item, int column);
    void onLoadPreset();
    void onSavePreset();
    void onPauseTest();
    void onStopTest();
    void onExportCsv();
    void onSaveScreenshot();
    void onViewHistory();
    void onExportResult();
    void onClearLog();
    void onExportLog();
    void onInstrumentReadyRead();       // 处理仪表返回数据
    void onInstrumentConnected();       // 连接成功槽
    void onSignalGeneratorReadyRead();
    void onSignalGeneratorConnected();
    void onSendCustomCommand();
    void onStartTest();                 // 开始测试按钮槽函数
    void onConfirmBler();               // 8.7 确认 BLER
    void onConfirmBlerVerdict();        // 8.6 确认 BLER 判定
    void onBwComboCurrentIndexChanged(int index);
    void onAcsTableRowClicked(int row, int column);
    void onTestRowClicked(int row, int column);


signals:
    void voltageReceived(double voltage);   // 用于等待电压值

private:
    void createToolBar();
    void initComboBoxes();
    void enhanceUi();
    void decorateTables();
    void polishTableItem(QTableWidgetItem *item, bool compactLongText = false);
    void polishTable(QTableWidget *table, bool compactLongText = true);
    void polishLogTable();
    void polishResultTables();
    void setupChartBox(QGroupBox *box, const QString &title, const QString &subtitle);
    void setupTextChartBox(QGroupBox *box, const QString &title, const QString &subtitle,
                           const QStringList &lines, const QString &legend = QString());
    void ensureOutputPowerPointsEditor();
    void ensureRefSensitivityPowerEditor();
    void ensureAcsTotalPowerEditor();
    void updateParameterFieldVisibility(const QString &caseName);
    double refSensitivityLinkLossCompensation() const;
    QList<double> outputPowerTestPoints(bool *ok = nullptr, QString *errorMessage = nullptr) const;
    void setupOutputPowerSchematicChart();
    void updateOutputPowerRunLiveChart(const QVector<double> &timeSec,
                                       const QVector<double> &measuredPowerDbm,
                                       const QVector<double> &targetPowerDbm,
                                       bool running);
    void updateOutputPowerTrendChart(const QVector<double> &timeSec,
                                     const QVector<double> &measuredPowerDbm,
                                     const QVector<double> &targetPowerDbm);
    void resetCaseRuntimeState();
    void applyCaseTemplate(const QString &caseName);
    void applyCaseResultTemplate(const QString &caseName);
    void setCaseInfoPanel(const QString &title, const QString &desc);
    void setParamCheckPanel(const QStringList &items,
                            const QStringList &states,
                            const QStringList &notes);
    void setDispatchQueuePanel(const QStringList &commands,
                               const QStringList &states);
    void setValidationMessages(const QStringList &messages);
    void updateRefSensitivityCharts();
    void updateRefSensitivityRunGraphs(int row = -1);
    QString refSensitivityTheoryImagePath(const QString &channel) const;
    QString judgeRefSensitivityRow(int row, bool logResult = false);
    void refreshRefSensitivityOverallStatus(bool finalConfirmation = false);
    void applyLogLevelFilter();
    void addLog(const QString &time, const QString &level, const QString &src, const QString &msg);
    void connectToInstrument(const QString& ip, quint16 port);
    void connectToSignalGenerator(const QString& ip, quint16 port);
    void connectToInstruments(const QString &analyzerIp, quint16 analyzerPort,
                              const QString &generatorIp, quint16 generatorPort);
    void sendScpiCommand(const QString& cmd);
    void sendScpiCommand(InstrumentSession *session, const QString &src, const QString &cmd);
    QString queryScpi(InstrumentSession *session, const QString &src, const QString &cmd, int timeoutMs = 2000);
    QByteArray queryScpiBinaryBlock(InstrumentSession *session, const QString &src, const QString &cmd, int timeoutMs = 5000);
    bool readVoltageWithTimeout(double &voltage, int timeoutMs = 2000);
    void runTest_8_1();
    void runTest_8_2();
    void runTest_8_3();
    void runTest_8_4();
    QString sendQuery(const QString &cmd, int timeoutMs = 2000);
    void runTest_8_5();
    void runTest_8_6();
    void runTest_8_7();                 // 原有的模拟测试
    bool hasSpectrumAnalyzer() const;
    bool hasSignalGenerator() const;
    bool runRealOutputPowerLinkTest();
    bool configure1466Cw(double freqMHz, double powerDbm, bool outputOn, bool enforceSafeClamp = true);
    bool shutdownSignalGeneratorOutput(const QString &reason, bool verify = true);
    bool stopSpectrumAnalyzerMeasurement(const QString &reason, bool verify = true);
    bool configure4071Spectrum(double centerMHz, double spanMHz, double rbwKHz, double vbwKHz);
    bool read4071Peak(double &peakFreqMHz, double &peakPowerDbm);
    double parseFirstDouble(const QString &text, bool *ok = nullptr) const;
    bool parse4071AcpowerResponse(const QString &response,
                                  double &mainPowerDbm,
                                  double &leftAclrDb,
                                  double &rightAclrDb,
                                  QString *details = nullptr) const;
    QStringList drainErrorQueue(InstrumentSession *session, const QString &src, int maxReads = 8);
    // 1466 信号发生器控制
    bool configure1466ArbPlayback(const QString &fileName,
                                  double freqMHz,
                                  double powerDbm,
                                  double sampleClockMHz,
                                  const QString &logContext,
                                  QString *errorDetails = nullptr);
    bool load1466ArbWaveform(const QString &fileName, double freqMHz, double powerDbm);
    bool configure1466RefSensitivityWaveform(const QString &fileName,
                                             double freqMHz,
                                             double outputPowerDbm,
                                             double sampleRateHz);
    bool start1466ArbOutput(bool enable);
    // 4071 频谱仪 ACLR 测量
    bool measure4071ACLR(double &mainPowerDbm, double &leftAclrDb, double &rightAclrDb);
    bool set4071FrequencyAndSpan(double centerMHz, double spanMHz);
    bool set4071RBWVBW(double rbwKHz, double vbwKHz);
    bool perform4071SingleSweep();
    bool configure4071ACLR(double centerFreqMHz,
                           double carrierBW_kHz,
                           double integrationBW_kHz,
                           double offset_kHz);
    bool save4071ScreenSnapshot(const QString &fileName);
    bool download4071File(const QString &remoteFileName, const QString &localFilePath);
    bool delete4071File(const QString &remoteFileName);
    bool check4071Capabilities();

    // ========== 新增：8.7 ACS 真实测试相关函数 ==========
    double getUsefulPowerForRefChannel(const QString &refChannel);           // 根据参考信道计算有用功率
    bool load1466AcsWaveform(const QStringList &fileNames,
                             double freqMHz,
                             double powerDbm,
                             QString *loadedFileName = nullptr);             // 加载 ACS 波形（支持候选文件名）
    QString capture4071ForAcs(const QString &testPointDesc,
                           double spanMHz,
                           const QString &localRunDir,
                           int pointIndex);                                  // 自动截图并保存到本地
    void runTest_8_7_real();    // 真实 ACS 测试主流程
    void set8_6BlerEditingEnabled(bool enabled);
    void setAcsBlerEditingEnabled(bool enabled);
    void updateBlerVsOffsetChart();
    void updatePassFailPieChart();
    void updateBarChart(QGroupBox *targetBox = nullptr);
    void refresh85ResultTableColumnWidths();
    struct TestRowData {
            QString simImagePath;      // 仿真图资源路径
            QString screenshotPath;    // 本地截图路径
            QString statusText;        // 用于状态显示的描述文字
        };

    Ui::MainWindow *ui;
    InstrumentSession *instrumentSocket;
    InstrumentSession *signalGeneratorSocket;
    Analyzer4071 *analyzer4071;
    Generator1466 *generator1466;
    QSerialPort *tagSerial;
    QString currentTestCase;
    bool testRunning;

    // 成员变量
    double lastVoltage;
    bool waitingForVoltage;
    QString lastIdnResponse;
    QString lastGeneratorIdnResponse;
    QPushButton *confirmBlerBtn;
    QList<AcsTestPoint> m_acsTestPoints;   // 保存 8.7 测试点信息
    QMap<int, QString> m_bandFreqPoints;
    QLabel *totalPowerLabel;
    QDoubleSpinBox *totalPowerSpin;
    QString m_acsScreenshotDir;              // 当前批次截图目录
    QList<QString> m_acsScreenshotPaths;     // 每个测试点的截图本地路径
    QLabel *m_simImageLabel;          // 用于显示仿真图
    QVector<TestRowData> m_testRowData;              // 存储每行数据
    QLabel *m_resultScreenshotLabel = nullptr;       // 结果页截图显示
    QPlainTextEdit *m_statusTextEdit = nullptr;      // 运行页状态文字显示

};

#endif // MAINWINDOW_H
