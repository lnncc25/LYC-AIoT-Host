#ifndef CONNECTIONDIALOG_H
#define CONNECTIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>

class ConnectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionDialog(QWidget *parent = nullptr);
    ~ConnectionDialog();

    void accept() override;   // 添加这一行

    QString getInstrumentIp() const { return instrumentIp->text(); }
    int getCtrlPort() const { return ctrlPort->value(); }
    QString getAnalyzerIp() const { return instrumentIp->text(); }
    int getAnalyzerPort() const { return ctrlPort->value(); }
    QString getSignalGeneratorIp() const { return cwIp->text(); }
    int getSignalGeneratorPort() const { return cwPort->value(); }

private slots:
    void onTestConnection();
    void onSavePreset();

private:
    void createUI();

    QComboBox *deviceTypeCombo;
    QLineEdit *instrumentIp;
    QSpinBox *ctrlPort;
    QSpinBox *resultPort;
    QComboBox *protocolCombo;

    QLineEdit *bsIp;
    QSpinBox *bsPort;
    QComboBox *bandCombo;
    QDoubleSpinBox *bsPower;

    QCheckBox *cwEnable;
    QLineEdit *cwIp;
    QSpinBox *cwPort;
    QDoubleSpinBox *cwFreq;
    QDoubleSpinBox *cwPower;

    QComboBox *serialPortCombo;
    QComboBox *baudRateCombo;
    QSpinBox *tagCountSpin;
    QComboBox *modulationCombo;
    QLineEdit *kFactors;

    QLabel *statusLabel;
};

#endif // CONNECTIONDIALOG_H
