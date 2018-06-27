#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QThread>
#include <QMessageBox>
#include <QDialog>
#include <QFileSystemWatcher>
#if TARGET_OS!=WINDOWS_OS
 #include <QtXlsx>
#endif
#include <QMutex>
#include <QEvent>
#include "utils.h"
#include "tsdialog.h"
#include "keyboard.h"
#include "reportdialog.h"
#include "saveusb.h"


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    int myFd;

    void SetPlotArea(QCustomPlot *widget);
    void SetValue(uint address, double value);

    virtual bool event(QEvent *event){
        if (event->type() == QEvent::TouchBegin ){
            event->accept();
            return true;
        }
        else if (event->type() == QEvent::TouchUpdate ){
            event->accept();
            return true;
        }
        else if (event->type() == QEvent::TouchEnd ){
            event->accept();
            return true;
        }

        return QMainWindow::event(event);
    }


protected:
    void changeEvent(QEvent *e);

private:
    void TestFunc();
    void SaveSettings();
    void LoadSettings();
    void Message(QString s, int type);

    QDateTime   date_time;
    QDir        dir_datafiles;       //директория сохранения отчетов
    QFile       datafile,           //файл с данными *.csv
                errorfile;          //файл с ошибками

#if TARGET_OS!=WINDOWS_OS
    QXlsx::Document xlsx;           //файл с данными *.xlsx
#endif

    QString     FileNameToSave;     //путь к файлу для сохранения
    QString     FlashDrivePath;     //путь к флешке

    QTimer  *ProgTimer;

    bool    bAlarmDetected,
            bLight,
            bCoolingStop;

    double  SampleT,//температура образца
            WaterT;//температура воды в начале испытаний
    short   Intens;//интенсивность осадков
    uint    ScreenOffDelay;

    double  getInitWaterT(void);
    uint GetScreenOffDelay();
    QString SetDataFileName();
    void ReadDataPoint(QTime tm, double WaterT, double RainIntens,
                                                double Tout,
                                                double Hum,
                                                double Pressure);

public slots:
    void realtimeDataSlot(); // пример построения графиков
    void xAxisRangeChanged( const QCPRange &newRange, const QCPRange &oldRange);
    void yAxisRangeChanged(const QCPRange &newRange, const QCPRange &oldRange);
    void yAxis2RangeChanged(const QCPRange &newRange, const QCPRange &oldRange);
    void mouseWheel(QWheelEvent*);

    void CheckSystemStatus();
    void ChangeProgramTime();
    void CheckStatusCode(uint status);
    void StopTest(uint);
    void ShowWaterTemp(double T);
    void ChangeSampleT(uint s, double T);
    void AddDataPoint(uint elapsed_time, double WaterT, double RainIntens);
    void WaitWaterLevel();
    void WaterLevelOk();
    void SetInitSampleTemp(double T);
    void SetInitWaterTemp(double T);
    void SetInitRainIntens(double I);
    void SetWaitStatus(uint h, uint m, uint s);
    void ShowTestTime(uint t);
    void SleepMonitor();
    void GetAmbientValues(double &Tout, double &Hum, double &Pressure);
    void SetTSampleValue(bool ok);
    void SetDelaySampleTemp(double T);
    void CheckDataFiles();
    void SaveGraphPict();
    void LoadDataFromFile(QString filename);
    void CreateCSVFileHeader(QFile    &datafile);
    void SetTestSettings(OvenThread *oth);
    void SetScreenOff(double value);
    void SaveFileToUsb();
    bool CreateXLSFileHeader(QString filename);
    void SetIP(QString addr);
    void SetMask(QString addr);
    void SetMainPort(QString addr);
    void SetPreferDNS(QString addr);
    void SetAltDNS(QString addr);
    void FillAlarmTable();
    void SetGreenField();
    void WaitStart(uint t);
private slots:
    void on_pbTableReport_clicked();

    void on_pbPlot_clicked();

    void on_pbSettings_clicked();

    void on_pbStart_clicked();

    void on_pbStop_clicked();

    void on_pbLight_clicked();

    void on_pbError_clicked();

    void on_chSampleTemp_clicked();

    void on_pbSendToUSB_clicked();

    void on_chbWaterTemp_clicked();

    void on_chbRainIntens_clicked();

    void on_pbPlotRight_clicked();

    void on_pbPlotLeft_clicked();

    void on_pbPlotFastLeft_clicked();

    void on_pbPlotFastRight_clicked();

    void on_pbChooseTestData_clicked();

    void on_pbBegin_clicked();

    void on_pbEnd_clicked();

    void on_pbUp_clicked();

    void on_pbDown_clicked();

    void on_pbHome_clicked();

    void on_pbAlarmUp_clicked();

    void on_pbAlarmDown_clicked();

    void on_pbAlarmBegin_clicked();

    void on_pbAlarmEnd_clicked();

    void on_pbAlarmDown10_clicked();

    void on_pbAlarmUp10_clicked();

    void on_lCurrentSampleTemp_clicked();

    void on_lInitWaterTemp_clicked();

    void on_lInitRainIntens_clicked();

    void on_tbTest_clicked();

    void on_tbDelayStart_clicked();

    void on_dteDelayStartTest_dateTimeChanged(const QDateTime &dateTime);

    void on_tbDateTime_clicked();

    void on_pbSetProgDateTime_clicked();

    void on_pbSampleTempInitDelay_clicked();

    void on_tbReportExport_clicked();

    void on_pbViewReport_clicked();

    void on_pbScalePlus_clicked();

    void on_pbScaleMinus_clicked();

    void on_pbDown10_clicked();

    void on_pbUp10_clicked();

    void on_pbPlotView_clicked();

    void on_tbScreen_clicked();

    void on_tbNetworkSettings_clicked();

    void on_sbScreenBrightness_valueChanged(int arg1);

    void on_pbScreenOffDelay_clicked();

    void on_chbTermoHumValue_clicked();

    void on_pbBack1_clicked();

    void on_pbBack2_clicked();

    void on_pbBack3_clicked();

    void on_pbBack4_clicked();

    void on_pbBack5_clicked();

    void on_pbBack6_clicked();

    void on_pbBack7_clicked();

    void on_pbBack_clicked();

    void on_pbShutdown_clicked();

    void on_pbIP_clicked();

    void on_pbMask_clicked();

    void on_pbMainPort_clicked();

    void on_pbPreferDNS_clicked();

    void on_pbAltDNS_clicked();   

    void on_pbSampleTemperature_clicked();

    void on_pbWaterTemp_clicked();

    void on_pbOnOffFridge_clicked();

    void on_pbRainRotating_clicked();

    void on_pbTableRotating_clicked();

    void on_pbStartDelay_clicked();

    void on_pbTermoHumValue_clicked();

    void on_pbShowRainIntens_clicked();

    void on_pbExportFileFormat_clicked();

private:
    Ui::MainWindow *ui;
    bool TestinProgress;
    short RainIntensType; //0 - показывать, 1 - не показывать, 2 - точно
signals:
    void check_status(uint);
    void stop_test(uint);
    void start_test();
    void del_keyboard();
    void del_dialog();
    void dislay_off();
    void test_off();
    void get_meas(bool);
};

int s();
int y(QString o);
void b_c(char *I, QString &O);
void b_dc(char *I, QString &O);


#endif // MAINWINDOW_H
