#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QStyle>
#include <QDesktopWidget>
#include <QDateTime>
#include <QDate>
#include <QTime>
#include <QCursor>

//--------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                       this->size(), qApp->desktop()->availableGeometry()));

    dir_datafiles.setPath(QDir::homePath());
    FlashDrivePath = "";

    errorfile.setFileName(dir_datafiles.absolutePath() + "/KD-1000/ErrorLog");
    FillAlarmTable();
    errorfile.open(QIODevice::Append | QIODevice::Text);

    SampleT = 0.0;
    WaterT = 0.0;
    Intens = 1;

    RainIntensType = 0;

    TestinProgress = false;
    bAlarmDetected = false;
    bLight = false;
    bCoolingStop = true;


    ProgTimer = new QTimer(this);
    ProgTimer->setInterval(1000);

    connect(ProgTimer, SIGNAL(timeout()), this, SLOT(ChangeProgramTime()));
    connect(this, SIGNAL(check_status(uint)), this, SLOT(CheckStatusCode(uint)));
    connect(this, SIGNAL(stop_test(uint)), this, SLOT(StopTest(uint)));
    connect(this, SIGNAL(dislay_off()), this, SLOT(SleepMonitor()));
    connect(this, SIGNAL(start_test()), this, SLOT(on_pbStart_clicked()));

    ui->swScreens->setCurrentIndex(0);
    ui->dsbLowTSample->setSuffix(trUtf8(" %1C").arg(QChar(176)));
    ui->dsbWaterTempInitDelay->setSuffix(trUtf8(" %1C").arg(QChar(176)));
    ui->dsbGisterezis->setSuffix(trUtf8(" %1C").arg(QChar(176)));
    ui->label_5->setText(trUtf8("Температура образца в начале испытания, %1C").arg(QChar(176)));
    ui->label_6->setText(trUtf8("Температура воды, %1C").arg(QChar(176)));

    SetPlotArea(ui->widgetPlot);        //настройка отображение графика

    date_time = QDateTime::currentDateTime();
    ui->dteDelayStartTest->setDateTime(date_time.addSecs(3));
    ui->lCurrentDate->setText(date_time.toString("dd.MM.yyyy"));
    ui->lCurrentTime->setText(date_time.toString("HH:mm:ss"));

    ui->leCurrentState->setText(QString("ОСТАНОВЛЕНО"));

    QFont tf = ui->twTable->font();
    tf.setPointSize(9);
    ui->twTable->setColumnCount(6);
    ui->twTable->setHorizontalHeaderItem(0, new QTableWidgetItem(trUtf8("Время")));
    ui->twTable->setHorizontalHeaderItem(1, new QTableWidgetItem(trUtf8("Темп.воды,%1C").arg(QChar(176))));
    ui->twTable->setHorizontalHeaderItem(2, new QTableWidgetItem(trUtf8("Интенс.,мм/мин")));
    ui->twTable->setHorizontalHeaderItem(3, new QTableWidgetItem(trUtf8("Tвозд.,%1С").arg(QChar(176))));
    ui->twTable->setHorizontalHeaderItem(4, new QTableWidgetItem(trUtf8("Отн.вл.возд,%")));
    ui->twTable->setHorizontalHeaderItem(5, new QTableWidgetItem(trUtf8("Атм.давл.,мм.")));
    ui->twTable->hideColumn(3);
    ui->twTable->hideColumn(4);
    ui->twTable->hideColumn(5);
    ui->twTable->setFont(tf);

    LoadSettings();
    ui->pbStartDelay->setText(trUtf8("Нет"));

#if TARGET_OS==RASPBIAN_OS
    bool ok;
    wiringPiSetup ();
    pinMode(RPI_PIN, OUTPUT);
    if ((myFd = wiringPiSPISetup (SPI_CHAN, SPI_SPEED)) < 0){
         check_status(STATUS_SPI_ERR);
    }
    else{
        //включение электромагнитного клапана
        ok = SetSPIParam(WATER_VALVE, 1);
        if(!ok)
            check_status(STATUS_SPI_ERR);
    }
#endif

    ProgTimer->start();
}
//--------------------------------------------------------------
MainWindow::~MainWindow()
{
    ProgTimer->disconnect();
    if(errorfile.isOpen())
        errorfile.close();
    delete ProgTimer;
    delete ui;
}
//--------------------------------------------------------------
void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}
//--------------------------------------------------------------
//изменение даты и времени в программе + запрос статуса
void MainWindow::ChangeProgramTime()
{
    date_time = date_time.addSecs(1);
    ui->lCurrentDate->setText(date_time.date().toString("dd.MM.yyyy"));
    ui->lCurrentTime->setText(date_time.time().toString("HH:mm:ss"));

#if TARGET_OS==RASPBIAN_OS
    QProcess *process = new QProcess();
    QStringList list;
    QString s;
    QByteArray out;

    out.clear();

    process->start("ls /media/pi");
    process->waitForFinished(-1);
    out = process->readAllStandardOutput();

    delete process;
    process = NULL;

    if(out.size()>1){
        FlashDrivePath = QString::fromUtf8(out);
        list = FlashDrivePath.split("\n");
        FlashDrivePath = "/media/pi/" + list.at(0);
        ui->pbSendToUSB->setEnabled(true);
        s = FlashDrivePath + "/" + QString("%1%2%3").arg("K").arg("e").arg("y");
        if (QFile(s).exists()){
            s = "cp " + QString("%1 %2").arg(s).arg(dir_datafiles.absolutePath());
            system(s.toLocal8Bit().data());
        }
    }
    else{
        ui->pbSendToUSB->setEnabled(false);
        FlashDrivePath = "";
    }
#endif

    CheckSystemStatus();
}
//--------------------------------------------------------------
//проверка исправности системы и считывание данных с датчиков
void MainWindow::CheckSystemStatus()
{
    uint error_code = SUCCESS_OPERATION;
    uint value;
    bool ok;
    double T, H, P;

    emit get_meas(false);

        //проверка статуса
        ok = GetSPIParam(STATUS, error_code);
        if(!ok){
            check_status(STATUS_SPI_ERR);
            return;
        }
        else
            check_status(error_code);

        //нажата кнопка "СТОП"
        ok = GetSPIParam(STOP_BUTTON, error_code);
        if(!ok){
            check_status(STATUS_SPI_ERR);
            return;
        }
        else
            if(error_code == 1)
                emit stop_test(SUCCESS_OPERATION);

        //температура снаружи
        ok = GetSPIParam(SENSOR_T_OUT, value);
        if(!ok){
            check_status(STATUS_SPI_ERR);
            return;
        }
        else
            T = value/VALUE_DELITEL;


        //влажность снаружи
        ok = GetSPIParam(SENSOR_HUMIDITY_OUT, value);
        if(!ok){
            check_status(STATUS_SPI_ERR);
            return;
        }
        else
            H = value/VALUE_DELITEL;


        //давление снаружи
        ok = GetSPIParam(SENSOR_PRESSURE_OUT, value);
        if(!ok){
            check_status(STATUS_SPI_ERR);
            return;
        }
        else
            P = value/13.3332;

        //проверка корректности полученных данных
        if(T>1.0 && T<100.0 && H>1.0 && H<100.0 && P>700.0 && P<800.0){
            ui->leAirTempExt->setText(QString("%1 %2C").arg(T, 0, 'f', 1).arg(QChar(176)));
            ui->leHumidityExt->setText(QString("%1 %").arg(H, 0, 'f', 0));
            ui->leExtPressure->setText(QString("%1 мм.рт.ст.").arg(P, 0, 'f', 0));
        }


        //нажата кнопка "СТАРТ"
        if(!TestinProgress){
            ok = GetSPIParam(START_BUTTON, error_code);
            if(!ok){
                check_status(STATUS_SPI_ERR);
                return;
            }

            if(error_code == 1){
                emit start_test();
                return;
            }
        }

    emit get_meas(true);
}
//--------------------------------------------------------------
//проверка статуса камеры
void MainWindow::CheckStatusCode(uint status)
{
    QString str = "";
    uint row_cnt;

    if(STATUS_ALARM_BUTTON&status)
        str = str + trUtf8("Нажата кнопка аварийной остановки!\n");

    if(STATUS_NO_WATER&status)
        str = str + trUtf8("Нет воды!\n");

    if(STATUS_PUMP_OVERHEATED&status)
        str = str + trUtf8("Перегрев насоса!\n");

    if(STATUS_T_WATER_NOTREACHED&status)
        str = str + trUtf8("Температура воды не достигнута!\n");

    if(STATUS_JANITOR_BLOCKED&status)
        str = str + trUtf8("Дворник заклинило!\n");

    if(STATUS_DOOR_OPENED&status)
        str = str + trUtf8("Открыта дверь!\n");

    if(STATUS_SPI_ERR&status)
        str = str + trUtf8("Ошибка SPI!\n");

    status &=~ STATUS_SPI_ERR;

    if(status!=SUCCESS_OPERATION){

        if(status == STATUS_DOOR_OPENED)
            return;

        emit stop_test(status);
        TestinProgress = false;

        //сирена при аварии
        if(!bAlarmDetected){
            SetSPIParam(SIREN, 1);
            Message(str, 0);
            SetSPIParam(SIREN, 0);
        }

        bAlarmDetected = true;

        ui->swScreens->setCurrentIndex(6);

        row_cnt = ui->twAlarm->rowCount();

        ui->pbStart->setEnabled(false);
        ui->pbLight->setEnabled(false);
        ui->pbStop->setEnabled(false);
        ui->pbPlot->setEnabled(false);
        ui->pbSettings->setEnabled(false);

        if(row_cnt>MAX_ROWS_ALARM_TABLE){
            ui->twAlarm->removeRow(0);
            row_cnt--;
        }

        if(row_cnt>0){
            if(ui->twAlarm->item(row_cnt-1,1)->text() != str){
                ui->twAlarm->setRowCount(row_cnt+1);
                ui->twAlarm->setItem(row_cnt, 0, new QTableWidgetItem(trUtf8("%1 %2").arg(ui->lCurrentDate->text())
                                                                                    .arg(ui->lCurrentTime->text())));
                ui->twAlarm->setItem(row_cnt, 1, new QTableWidgetItem(trUtf8("%1").arg(str)));
                ui->twAlarm->setCurrentCell(row_cnt, 1);
                ui->twAlarm->item(row_cnt,0)->setBackgroundColor(Qt::red);
                ui->twAlarm->item(row_cnt,1)->setBackgroundColor(Qt::red);

                row_cnt = ui->twAlarm->rowCount();
                if(row_cnt>1){
                    ui->twAlarm->item(row_cnt-2,0)->setBackgroundColor(Qt::white);
                    ui->twAlarm->item(row_cnt-2,1)->setBackgroundColor(Qt::white);
                }
                str = trUtf8("%1|%2\n").arg(ui->twAlarm->item(row_cnt-1,0)->text()).arg(ui->twAlarm->item(row_cnt-1,1)->text());
                errorfile.write(str.toLocal8Bit().data());
            }
            else{
                row_cnt--;
                ui->twAlarm->setItem(row_cnt, 0, new QTableWidgetItem(trUtf8("%1 %2").arg(ui->lCurrentDate->text())
                                                                                    .arg(ui->lCurrentTime->text())));
                ui->twAlarm->item(row_cnt,0)->setBackgroundColor(Qt::red);
                ui->twAlarm->setCurrentCell(row_cnt, 1);
            }
        }//end if(row_cnt>0)
        else{
            ui->twAlarm->setRowCount(row_cnt+1);
            ui->twAlarm->setItem(row_cnt, 0, new QTableWidgetItem(trUtf8("%1 %2").arg(ui->lCurrentDate->text())
                                                                                .arg(ui->lCurrentTime->text())));
            ui->twAlarm->setItem(row_cnt, 1, new QTableWidgetItem(trUtf8("%1").arg(str)));
            ui->twAlarm->setCurrentCell(row_cnt, 1);
            ui->twAlarm->item(row_cnt,0)->setBackgroundColor(Qt::red);
            ui->twAlarm->item(row_cnt,1)->setBackgroundColor(Qt::red);
            //if (row_cnt>1){
            str = trUtf8("%1|%2\n").arg(ui->twAlarm->item(row_cnt,0)->text()).arg(ui->twAlarm->item(row_cnt,1)->text());
            errorfile.write(str.toLocal8Bit().data());
        }
    }
    else{
        ui->pbStart->setEnabled(true);
        ui->pbLight->setEnabled(true);
        ui->pbStop->setEnabled(true);
        ui->pbPlot->setEnabled(true);
        ui->pbSettings->setEnabled(true);

        row_cnt = ui->twAlarm->rowCount();
        if(row_cnt>0){
            ui->twAlarm->item(row_cnt-1,0)->setBackgroundColor(Qt::white);
            ui->twAlarm->item(row_cnt-1,1)->setBackgroundColor(Qt::white);
        }

        bAlarmDetected = false;
    }
}
//--------------------------------------------------------------
//остановка испытаний
void MainWindow::StopTest(uint)
{
    emit test_off();

    if(ui->pbSampleTemperature->text()==trUtf8("Измерять"))
        ui->lCurrentSampleTemp->setText("-");

    ui->lCurrentRainIntens->setText(QString("-"));

    if(!bAlarmDetected){
        ui->pbStart->setEnabled(true);
        ui->pbSettings->setEnabled(true);
        ui->pbChooseTestData->setEnabled(true);
    }

    TestinProgress = false;
    ui->pbStartDelay->setText(trUtf8("Нет"));

    //выключение двигателя лейки
    SetSPIParam(RAIN_DRIVE, 0);

    //выключение двигателя столика
    SetSPIParam(TABLE_DRIVE, 0);

    //остановка испытаний
    SetSPIParam(START_STOP, 0);

    //выключение компрессора
    SetSPIParam(COMPRESSOR, 0);

    //включение вентилятора
    SetSPIParam(FAN, 1);

    QPalette p = ui->leCurrentState->palette();
    p.setColor(QPalette::Base, Qt::red);
    ui->leCurrentState->setPalette(p);
    ui->leCurrentState->setText(QString("ОСТАНОВЛЕНО"));

    if(datafile.isOpen())
        datafile.close();

    system("echo 0 > /sys/class/backlight/rpi_backlight/bl_power"); //вкл. дисплей
}
//--------------------------------------------------------------
//нажатие на аварийную кнопку
void MainWindow::on_pbError_clicked()
{
    int row_cnt;
    ui->swScreens->setCurrentIndex(6);
    row_cnt = ui->twAlarm->rowCount();
    if(row_cnt>0)
        ui->twAlarm->setCurrentCell(row_cnt-1, 1);
}
//--------------------------------------------------------------
//вкл/выкл освещения в камере
void MainWindow::on_pbLight_clicked()
{
    bool ok;

    bLight = !bLight;
    if(bLight){
        ok = SetSPIParam(LIGHT, 1);
    }
    else{
        ok = SetSPIParam(LIGHT, 0);
    }

    if(!ok)
        check_status(STATUS_SPI_ERR);
}
//--------------------------------------------------------------
//переход на экран графика
void MainWindow::on_pbPlot_clicked()
{
    ui->swScreens->setCurrentIndex(1);
}
//--------------------------------------------------------------
//переход на экран таблицы
void MainWindow::on_pbTableReport_clicked()
{
    ui->swScreens->setCurrentIndex(2);
    if(ui->twTable->rowCount()>0)
        ui->twTable->setCurrentCell(0,0);
}
//--------------------------------------------------------------
//переход на экран настроек
void MainWindow::on_pbSettings_clicked()
{
    ui->swScreens->setCurrentIndex(3);
}
//--------------------------------------------------------------
//в начало таблицы
void MainWindow::on_pbBegin_clicked()
{
    int col = ui->twTable->currentColumn();
    if(ui->twTable->rowCount()>0)
        ui->twTable->setCurrentCell(0, col);
}
//--------------------------------------------------------------
//в конец таблицы
void MainWindow::on_pbEnd_clicked()
{
    int row = ui->twTable->rowCount()-1;
    int col = ui->twTable->currentColumn();
    if(row>=0)
        ui->twTable->setCurrentCell(row, col);
}
//--------------------------------------------------------------
//на строку вверх в таблице
void MainWindow::on_pbUp_clicked()
{
    int row = ui->twTable->currentRow();
    int col = ui->twTable->currentColumn();

    row--;
    if(row>=0)
        ui->twTable->setCurrentCell(row, col);
}
//--------------------------------------------------------------
//на 10 строк вверх в таблице
void MainWindow::on_pbUp10_clicked()
{
    int row = ui->twTable->currentRow();
    int col = ui->twTable->currentColumn();

    row = row - 10;
    if(row>=0)
        ui->twTable->setCurrentCell(row, col);
}
//--------------------------------------------------------------
//на строку вниз в таблице
void MainWindow::on_pbDown_clicked()
{
    int row = ui->twTable->currentRow();
    int col = ui->twTable->currentColumn();

    row++;
    if(row<ui->twTable->rowCount())
        ui->twTable->setCurrentCell(row, col);
}
//--------------------------------------------------------------
//на 10 строк вниз в таблице
void MainWindow::on_pbDown10_clicked()
{
    int row = ui->twTable->currentRow();
    int col = ui->twTable->currentColumn();

    row = row + 10;
    if(row<ui->twTable->rowCount())
        ui->twTable->setCurrentCell(row, col);
}
//--------------------------------------------------------------
//нажатие на кнопку СТАРТ
void MainWindow::on_pbStart_clicked()
{
    int v;
    bool ok;
    uint error_code = SUCCESS_OPERATION;
    QDate d;
    d.setDate(2017,12,1);

    v = s();

    if(TestinProgress)
        return;

    TestinProgress = true;

    if(v!=5)
        if(QDateTime::currentDateTime().date()>d.addDays(v*30)){
            Message("", 0);
            return;
        }

    //проверка открытия двери
    ok = GetSPIParam(SENSOR_DOOR_OPEN, error_code);
    if(!ok){
        check_status(STATUS_SPI_ERR);
        return;
    }
    else{
        if(error_code!=SUCCESS_OPERATION)
            check_status(STATUS_DOOR_OPENED);
    }

    ui->swScreens->setCurrentIndex(0);

    QPalette p = ui->leCurrentState->palette();
    p.setColor(QPalette::Base, Qt::yellow);
    ui->leCurrentState->setPalette(p);

    SetSPIParam(SIREN, 1);
    QThread::msleep(1300);
    SetSPIParam(SIREN, 0);

    ok = SetSPIParam(WATER_VALVE, 1);
    if(!ok){
        check_status(STATUS_SPI_ERR);
        return;
    }

    ui->twTable->setRowCount(0);
    SetPlotArea(ui->widgetPlot);

    ui->pbStart->setEnabled(false);
    ui->pbSettings->setEnabled(false);
    ui->pbChooseTestData->setEnabled(false);

    FileNameToSave = SetDataFileName();
    if(ui->pbExportFileFormat->text() == trUtf8("XLSX")){
        FileNameToSave = FileNameToSave + ".xlsx";
        if(!CreateXLSFileHeader(FileNameToSave))
            return;
    }
    else{
        FileNameToSave = FileNameToSave + ".csv";
        datafile.setFileName(FileNameToSave);
        if(!datafile.open(QIODevice::WriteOnly | QIODevice::Text)){
            Message(trUtf8("Невозможно создать файл для сохранения данных!"), 0);
            return;
        }
        else
            CreateCSVFileHeader(datafile);
    }

    QThread *thread = new QThread;
    OvenThread *oth = new OvenThread(this);
    
    if(SampleT>0.0)
        ui->lTSampleInTable->setText(trUtf8("Темп.образца в начале испытания: %1 %2С").arg(SampleT).arg(QChar(176)));
    else
        ui->lTSampleInTable->setText(trUtf8("Темп.образца в начале испытания не задана."));
    
    ScreenOffDelay = GetScreenOffDelay();
    SaveSettings();
    SetTestSettings(oth);

    oth->moveToThread(thread);
    connect(thread, SIGNAL(started()), oth, SLOT(WaitingTestStart()));
    connect(thread, SIGNAL(finished()), oth, SLOT(deleteLater()));
    connect(oth, SIGNAL(finished()), thread, SLOT(quit()));
    connect(this, SIGNAL(test_off()), oth, SLOT(StoppedTest()));
    connect(this, SIGNAL(stop_test(uint)), oth, SLOT(StopTest(uint)));
    connect(this, SIGNAL(get_meas(bool)), oth, SLOT(GetMeas(bool)));
    connect(oth, SIGNAL(stop_test(uint)), this, SLOT(StopTest(uint)));
    connect(oth, SIGNAL(check_error(uint)), this, SLOT(CheckStatusCode(uint)));

    connect(oth, SIGNAL(waiting(uint)), this, SLOT(WaitStart(uint)));
    connect(oth, SIGNAL(change_value_sampleT(uint, double)), this, SLOT(ChangeSampleT(uint, double)));
    connect(oth, SIGNAL(water_level_waiting()), this, SLOT(WaitWaterLevel()));
    connect(oth, SIGNAL(water_level_ok()), this, SLOT(WaterLevelOk()));
    connect(oth, SIGNAL(change_value_waterT(double)), this, SLOT(ShowWaterTemp(double)));
    connect(oth, SIGNAL(change_test_time(uint)), this, SLOT(ShowTestTime(uint)));
    connect(oth, SIGNAL(data_readed(uint, double, double)), this, SLOT(AddDataPoint(uint, double, double)));
    connect(oth, SIGNAL(change_color_to_green()), this, SLOT(SetGreenField()));

    //включение света
    ok = SetSPIParam(LIGHT, 1);
    if(!ok){
        StopTest(STATUS_SPI_ERR);
        return;
    }

    //выключение вентилятора
    SetSPIParam(FAN, 0);

    thread->start();
    thread->setPriority(QThread::HighPriority);
}
//--------------------------------------------------------------
//отображение значения температуры образца на экране
void MainWindow::ChangeSampleT(uint s, double T)
{
    SampleT = T;
    ui->lCurrentSampleTemp->setText(QString("%1").arg(T, 0,'f', 1));
    if(ui->pbWaterTemp->text() == trUtf8("Авто")){
        WaterT = T-ui->dsbLowTSample->value();
        if(WaterT < 4.0)
            WaterT = 4.0;
        ui->lInitWaterTemp->setText(QString("%1").arg(WaterT,0,'f',1));
    }
    ui->leCurrentState->setText(QString("ИЗМЕР.Т ОБРАЗЦА %1").arg(s));
}
//--------------------------------------------------------------
//ожидание необходимого уровня воды
void MainWindow::WaitWaterLevel()
{
    ui->leCurrentState->setText(trUtf8("НАБОР ВОДЫ"));
}
//--------------------------------------------------------------
//необходимый уровень воды достигнут
void MainWindow::WaterLevelOk()
{
    ui->leCurrentState->setText(trUtf8("ОХЛАЖДЕНИЕ ВОДЫ"));
}
//--------------------------------------------------------------
//отображение значения температуры воды на экране
void MainWindow::ShowWaterTemp(double T)
{
    ui->lCurrentWaterTemp->setText(trUtf8("%1").arg(T, 0, 'f', 1));
}
//--------------------------------------------------------------
//отображение статуса камеры в работе
void MainWindow::ShowTestTime(uint t)
{
    uint h, m, s;
    h = t/3600;
    m = ((t - h*3600)/60);
    s = t - h*3600 - m*60;

    if(t == ScreenOffDelay)
        emit dislay_off();

    QTime tm;
    tm = tm.fromString(QString("%1:%2:%3").arg(h).arg(m).arg(s),"H:m:s");
    ui->leCurrentState->setText(trUtf8("В РАБОТЕ %1").arg(tm.toString("HH:mm:ss")));
}
//--------------------------------------------------------------
//отключение монитора
void MainWindow::SleepMonitor()
{
    system("echo 1 > /sys/class/backlight/rpi_backlight/bl_power");
}
//--------------------------------------------------------------
//считывание данных об окужающей среде
void MainWindow::GetAmbientValues(double &Tout, double &Hum, double &Pressure)
{
    QString str, s;
    QStringList list;
    bool ok;

    str = ui->leAirTempExt->text();
    s = str;
    str = str.simplified();
    list = str.split(" ");
    Tout = list.at(0).toDouble(&ok);
    if(!ok)
        Tout = -99.0;
    ui->leAirTempExt->setText(s);

    str = ui->leHumidityExt->text();
    s = str;
    str = str.simplified();
    list = str.split(" ");
    Hum = list.at(0).toDouble(&ok);
    if(!ok)
        Hum = -99.0;
    ui->leHumidityExt->setText(s);

    str = ui->leExtPressure->text();
    s = str;
    str = str.simplified();
    list = str.split(" ");
    Pressure = list.at(0).toDouble(&ok);
    if(!ok)
        Pressure = -99.0;
    ui->leExtPressure->setText(s);
}
//--------------------------------------------------------------
//установка температуры образца
void MainWindow::SetTSampleValue(bool ok)
{
    emit del_dialog();

    if(!ok){
        ui->pbSampleTempInitDelay->setText(trUtf8("Не использовать"));
    }
    else{
        keyboard *brd = new keyboard();

        connect(brd, SIGNAL(set_value(double)), this, SLOT(SetDelaySampleTemp(double)));
        connect(this, SIGNAL(del_keyboard()), brd, SLOT(deleteLater()));

        brd->exec();
    }
}
//--------------------------------------------------------------
//установка температуры на экране отложенного старта
void MainWindow::SetDelaySampleTemp(double T)
{
    emit del_keyboard();

    if(T>=1.0 && T<=40.0)
        ui->pbSampleTempInitDelay->setText(trUtf8("%1 %2C").arg(T).arg(QChar(176)));
    else{
        Message(trUtf8("Вне диапазона!\nВводимое значение температуры образца должно быть больше 1 и меньше 40."), 0);
        ui->pbSampleTempInitDelay->setText(trUtf8("Не использовать"));
    }
}
//--------------------------------------------------------------
//проверка количества файлов в директории
void MainWindow::CheckDataFiles()
{
    int N;
    QStringList data_files = dir_datafiles.entryList(QStringList("*.csv")<<"*.xlsx");

    N = data_files.count();
    if(N>ui->sbKolFiles->value()){
        QString filename_to_del;
        int i;

        filename_to_del = data_files[0];
        for(i=1; i<N; i++){
            if(QFileInfo(filename_to_del).created() > QFileInfo(data_files[i]).created())
                filename_to_del = data_files[i];
        }//end for(i)

        filename_to_del = dir_datafiles.absolutePath() + "/" + filename_to_del;
        QFile::setPermissions(filename_to_del, QFile::ReadOwner | QFile::WriteOwner);
        QFile(filename_to_del).remove();
    }//end if(N>MAX_DATA_FILES)
}
//--------------------------------------------------------------
//сохранение графика
void MainWindow::SaveGraphPict()
{
    QString name,
            exp="";
    QDateTime dt = QDateTime::currentDateTime();

    name = QString("Graph_%1_%2").arg(dt.toString("dd.MM.yyyy")).arg(dt.toString("HH;mm;ss"));

    QString filename = QFileDialog::getSaveFileName(this, "Save as...", name,
                                                    "PNG (*.png);;BMP (*.bmp);;PDF ( *.pdf);;"
                                                     "JPEG (*.jpg)");
    exp=filename.mid(filename.lastIndexOf(".")+1);
    exp=exp.toLower();

    if(exp == "png")
        ui->widgetPlot->savePng(filename);
    if(exp == "bmp")
        ui->widgetPlot->saveBmp(filename);
    if(exp == "jpg")
        ui->widgetPlot->saveJpg(filename);
    if(exp == "pdf")
        ui->widgetPlot->savePdf(filename);
}
//--------------------------------------------------------------
//загрузка данных из файла
void MainWindow::LoadDataFromFile(QString FileName)
{
    del_dialog();

    QString str;
    QFile file;
    QStringList list;
    double ST;
    bool ok, btm = false;
    QTime start_tm, tm;
    int N, test_time;
    uint i;
    uint h, m, s;

    if(FileName.length()<=4)
        return;

    FileName = dir_datafiles.absolutePath() + "/" + FileName;

    file.setFileName(FileName);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        Message(trUtf8("Невозможно прочитать файл с данными!"), 0);
        return;
    }

    SetPlotArea(ui->widgetPlot);
    ui->twTable->setRowCount(0);

    i=0;
    while(!file.atEnd()){
        str = file.readLine();  //читаем строку

        str = str.simplified();

        switch(i){
            case 3:     list = str.split(";;;;;;");
                        ui->lReportNameInTable->setText(list.at(0));
                        ui->lCurrentDataReportName->setText(list.at(0));
                        break;

            case 11:    list = str.split(" ");
                        ST = list.at(5).toDouble(&ok);
                        if(ok){
                            ui->lTSampleInTable->setText(QString("Темп.образца в начале испытания: %1 %2С").arg(ST).arg(QChar(176)));
                            SampleT = ST;
                        }
                        else{
                            ui->lTSampleInTable->setText(QString("Темп.образца в начале испытания: -"));
                            SampleT = 0;
                        }
                        break;

            case 15:    list = str.split(";");
                        N = list.size();
                        if(N==4){
                            ui->twTable->hideColumn(3);
                            ui->twTable->hideColumn(4);
                            ui->twTable->hideColumn(5);
                        }
                        else{
                            ui->twTable->showColumn(3);
                            ui->twTable->showColumn(4);
                            ui->twTable->showColumn(5);
                        }
                        break;

            default:break;
        };

        if(i>16 && str.length()>20){
            str = str.replace(QString(","), QString("."));
            if(str.at(str.length()-1) == '.')
                str = str.remove(str.length()-1, 1);
            list = str.split(";");
            tm = tm.fromString(list.at(1), "HH:mm:ss");
            if(!btm)
                start_tm = tm;
            N = list.size();

            test_time = start_tm.secsTo(tm);

            h = test_time/3600;
            m = ((test_time - h*3600)/60);
            s = test_time - h*3600 - m*60;
            tm = tm.fromString(QString("%1:%2:%3").arg(h).arg(m).arg(s),"H:m:s");

            btm = true;
            if(N>4)
                ReadDataPoint(tm, list.at(2).toDouble(),
                                list.at(3).toDouble(),
                                list.at(4).toDouble(),
                                list.at(5).toDouble(),
                                list.at(6).toDouble());
            else
                ReadDataPoint(tm, list.at(2).toDouble(),
                                list.at(3).toDouble(),
                                0.0,
                                0.0,
                                0.0);
        }

        i++;
    }

    if(!ui->chbTermoHumValue->isChecked()){
        ui->twTable->hideColumn(3);
        ui->twTable->hideColumn(4);
        ui->twTable->hideColumn(5);
    }
    else{
        ui->twTable->showColumn(3);
        ui->twTable->showColumn(4);
        ui->twTable->showColumn(5);
    }

    ui->widgetPlot->graph(0)->setVisible(ui->chSampleTemp->isChecked());
    ui->widgetPlot->graph(1)->setVisible(ui->chbWaterTemp->isChecked());
    ui->widgetPlot->graph(2)->setVisible(ui->chbRainIntens->isChecked());

    ui->widgetPlot->replot();
}
//--------------------------------------------------------------
//создание заголовка в CSV файле
void MainWindow::CreateCSVFileHeader(QFile    &datafile)
{
    QString s;
    char str[512];

    datafile.write(";;;ОТЧЕТ;;;\n");
    datafile.write(";;;по проведенному испытанию в камере дождя КД-1000;;;\n\n");
    sprintf(str, "%s\n\n", ui->lReportNameInTable->text().toUtf8().data());
    datafile.write(str);
    datafile.write("Подразделение: _________________________\n\n");
    datafile.write("Изделие: __________________________\n\n");
    datafile.write("ФИО испытателя: __________________________\n\n");

    if(SampleT>0.0)
        s = QString("Температура изделия в начале испытаний: %1 %2C\n\n").arg(SampleT).arg(QChar(176));
    else
        s = QString("Температура изделия в начале испытаний: не задана\n\n");

    sprintf(str, "%s", s.toLocal8Bit().data());
    datafile.write(str);

    datafile.write("Температура окружающего воздуха в начале испытаний: ________________________\n\n");

    datafile.write("Дата;Время;Tводы;Инт.;");
    if(ui->pbTermoHumValue->text()==trUtf8("Да")){
        datafile.write("Tвозд.;Отн.вл.возд;Атм.давл.");
    }
    datafile.write("\n");

    s = QString(";;(%1C);(мм/мин.);").arg(QChar(176));
    sprintf(str, "%s", s.toLocal8Bit().data());
    datafile.write(str);
    if(ui->pbTermoHumValue->text()==trUtf8("Да")){
        s = QString("(%1C);(%);(мм.рт.ст)").arg(QChar(176));
        sprintf(str, "%s", s.toLocal8Bit().data());
        datafile.write(str);
    }
    datafile.write("\n");
}
//--------------------------------------------------------------
//установка настроек для камеры
void MainWindow::SetTestSettings(OvenThread *oth)
{
    QStringList list;
    double T;
    bool ok;

    oth->TestDuration = (uint)(ui->spTestTime->value()*60);
    oth->WriteDataStep = (short)(ui->sbDataWriteInterval->value());

    //вращение дождевателя
    if(ui->pbRainRotating->text() == trUtf8("Да")){
        oth->RainDrive = 1;
        oth->RainMovingType = true;
    }
    else{
        oth->RainDrive = 0;
        oth->RainMovingType = true;
    }

    //холодильный агрегат
    if(ui->pbOnOffFridge->text() == trUtf8("Да"))
        oth->CoolingStop = true;
    else
        oth->CoolingStop = false;

    bCoolingStop = oth->CoolingStop;

    //вращение столика
    if(ui->pbTableRotating->text() == trUtf8("Да"))
        oth->TableDrive = true;
    else
        oth->TableDrive = false;

    //отложенный старт
    if(ui->pbStartDelay->text() == trUtf8("Да")){

        oth->bStartDelay = true;
        oth->delay = date_time.secsTo(ui->dteDelayStartTest->dateTime());
        oth->NeedMeasTSample = false;

        if(ui->pbSampleTempInitDelay->text() != trUtf8("Не использовать")){
            list = ui->pbSampleTempInitDelay->text().split(" ");
            T = list.at(0).toDouble(&ok);
            if(ok)
                SampleT = T;
            else
                SampleT = 0;
            ui->lCurrentSampleTemp->setText(QString("%1").arg(SampleT));
        }
        else
            ui->lCurrentSampleTemp->setText("-");

        oth->NeedSetTWater = true;

        oth->WaterT = ui->dsbWaterTempInitDelay->value();
        ui->lInitWaterTemp->setText(QString("%1").arg(oth->WaterT, 0, 'f', 1));
        oth->RainIntens = ui->sbRainIntensInitDelay->value();
        ui->lInitRainIntens->setText(trUtf8("%1").arg(oth->RainIntens, 0, 'd', 0));
    }
    //обычный запуск
    else{
        oth->bStartDelay = false;
        oth->delay = 0;

        if(ui->pbSampleTemperature->text() == trUtf8("Измерять"))
            oth->NeedMeasTSample = true;
        else
            oth->NeedMeasTSample = false;

        if(ui->pbWaterTemp->text() == trUtf8("Не охлаждать"))
            oth->NeedSetTWater = false;
        else{
            oth->NeedSetTWater = true;
            //ui->lInitWaterTemp->setText(QString("%1").arg(WaterT, 0, 'f', 1));
        }

        oth->WaterT = WaterT;
        oth->RainIntens = Intens;
        //ui->lInitRainIntens->setText(trUtf8("%1").arg(Intens, 0, 'd', 0));
    }

    //множитель интенсивности осадков
    oth->IntensCoeff = ui->dsbCalibRainValue->value();

    //гистерезис в режиме термостата
    oth->GisterezisT = ui->dsbGisterezis->value();
}
//--------------------------------------------------------------
//установка значения отключения экрана
void MainWindow::SetScreenOff(double value)
{
    del_keyboard();

    uint t = (uint)(value);

    if(t == TO_CH){
        ui->swScreens->setCurrentIndex(11);
    }
    else{
        ScreenOffDelay = t;
        ui->pbScreenOffDelay->setText(trUtf8("%1 мин").arg(t));
    }
}
//--------------------------------------------------------------
//сохранение файла на флешку
void MainWindow::SaveFileToUsb()
{
    QString str,filename, date, Tsample;
    char s[512];
    QStringList list;
    QFile df;
    int rows, columns;

    emit del_dialog();

    if(ui->lReportNameInTable->text().length()<20)
        return;

    str = ui->lReportNameInTable->text();
    str = str.simplified();
    list = str.split("№");
    filename = list.at(1).simplified();

    list = filename.split("-");
    date = list.at(0).simplified();

    list = ui->lTSampleInTable->text().split(":");
    Tsample = list.at(1).simplified();

    //!!!!
    //для винды будет ошибка, имя файла не должно содержать двоеточия!!!!!!!!
    //!!!!
    filename = filename.replace(QString("-"), QString("_"));
    filename = filename.replace(QString(":"), QString("-"));
    filename = FlashDrivePath + "/" + filename;

    if(ui->pbExportFileFormat->text() == trUtf8("XLSX")){
        filename = filename + ".xlsx";

#if TARGET_OS!=WINDOWS_OS
        QXlsx::Document xlsx;
        xlsx.write("D1", trUtf8("ОТЧЕТ"));
        xlsx.write("B2", trUtf8("по проведенному испытанию в камере дождя КД-1000"));
        xlsx.write("A4", trUtf8("%1").arg(ui->lReportNameInTable->text()));
        xlsx.write("A6", trUtf8("Подразделение: _________________________"));
        xlsx.write("A8", trUtf8("Изделие: __________________________"));
        xlsx.write("A10", trUtf8("ФИО испытателя: __________________________"));
        if(Tsample == "не задано")
            xlsx.write("A12", trUtf8("Температура изделия в начале испытаний: %1").arg(Tsample));
        else
            xlsx.write("A12", trUtf8("Температура изделия в начале испытаний: %1%2C").arg(Tsample).arg(QChar(176)));
        xlsx.write("A14", trUtf8("Температура окружающего воздуха в начале испытаний: ________________________"));

        rows = ui->twTable->rowCount(); //кол-во записе в таблице
        columns = ui->twTable->columnCount(); //кол-во колонок в таблице

        xlsx.write("A16", trUtf8("Дата"));
        xlsx.write("B16", trUtf8("Время"));
        xlsx.write("C16", trUtf8("Tводы"));
        xlsx.write("C17", trUtf8("(%1C)").arg(QChar(176)));
        xlsx.write("D16", trUtf8("Инт."));
        xlsx.write("D17", trUtf8("(мм/мин.)"));

        if(columns > 4){
            xlsx.write("E16", trUtf8("Tвозд."));
            xlsx.write("E17", trUtf8("(%1C)").arg(QChar(176)));
            xlsx.write("F16", trUtf8("Отн.вл.возд"));
            xlsx.write("F17", trUtf8("(%)"));
            xlsx.write("G16", trUtf8("Атм.давл."));
            xlsx.write("G17", trUtf8("(мм.рт.ст)"));
        }

        for (int i=0; i<rows; i++){
            xlsx.write(i+18, 1, date);
            for (int j=0; j<columns; j++){
                str = ui->twTable->item(i,j)->text().replace(QString("."), QString(","));
                xlsx.write(i+18, j+2, str);
            }
        }

        xlsx.saveAs(filename);
#endif
    }
    else{
        filename = filename + ".csv";

        df.setFileName(filename);

        if(!df.open(QIODevice::WriteOnly | QIODevice::Text)){
            Message(trUtf8("Невозможно создать файл для сохранения данных на USB-носителе!"), 0);
            return;
        }

        df.write(";;;ОТЧЕТ;;;\n");
        df.write(";;;по проведенному испытанию в камере дождя КД-1000;;;\n\n");
        sprintf(s, "%s\n\n", ui->lReportNameInTable->text().toUtf8().data());
        df.write(s);
        df.write("Подразделение: _________________________\n\n");
        df.write("Изделие: __________________________\n\n");
        df.write("ФИО испытателя: __________________________\n\n");
        if(Tsample == "не задано")
            str = QString("Температура изделия в начале испытаний: %1\n\n").arg(Tsample);
        else
            str = QString("Температура изделия в начале испытаний: %1\n\n").arg(Tsample);
        sprintf(s, "%s", str.toLocal8Bit().data());
        df.write(s);
        df.write("Температура окружающего воздуха в начале испытаний: ________________________\n\n");

        rows = ui->twTable->rowCount(); //кол-во записе в таблице
        columns = ui->twTable->columnCount(); //кол-во колонок в таблице

        df.write("Дата;Время;Tводы;Инт.;");
        if(columns > 4)
            df.write("Tвозд.;Отн.вл.возд;Атм.давл.");
        df.write("\n");

        str = QString(";;(%1C);(мм/мин.);").arg(QChar(176));
        sprintf(s, "%s", str.toLocal8Bit().data());
        df.write(s);
        if(columns > 4){
            str = QString("(%1C);(%);(мм.рт.ст)").arg(QChar(176));
            sprintf(s, "%s", str.toLocal8Bit().data());
            df.write(s);
        }
        df.write("\n");

        for(int i=0; i<rows; i++){
            sprintf(s, "%s;", date.toLocal8Bit().data());
            df.write(s);
            for(int j=0; j<columns; j++){
                    str = ui->twTable->item(i,j)->text().replace(QString("."), QString(","));
                    sprintf(s, "%s;", str.toLocal8Bit().data());
                    df.write(s);
            }
            df.write("\n");
        }
        df.close();
    }
}
//--------------------------------------------------------------
//сохранение файла в формате XLSX
bool MainWindow::CreateXLSFileHeader(QString filename)
{
#if TARGET_OS!=WINDOWS_OS
    xlsx.write("D1", trUtf8("ОТЧЕТ"));
    xlsx.write("B2", trUtf8("по проведенному испытанию в камере дождя КД-1000"));
    xlsx.write("A4", trUtf8("%1").arg(ui->lReportNameInTable->text()));
    xlsx.write("A6", trUtf8("Подразделение: _________________________"));
    xlsx.write("A8", trUtf8("Изделие: __________________________"));
    xlsx.write("A10", trUtf8("ФИО испытателя: __________________________"));

    if(SampleT>0.0)
        xlsx.write("A12", trUtf8("Температура изделия в начале испытаний: %1%2C").arg(SampleT).arg(QChar(176)));
    else
        xlsx.write("A12", trUtf8("Температура изделия в начале испытаний: не задана"));

    xlsx.write("A14", trUtf8("Температура окружающего воздуха в начале испытаний: ________________________"));
    xlsx.write("A16", trUtf8("Дата"));
    xlsx.write("B16", trUtf8("Время"));
    xlsx.write("C16", trUtf8("Tводы"));
    xlsx.write("C17", trUtf8("(%1C)").arg(QChar(176)));
    xlsx.write("D16", trUtf8("Инт."));
    xlsx.write("D17", trUtf8("(мм/мин.)"));

    if(ui->pbTermoHumValue->text() == trUtf8("Да")){
        xlsx.write("E16", trUtf8("Tвозд."));
        xlsx.write("E17", trUtf8("(%1C)").arg(QChar(176)));
        xlsx.write("F16", trUtf8("Отн.вл.возд"));
        xlsx.write("F17", trUtf8("(%)"));
        xlsx.write("G16", trUtf8("Атм.давл."));
        xlsx.write("G17", trUtf8("(мм.рт.ст)"));
    }

    return xlsx.saveAs(filename);
#else
    if(filename.length()>0)
        return true;
    else
        return false;
#endif
}
//--------------------------------------------------------------
//установка IP адреса на экране
void MainWindow::SetIP(QString addr)
{
    del_keyboard();
    ui->pbIP->setText(addr);
}
//--------------------------------------------------------------
//установка маски подсети на экране
void MainWindow::SetMask(QString addr)
{
    del_keyboard();
    ui->pbMask->setText(addr);
}
//--------------------------------------------------------------
//установка адреса основного шлюза на экране
void MainWindow::SetMainPort(QString addr)
{
    del_keyboard();
    ui->pbMainPort->setText(addr);
}
//--------------------------------------------------------------
//установка предпочитаемого DNS адреса
void MainWindow::SetPreferDNS(QString addr)
{
    del_keyboard();
    ui->pbPreferDNS->setText(addr);
}
//--------------------------------------------------------------
//установка альтернативного DNS адреса
void MainWindow::SetAltDNS(QString addr)
{
    del_keyboard();
    ui->pbAltDNS->setText(addr);
}
//--------------------------------------------------------------
//заполенение таблицы с авариями
void MainWindow::FillAlarmTable()
{
    //MAX_ROWS_ALARM_TABLE

    uint lines = 0;
    int row_cnt;
    QString str;
    QStringList list;
    uint i;

    if(!errorfile.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    while(!errorfile.atEnd()){
        str = errorfile.readLine();  //читаем строку
        if(str.length()>20)
            lines++;
    }

    errorfile.seek(0);

    if(lines>0){
        if(lines<=MAX_ROWS_ALARM_TABLE){
            while(!errorfile.atEnd()){
                str = errorfile.readLine();
                if(str.length()>20){
                    list = str.split("|");
                    row_cnt = ui->twAlarm->rowCount();
                    ui->twAlarm->setRowCount(row_cnt+1);
                    ui->twAlarm->setItem(row_cnt, 0, new QTableWidgetItem(trUtf8("%1").arg(list.at(0))));
                    ui->twAlarm->setItem(row_cnt, 1, new QTableWidgetItem(trUtf8("%2").arg(list.at(1))));
                }
            }//end while
        }//end if
        else{
            i = 0;
            while(!errorfile.atEnd()){
                str = errorfile.readLine();
                if(i>(MAX_ROWS_ALARM_TABLE-lines) && str.length()>20){
                    list = str.split("|");
                    row_cnt = ui->twAlarm->rowCount();
                    ui->twAlarm->setRowCount(row_cnt+1);
                    ui->twAlarm->setItem(row_cnt, 0, new QTableWidgetItem(trUtf8("%1").arg(list.at(0))));
                    ui->twAlarm->setItem(row_cnt, 1, new QTableWidgetItem(trUtf8("%2").arg(list.at(1))));
                    row_cnt++;
                }
                i++;
            }//end while
        }
     }
    errorfile.close();
}
//--------------------------------------------------------------
//ввод температуры образца с клавиатуры
void MainWindow::SetInitSampleTemp(double T)
{
     emit del_keyboard();

    if(T>=1.0 && T<=40.0){
        SampleT = T;
        ui->lCurrentSampleTemp->setText(QString("%1").arg(T, 0, 'f', 1));
        if(ui->pbWaterTemp->text() == trUtf8("Авто")){
            WaterT = T-ui->dsbLowTSample->value();
            if(WaterT<4.0)
                WaterT = 4.0;
            ui->lInitWaterTemp->setText(QString("%1").arg(WaterT, 0, 'f', 1));
        }
    }
    else
        Message(trUtf8("Вне диапазона!\nВводимое значение температуры образца должно быть больше 1 и меньше 40."), 0);
}
//--------------------------------------------------------------
void MainWindow::SetGreenField()
{
    QPalette p = ui->leCurrentState->palette();
    p.setColor(QPalette::Base, Qt::green);
    ui->leCurrentState->setPalette(p);
    bLight = true;
}
//--------------------------------------------------------------
//отображение на экране ожидания запуска
void MainWindow::WaitStart(uint t)
{
    uint h, m, s;
    QTime tm;

    h = t/3600;
    m = ((t - h*3600)/60);
    s = t - h*3600 - m*60;

    if(t == ScreenOffDelay)
        emit dislay_off();

    tm = tm.fromString(QString("%1:%2:%3").arg(h).arg(m).arg(s),"H:m:s");
    ui->leCurrentState->setText(trUtf8("ОТЛОЖ. ЗАПУСК %1").arg(tm.toString("HH:mm:ss")));
}
//--------------------------------------------------------------
//ввод температуры воды с клавиатуры
void MainWindow::SetInitWaterTemp(double T)
{
    emit del_keyboard();

    if(T>=4.0 && T<=30.0){
        ui->lInitWaterTemp->setText(QString("%1").arg(T, 0, 'f', 1));
        WaterT = T;
    }
    else
        Message(trUtf8("Вне диапазона!\nВводимое значение температуры воды должно быть больше 4 и меньше 30."), 0);
}
//--------------------------------------------------------------
//ввод интенсивности осадков с клавиатуры
void MainWindow::SetInitRainIntens(double I)
{
    emit del_keyboard();

    if(I>=1.0 && I<=12.0){
        ui->lInitRainIntens->setText(trUtf8("%1").arg(I, 0, 'd', 0));
        Intens = (short)(I);
    }
    else
        Message(trUtf8("Вне диапазона!\nВводимое значение интенсивности осадков должно быть больше 1 и меньше 12."), 0);
}
//--------------------------------------------------------------
//отображение статуса отложенного запуска
void MainWindow::SetWaitStatus(uint h, uint m, uint s)
{
    ui->leCurrentState->setText(trUtf8("ОТЛОЖ.ЗАПУСК %1:%2:%3").arg(h).arg(m).arg(s));
}
//--------------------------------------------------------------
//нажатие на кнопку стоп в программе
void MainWindow::on_pbStop_clicked()
{
    emit stop_test(SUCCESS_OPERATION);

    SetSPIParam(SIREN, 1);
    QThread::msleep(1300);
    SetSPIParam(SIREN, 0);
}
//--------------------------------------------------------------
//добавление данных в таблицу, на график и в файл
void MainWindow::AddDataPoint(uint elapsed_time, double WaterT, double RainIntens)
{
    int row_cnt = ui->twTable->rowCount();
    double mp;
    char str[256];
    double Tout, Hum, Pressure;

    if(RainIntensType!=0)
        RainIntens = Intens;

    ui->twTable->insertRow(row_cnt);
    ui->twTable->setItem(row_cnt, 0, new QTableWidgetItem(ui->lCurrentTime->text()));
    if(WaterT == 0.0)
        ui->twTable->setItem(row_cnt, 1, new QTableWidgetItem(QString("-")));
    else
        ui->twTable->setItem(row_cnt, 1, new QTableWidgetItem(QString("%1").arg(WaterT, 0, 'f', 1)));
    ui->twTable->setItem(row_cnt, 2, new QTableWidgetItem(QString("%1").arg(RainIntens, 0, 'f', 1)));
    ui->twTable->item(row_cnt, 0)->setTextAlignment(Qt::AlignCenter);
    ui->twTable->item(row_cnt, 1)->setTextAlignment(Qt::AlignCenter);
    ui->twTable->item(row_cnt, 2)->setTextAlignment(Qt::AlignCenter);

    if(ui->pbExportFileFormat->text() == trUtf8("XLSX")){
#if TARGET_OS!=WINDOWS_OS
        uint h, m, s;

        h = elapsed_time/3600;
        m = (elapsed_time - h*3600)/60;
        s = elapsed_time - h*3600 - m*60;

        xlsx.write(row_cnt+17, 1, ui->lCurrentDate->text());
        xlsx.write(row_cnt+17, 2, QString("%1:%2:%3").arg(h).arg(m).arg(s));
        if(WaterT == 0.0)
            xlsx.write(row_cnt+17, 3, QString("-"));
        else
            xlsx.write(row_cnt+17, 3, QString("%1").arg(WaterT, 0, 'f', 1));
        xlsx.write(row_cnt+17, 4, QString("%1").arg(RainIntens, 0, 'f', 1));
#endif
    }
    else{
        if(WaterT==0.0)
            sprintf(str, "%s;%s;-;%1.1f;", ui->lCurrentDate->text().toLocal8Bit().data(),
                                                ui->lCurrentTime->text().toLocal8Bit().data(), RainIntens);
        else
            sprintf(str, "%s;%s;%1.1f;%1.1f;", ui->lCurrentDate->text().toLocal8Bit().data(),
                                                ui->lCurrentTime->text().toLocal8Bit().data(), WaterT, RainIntens);
        datafile.write(str);
    }

    GetAmbientValues(Tout, Hum, Pressure);
    ui->twTable->setItem(row_cnt, 3, new QTableWidgetItem(QString("%1").arg(Tout, 0, 'f', 1)));
    ui->twTable->setItem(row_cnt, 4, new QTableWidgetItem(QString("%1").arg(Hum, 0, 'f', 0)));
    ui->twTable->setItem(row_cnt, 5, new QTableWidgetItem(QString("%1").arg(Pressure, 0, 'f', 0)));
    ui->twTable->item(row_cnt, 3)->setTextAlignment(Qt::AlignCenter);
    ui->twTable->item(row_cnt, 4)->setTextAlignment(Qt::AlignCenter);
    ui->twTable->item(row_cnt, 5)->setTextAlignment(Qt::AlignCenter);

    //если включены показания термогигрометра
    if(ui->pbTermoHumValue->text()==trUtf8("Да")){
        if(ui->pbExportFileFormat->text() == trUtf8("XLSX")){
#if TARGET_OS!=WINDOWS_OS
            xlsx.write(row_cnt+17, 6, QString("%1").arg(Tout, 0, 'f', 1));
            xlsx.write(row_cnt+17, 7, QString("%1").arg(Hum, 0, 'f', 0));
            xlsx.write(row_cnt+17, 8, QString("%1").arg(Pressure, 0, 'f', 0));
#endif
        }
        else{
            sprintf(str, "%1.1f;%1.1f;%1.1f", Tout, Hum, Pressure);
            datafile.write(str);
       }
    }

    if(ui->pbExportFileFormat->text() == trUtf8("CSV"))
        datafile.write("\n");

    if(WaterT!=0.0)
        ui->lCurrentWaterTemp->setText(QString("%1").arg(WaterT, 0, 'f', 1));

    if(RainIntensType == 1)
        ui->lCurrentRainIntens->setText(QString(" "));
    else
        ui->lCurrentRainIntens->setText(QString("%1").arg(RainIntens, 0, 'f', 1));

    ui->leAirTempExt->setText(QString("%1 %2C").arg(Tout, 0, 'f', 1).arg(QChar(176)));

    ui->leHumidityExt->setText(QString("%1 %").arg(Hum, 0, 'f', 0));
    ui->leExtPressure->setText(QString("%1 мм.рт.ст.").arg(Pressure, 0, 'f', 0));

    mp = elapsed_time/60.0;
    if(SampleT>0.0)
        ui->widgetPlot->graph(0)->addData(0, SampleT);
    if(WaterT!=0.0)
        ui->widgetPlot->graph(1)->addData(mp, WaterT);
    ui->widgetPlot->graph(2)->addData(mp, RainIntens);

    ui->widgetPlot->xAxis->setRange(mp, 10, Qt::AlignRight);
    ui->widgetPlot->replot();
}
//--------------------------------------------------------------
//добавление данных в таблицу и график из файла
void MainWindow::ReadDataPoint(QTime tm, double WaterT, double RainIntens, double Tout, double Hum, double Pressure)
{
    int row_cnt = ui->twTable->rowCount();
    double mp;

    mp = tm.hour()*60 + tm.minute() + tm.second()/60.0;

    ui->twTable->insertRow(row_cnt);
    ui->twTable->setItem(row_cnt, 0, new QTableWidgetItem(QString("%1").arg(tm.toString())));
    ui->twTable->setItem(row_cnt, 1, new QTableWidgetItem(QString("%1").arg(WaterT, 0, 'f', 1)));
    ui->twTable->setItem(row_cnt, 2, new QTableWidgetItem(QString("%1").arg(RainIntens, 0, 'f', 1)));
    ui->twTable->item(row_cnt, 0)->setTextAlignment(Qt::AlignCenter);
    ui->twTable->item(row_cnt, 1)->setTextAlignment(Qt::AlignCenter);
    ui->twTable->item(row_cnt, 2)->setTextAlignment(Qt::AlignCenter);


    if(Tout!=0){
        ui->twTable->setItem(row_cnt, 3, new QTableWidgetItem(QString("%1").arg(Tout, 0, 'f', 1)));
        ui->twTable->item(row_cnt, 3)->setTextAlignment(Qt::AlignCenter);
    }
    if(Hum!=0){
        ui->twTable->setItem(row_cnt, 4, new QTableWidgetItem(QString("%1").arg(Hum, 0, 'f', 0)));
        ui->twTable->item(row_cnt, 4)->setTextAlignment(Qt::AlignCenter);
    }
    if(Pressure!=0){
        ui->twTable->setItem(row_cnt, 5, new QTableWidgetItem(QString("%1").arg(Pressure, 0, 'f', 0)));
        ui->twTable->item(row_cnt, 5)->setTextAlignment(Qt::AlignCenter);
    }


    if(SampleT>0.0)
        ui->widgetPlot->graph(0)->addData(0, SampleT);
    ui->widgetPlot->graph(1)->addData(mp, WaterT);
    ui->widgetPlot->graph(2)->addData(mp, RainIntens);

    ui->widgetPlot->xAxis->setRange(mp, 10, Qt::AlignRight);
    ui->widgetPlot->replot();
}
//--------------------------------------------------------------
//включение графика с температурой образца
void MainWindow::on_chSampleTemp_clicked()
{
    ui->widgetPlot->graph(0)->setVisible(ui->chSampleTemp->isChecked());
    ui->widgetPlot->replot();
}
//--------------------------------------------------------------
//сохранение отчета в текстовом файле
void MainWindow::on_pbSendToUSB_clicked()
{
    SaveUsb *sv = new SaveUsb();
    QDir usb_path;

    usb_path.setPath(FlashDrivePath);

    connect(sv, SIGNAL(save_to_usb()), this, SLOT(SaveFileToUsb()));
    connect(this, SIGNAL(del_dialog()), sv, SLOT(deleteLater()));

    sv->FillList(usb_path);

    sv->exec();
}
//--------------------------------------------------------------
//включение графика с температурой воды
void MainWindow::on_chbWaterTemp_clicked()
{
    ui->widgetPlot->graph(1)->setVisible(ui->chbWaterTemp->isChecked());
    ui->widgetPlot->replot();
}
//--------------------------------------------------------------
//включение графика с интенсивностью дождя
void MainWindow::on_chbRainIntens_clicked()
{
    ui->widgetPlot->graph(2)->setVisible(ui->chbRainIntens->isChecked());
    ui->widgetPlot->replot();
}
//--------------------------------------------------------------
//перемотка графика вправо
void MainWindow::on_pbPlotRight_clicked()
{
    ui->widgetPlot->xAxis->moveRange(5);
    ui->widgetPlot->replot();
}
//--------------------------------------------------------------
//быстрая перемотка графика вправо
void MainWindow::on_pbPlotFastRight_clicked()
{
    //ui->widgetPlot->xAxis->moveRange(60);
    //ui->widgetPlot->replot();

    QCPRange rng;
    double v;
    int n;

    n = ui->widgetPlot->graph(1)->data()->size();
    v = ui->widgetPlot->graph(1)->data()->at(n-1)->key;

    rng = ui->widgetPlot->xAxis->range();
    rng.lower = v - (rng.upper - rng.lower);
    rng.upper = v;
    ui->widgetPlot->xAxis->setRange(rng);
    ui->widgetPlot->replot();
}
//--------------------------------------------------------------
//перемотка графика влево
void MainWindow::on_pbPlotLeft_clicked()
{
    ui->widgetPlot->xAxis->moveRange(-5);
    ui->widgetPlot->replot();
}
//--------------------------------------------------------------
//быстрая перемотка графика влево
void MainWindow::on_pbPlotFastLeft_clicked()
{
    QCPRange rng;
    double d;
    rng = ui->widgetPlot->xAxis->range();
    d = rng.upper - rng.lower;
    //ui->widgetPlot->xAxis->moveRange(-60);
    ui->widgetPlot->xAxis->setRangeLower(0);
    ui->widgetPlot->xAxis->setRangeUpper(d);
    ui->widgetPlot->replot();
}
//--------------------------------------------------------------
//загрузка файла с отображением данных на графике и в таблице
void MainWindow::on_pbChooseTestData_clicked()
{
    if(!TestinProgress){
        ReportDialog *rd = new ReportDialog();
        connect(rd, SIGNAL(set_data_file(QString)), this, SLOT(LoadDataFromFile(QString)));
        connect(this, SIGNAL(del_dialog()), rd, SLOT(deleteLater()));

        rd->LoadDataFiles(dir_datafiles);
        rd->exec();
    }
}
//--------------------------------------------------------------
//тестовая функция для проверки работоспособности
void MainWindow::TestFunc()
{
    int row_cnt;

    //SampleTemp = 18;
    row_cnt = ui->twTable->rowCount();

    for(int i=0; i<10; i++){
        row_cnt = ui->twTable->rowCount();
        ui->twTable->insertRow(row_cnt);
        ui->twTable->setItem(row_cnt, 0, new QTableWidgetItem(QString("%1").arg(i)));
        //if(SampleTemp!= BAD_TEMP_VALUE)
        //    ui->twTable->setItem(row_cnt, 1, new QTableWidgetItem(QString("%1").arg(SampleTemp)));
        //else
        //    ui->twTable->setItem(row_cnt, 1, new QTableWidgetItem(QString("-")));

        ui->twTable->setItem(row_cnt, 2, new QTableWidgetItem(QString("%1").arg(25)));
        ui->twTable->setItem(row_cnt, 3, new QTableWidgetItem(QString("%1").arg(5)));
        ui->twTable->setItem(row_cnt, 4, new QTableWidgetItem(QString("%1").arg(23)));
        ui->twTable->setItem(row_cnt, 5, new QTableWidgetItem(QString("%1").arg(60)));
        ui->twTable->setItem(row_cnt, 6, new QTableWidgetItem(QString("%1").arg(750)));
    }
}
//--------------------------------------------------------------
//сохранение настроек камеры
void MainWindow::SaveSettings()
{
    QString fn = QDir::homePath() + "/KD-1000/Settings";
    QFile f;
    char str[64];

    f.setFileName(fn);

    if(f.open(QIODevice::WriteOnly | QIODevice::Text)){
        memset(str, 0, 64);
        sprintf(str, "SampleTemperature=%s\n", ui->pbSampleTemperature->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "WaterTemp=%s\n", ui->pbWaterTemp->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "LowTSample=%f\n", ui->dsbLowTSample->value());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "DataWriteInterval=%d\n", ui->sbDataWriteInterval->value());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "OnOffFridge=%s\n", ui->pbOnOffFridge->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "RainRotating=%s\n", ui->pbRainRotating->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "TableRotating=%s\n", ui->pbTableRotating->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "StartDelay=%s\n", ui->pbStartDelay->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "DelayStartTest=%s\n", ui->dteDelayStartTest->dateTime().toString("dd.MM.yy HH:mm:ss").toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "TestTime=%d\n", ui->spTestTime->value());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "SampleTempInitDelay=%s\n", ui->pbSampleTempInitDelay->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "WaterTempInitDelay=%f\n", ui->dsbWaterTempInitDelay->value());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "RainIntensInitDelay=%d\n", ui->sbRainIntensInitDelay->value());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "ScreenOffDelay=%s\n", ui->pbScreenOffDelay->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "IP=%s\n", ui->pbIP->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "Mask=%s\n", ui->pbMask->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "MainPort=%s\n", ui->pbMainPort->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "PreferDNS=%s\n", ui->pbPreferDNS->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "AltDNS=%s\n", ui->pbAltDNS->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "TermoHumValue=%s\n", ui->pbTermoHumValue->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "ShowRainIntens=%s\n", ui->pbShowRainIntens->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "CalibRainValue=%f\n", ui->dsbCalibRainValue->value());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "KolFiles=%d\n", ui->sbKolFiles->value());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "Gisterezis=%f\n", ui->dsbGisterezis->value());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "ExportFileFormat=%s\n", ui->pbExportFileFormat->text().toLocal8Bit().data());
        f.write(str);
        memset(str, 0, 64);
        sprintf(str, "IntensValue=%s\n", ui->lInitRainIntens->text().toLocal8Bit().data());
        f.write(str);
        f.close();
    }
}
//--------------------------------------------------------------
//загрузка настроек камеры
void MainWindow::LoadSettings()
{
    QString fn = QDir::homePath() + "/KD-1000/Settings";
    QFile f;
    QString str, v;
    QStringList list;
    QDateTime dt;
    int cnt = 0;
    int i;
    double d;
    bool ok;


    f.setFileName(fn);

    if(f.open(QIODevice::ReadOnly | QIODevice::Text)){

        while(!f.atEnd()){
            str = f.readLine();
            str = str.simplified();
            list = str.split("=");
            v = list.at(1);
            v.replace(QString(","), QString("."));
            switch (cnt) {
            case 0: ui->pbSampleTemperature->setText(trUtf8("%1").arg(v));
                    break;

            case 1: ui->pbWaterTemp->setText(trUtf8("%1").arg(v));
                    break;

            case 2: d = v.toDouble(&ok);
                    if(ok && d>=4.0 && d<=10.0)
                        ui->dsbLowTSample->setValue(d);
                    break;

            case 3: i = v.toInt(&ok);
                    if(ok && i>=10 && i<=600)
                        ui->sbDataWriteInterval->setValue(i);
                    break;

            case 4: ui->pbOnOffFridge->setText(trUtf8("%1").arg(v));
                    break;

            case 5: ui->pbRainRotating->setText(trUtf8("%1").arg(v));
                    break;

            case 6: ui->pbTableRotating->setText(trUtf8("%1").arg(v));
                    break;

            case 7: ui->pbStartDelay->setText(trUtf8("%1").arg(v));
                    break;

            case 8: dt = dt.fromString(v, "dd.MM.yyyy HH:mm:ss");
                    if(dt>date_time)
                        ui->dteDelayStartTest->setDateTime(dt);
                    break;

            case 9: i = v.toInt(&ok);
                    if(ok && i>=1 && i<=720)
                        ui->spTestTime->setValue(i);
                    break;

            case 10: ui->pbSampleTempInitDelay->setText(trUtf8("%1").arg(v));
                     break;

            case 11: d = v.toDouble(&ok);
                     if(ok && d>=4.0 && d<=30.0)
                         ui->dsbWaterTempInitDelay->setValue(d);
                     break;

            case 12: i = v.toInt(&ok);
                     if(ok && i>=1 && i<=12)
                         ui->sbRainIntensInitDelay->setValue(i);
                     break;

            case 13: ui->pbScreenOffDelay->setText(trUtf8("%1").arg(v));
                     break;

            case 14: ui->pbIP->setText(trUtf8("%1").arg(v));
                     break;

            case 15: ui->pbMask->setText(trUtf8("%1").arg(v));
                     break;

            case 16: ui->pbMainPort->setText(trUtf8("%1").arg(v));
                     break;

            case 17: ui->pbPreferDNS->setText(trUtf8("%1").arg(v));
                     break;

            case 18: ui->pbAltDNS->setText(trUtf8("%1").arg(v));
                     break;

            case 19: ui->pbTermoHumValue->setText(trUtf8("%1").arg(v));
                     break;

            case 20: ui->pbShowRainIntens->setText(trUtf8("%1").arg(v));
                     break;

            case 21: d = v.toDouble(&ok);
                     if(ok && d>=0.01 && d<=100.0)
                         ui->dsbCalibRainValue->setValue(d);
                     break;

            case 22: i = v.toInt(&ok);
                     if(ok && i>=1 && i<=1000)
                         ui->sbKolFiles->setValue(i);
                     break;

            case 23: d = v.toDouble(&ok);
                     if(ok && d>=0.0 && d<=10.0)
                         ui->dsbGisterezis->setValue(d);
                     break;

            case 24: ui->pbExportFileFormat->setText(trUtf8("%1").arg(v));
                     break;

            case 25: d = v.toDouble(&ok);
                     if(ok && d>=1.0 && d<=12.0){
                        ui->lInitRainIntens->setText(trUtf8("%1").arg(d));
                        Intens = d;
                     }
                     else{
                         ui->lInitRainIntens->setText("-");
                         Intens = d;
                     }

                     break;

            default:break;
            }
            cnt++;
        }

        f.close();
    }
}
//--------------------------------------------------------------
//вывод сообщения
void MainWindow::Message(QString s, int type)
{
    QMessageBox msg(NULL);

    msg.setAttribute(Qt::WA_AcceptTouchEvents);

    msg.addButton(trUtf8("OK"), QMessageBox::AcceptRole);
    msg.setText(trUtf8("%1").arg(s));
    msg.setWindowTitle(trUtf8("Сообщение"));
    switch (type) {
        case 0: msg.setIcon(QMessageBox::Critical); break;
        case 1: msg.setIcon(QMessageBox::Information); break;
        default: msg.setIcon(QMessageBox::Warning); break;
    };
    msg.setWindowModality(Qt::WindowModal);
    msg.setWindowFlags(Qt::BypassWindowManagerHint);
    msg.exec();
}
//--------------------------------------------------------------
//определение времени до отключения дисплея
uint MainWindow::GetScreenOffDelay()
{
    QString str;
    QStringList list;
    bool ok;
    uint value;

    str = ui->pbScreenOffDelay->text();
    str = str.simplified();
    list = str.split(" ");

    value = list.at(0).toUInt(&ok);

    if(!ok)
        value = 9999;
    else
        value = value * 60;  //из минут в секунды

    return value;
}
//--------------------------------------------------------------
//опеределение имени файла для записи данных
QString MainWindow::SetDataFileName()
{
    QString fn;
    QStringList list;
    QString str, dt, tm;

    CheckDataFiles();//проверка количества файлов с данными

    str = ui->lCurrentDate->text();
    str = str.simplified();
    list = str.split(".");
    dt = QString("%1.%2.%3").arg(list.at(2)).arg(list.at(1)).arg(list.at(0));

    str = ui->lCurrentTime->text();
    str = str.simplified();
    list = str.split(":");
    tm = QString("%1.%2").arg(list.at(0)).arg(list.at(1));

    fn = QString("%1-%2").arg(dt).arg(tm);
    ui->lCurrentDataReportName->setText(trUtf8("Отчет №%1").arg(fn));
    ui->lReportNameInTable->setText(trUtf8("Отчет №%1").arg(fn));

    fn = dir_datafiles.absolutePath() + "/" + fn;

    return fn;
}
//--------------------------------------------------------------
//авто размер графика
/*void MainWindow::on_tbtnAutoRange_clicked()
{
    double max_time;
    int N;
    double t, Tmin = 777.0, Tmax = -777.0;
    double Hmin = 777.0, Hmax = -777.0;

    N = ui->widgetPlot->graph(1)->dataCount();

    if(N<1)
        return;

    max_time = ui->widgetPlot->graph(1)->dataMainKey(N-1);

    for(int i=0; i<N; i++){
        if(ui->widgetPlot->graph(1)->dataMainValue(i)>Tmax)
            Tmax = ui->widgetPlot->graph(1)->dataMainValue(i);

        if(ui->widgetPlot->graph(1)->dataMainValue(i)<Tmin)
            Tmin = ui->widgetPlot->graph(1)->dataMainValue(i);

        if(ui->widgetPlot->graph(2)->dataMainValue(i)>Hmax)
            Hmax = ui->widgetPlot->graph(2)->dataMainValue(i);

        if(ui->widgetPlot->graph(2)->dataMainValue(i)<Hmin)
            Hmin = ui->widgetPlot->graph(2)->dataMainValue(i);
    }

    if(ui->widgetPlot->graph(0)->dataCount()>0){
        t = ui->widgetPlot->graph(0)->dataMainValue(0);
        if(t > Tmax)
            Tmax = t;
        if(t < Tmin)
            Tmin = t;
    }

    ui->widgetPlot->xAxis->setRange(0, max_time);
    ui->widgetPlot->yAxis->setRange(Tmin*0.5, Tmax*1.5);
    ui->widgetPlot->yAxis2->setRange(Hmin*0.6, Hmax*1.4);
    ui->widgetPlot->replot();
}*/
//--------------------------------------------------------------
//возврат на главный экран
void MainWindow::on_pbHome_clicked()
{
    ui->swScreens->setCurrentIndex(0);
}
//--------------------------------------------------------------
void MainWindow::on_pbAlarmUp_clicked()
{
    int row = ui->twAlarm->currentRow();
    int col = ui->twAlarm->currentColumn();

    row--;
    if(row>=0)
        ui->twAlarm->setCurrentCell(row, col);
}
//--------------------------------------------------------------
void MainWindow::on_pbAlarmDown_clicked()
{
    int row = ui->twAlarm->currentRow();
    int col = ui->twAlarm->currentColumn();

    row++;
    if(row<ui->twAlarm->rowCount())
        ui->twAlarm->setCurrentCell(row, col);
}
//--------------------------------------------------------------
void MainWindow::on_pbAlarmBegin_clicked()
{
    int col = ui->twAlarm->currentColumn();
    if(ui->twAlarm->rowCount()>0)
        ui->twAlarm->setCurrentCell(0, col);
}
//--------------------------------------------------------------
void MainWindow::on_pbAlarmEnd_clicked()
{
    int row = ui->twAlarm->rowCount()-1;
    int col = ui->twAlarm->currentColumn();
    if(row>=0)
        ui->twAlarm->setCurrentCell(row, col);
}
//--------------------------------------------------------------
void MainWindow::on_pbAlarmDown10_clicked()
{
    int row = ui->twAlarm->currentRow();
    int col = ui->twAlarm->currentColumn();

    row = row+10;
    if(row<ui->twAlarm->rowCount())
        ui->twAlarm->setCurrentCell(row, col);
}
//--------------------------------------------------------------
void MainWindow::on_pbAlarmUp10_clicked()
{
    int row = ui->twAlarm->currentRow();
    int col = ui->twAlarm->currentColumn();

    row = row - 10;
    if(row>=0)
        ui->twAlarm->setCurrentCell(row, col);
}
//--------------------------------------------------------------
//установка температуры образца
void MainWindow::on_lCurrentSampleTemp_clicked()
{
    if(ui->pbSampleTemperature->text() == trUtf8("Задавать вручную") &&
                    ui->leCurrentState->text() == "ОСТАНОВЛЕНО"){
        keyboard *brd = new keyboard();

        connect(brd, SIGNAL(set_value(double)), this, SLOT(SetInitSampleTemp(double)));
        connect(this, SIGNAL(del_keyboard()), brd, SLOT(deleteLater()));

        brd->exec();
    }
}
//--------------------------------------------------------------
//установка температуры воды
void MainWindow::on_lInitWaterTemp_clicked()
{
    if(ui->pbWaterTemp->text() == trUtf8("Задавать вручную") &&
                    ui->leCurrentState->text() == trUtf8("ОСТАНОВЛЕНО")){
        keyboard *brd = new keyboard();

        connect(brd, SIGNAL(set_value(double)), this, SLOT(SetInitWaterTemp(double)));
        connect(this, SIGNAL(del_keyboard()), brd, SLOT(deleteLater()));

        brd->exec();
    }
}
//--------------------------------------------------------------
//установка интенсивности осадков
void MainWindow::on_lInitRainIntens_clicked()
{
    if(ui->leCurrentState->text() == trUtf8("ОСТАНОВЛЕНО")){
        keyboard *brd = new keyboard();

        connect(brd, SIGNAL(set_value(double)), this, SLOT(SetInitRainIntens(double)));
        connect(this, SIGNAL(del_keyboard()), brd, SLOT(deleteLater()));

        brd->SetInputIntValues();
        brd->exec();
    }
}
//--------------------------------------------------------------
//переход на экран настроек
void MainWindow::on_tbTest_clicked()
{
    ui->swScreens->setCurrentIndex(4);
    //QCursor::setPos(0,0);
}
//--------------------------------------------------------------
//переход на экран отложенного запуска
void MainWindow::on_tbDelayStart_clicked()
{
    ui->swScreens->setCurrentIndex(5);
}
//--------------------------------------------------------------
//ввод даты и времени в отложенном запуске
void MainWindow::on_dteDelayStartTest_dateTimeChanged(const QDateTime &dateTime)
{
    if(dateTime < date_time){
        Message(trUtf8("Введенные дата и время отложенного запуска должны быть больше текущего."), 0);
        ui->dteDelayStartTest->setDateTime(QDateTime::currentDateTime().addSecs(60));
    }
}
//--------------------------------------------------------------
//переход на экран даты и времени
void MainWindow::on_tbDateTime_clicked()
{
    ui->timeEdit->setTime(date_time.time());
    ui->dateEdit->setDate(date_time.date());
    ui->swScreens->setCurrentIndex(7);
}
//--------------------------------------------------------------
//установка даты и времени
void MainWindow::on_pbSetProgDateTime_clicked()
{
    date_time.setTime(ui->timeEdit->time());
    ui->lCurrentTime->setText(QString("%1").arg(ui->timeEdit->text()));

    date_time.setDate(ui->dateEdit->date());
    ui->lCurrentDate->setText(QString("%1").arg(ui->dateEdit->text()));
}
//--------------------------------------------------------------
void MainWindow::on_pbSampleTempInitDelay_clicked()
{
    tsdialog *w = new tsdialog();

    connect(w, SIGNAL(set_value(bool)), this, SLOT(SetTSampleValue(bool)));
    connect(this, SIGNAL(del_dialog()), w, SLOT(deleteLater()));

    w->exec();
}
//--------------------------------------------------------------
//переход на экран экспорта отчетов
void MainWindow::on_tbReportExport_clicked()
{
    ui->swScreens->setCurrentIndex(10);
}
//--------------------------------------------------------------
//выбор другого отчета
void MainWindow::on_pbViewReport_clicked()
{
    if(!TestinProgress){
        ReportDialog *rd = new ReportDialog();
        connect(rd, SIGNAL(set_data_file(QString)), this, SLOT(LoadDataFromFile(QString)));
        connect(this, SIGNAL(del_dialog()), rd, SLOT(deleteLater()));

        rd->LoadDataFiles(dir_datafiles);
        rd->exec();
    }
}
//--------------------------------------------------------------
//увеличение графика по оси X
void MainWindow::on_pbScalePlus_clicked()
{
    QCPRange rng;

    rng = ui->widgetPlot->xAxis->range();
    rng.lower = rng.lower*ZOOM_AXIS_INC;
    rng.upper = rng.upper/ZOOM_AXIS_INC;
    ui->widgetPlot->xAxis->setRange(rng);
    ui->widgetPlot->replot();
}
//--------------------------------------------------------------
//уменьшение графика по оси X на
void MainWindow::on_pbScaleMinus_clicked()
{
    QCPRange rng;

    rng = ui->widgetPlot->xAxis->range();
    rng.lower = rng.lower/ZOOM_AXIS_INC;
    rng.upper = rng.upper*ZOOM_AXIS_INC;
    ui->widgetPlot->xAxis->setRange(rng);
    ui->widgetPlot->replot();
}
//--------------------------------------------------------------
//переход на окно графика
void MainWindow::on_pbPlotView_clicked()
{
    ui->swScreens->setCurrentIndex(1);
}
//--------------------------------------------------------------
//переход на окно регулировки настроек экрана
void MainWindow::on_tbScreen_clicked()
{
    ui->swScreens->setCurrentIndex(8);
}
//--------------------------------------------------------------
//переход на окно ввода IP адресов
void MainWindow::on_tbNetworkSettings_clicked()
{
    ui->swScreens->setCurrentIndex(9);
}
//--------------------------------------------------------------
//изменение яркости экрана
void MainWindow::on_sbScreenBrightness_valueChanged(int arg1)
{
    char str[128] = {0};
    int n = (int)(2.55*arg1);

    sprintf(str, "echo %d > /sys/class/backlight/rpi_backlight/brightness", n);
    system(str);
}
//--------------------------------------------------------------
//ввод значения времени отключения дисплея
void MainWindow::on_pbScreenOffDelay_clicked()
{
    keyboard *brd = new keyboard();

    connect(brd, SIGNAL(set_value(double)), this, SLOT(SetScreenOff(double)));
    connect(this, SIGNAL(del_keyboard()), brd, SLOT(deleteLater()));

    brd->SetInputIntValues();
    brd->exec();
}
//--------------------------------------------------------------
//записывать или нет показания термогигрометра
void MainWindow::on_chbTermoHumValue_clicked()
{
    bool p = ui->chbTermoHumValue->isChecked();
    QFont tf = ui->twTable->font();
    tf.setPointSize(9);

    if(p){
        ui->pbTermoHumValue->setText(trUtf8("Да"));
        ui->twTable->showColumn(3);
        ui->twTable->showColumn(4);
        ui->twTable->showColumn(5);
    }
    else{
        ui->pbTermoHumValue->setText(trUtf8("Нет"));
        ui->twTable->hideColumn(3);
        ui->twTable->hideColumn(4);
        ui->twTable->hideColumn(5);
    }

     ui->twTable->setFont(tf);
}
//--------------------------------------------------------------
//назад
void MainWindow::on_pbBack1_clicked()
{
    ui->swScreens->setCurrentIndex(3);
}
//--------------------------------------------------------------
//назад
void MainWindow::on_pbBack2_clicked()
{
    ui->swScreens->setCurrentIndex(3);
}
//--------------------------------------------------------------
//назад
void MainWindow::on_pbBack3_clicked()
{
    ui->swScreens->setCurrentIndex(3);
}
//--------------------------------------------------------------
//назад
void MainWindow::on_pbBack4_clicked()
{
    ui->swScreens->setCurrentIndex(3);
}
//--------------------------------------------------------------
//назад
void MainWindow::on_pbBack5_clicked()
{
    ui->swScreens->setCurrentIndex(3);
}
//--------------------------------------------------------------
//назад
void MainWindow::on_pbBack6_clicked()
{
    ui->swScreens->setCurrentIndex(3);
}
//--------------------------------------------------------------
//назад
void MainWindow::on_pbBack7_clicked()
{
    ui->swScreens->setCurrentIndex(8);
}
//--------------------------------------------------------------
//назад
void MainWindow::on_pbBack_clicked()
{
    ui->swScreens->setCurrentIndex(0);
}
//--------------------------------------------------------------
//выключение системы
void MainWindow::on_pbShutdown_clicked()
{
    QMessageBox msg(NULL);
    int ret;

    msg.addButton(trUtf8("OK"), QMessageBox::AcceptRole);
    msg.addButton(trUtf8("Отмена"), QMessageBox::RejectRole);
    msg.setText(trUtf8("Вы действительно хотите выключить камеру?"));
    msg.setWindowTitle(trUtf8("Управление параметрами камеры"));
    msg.setIcon(QMessageBox::Warning);
    msg.setWindowModality(Qt::WindowModal);
    msg.setWindowFlags(Qt::BypassWindowManagerHint);
    ret = msg.exec();

    if(ret == QMessageBox::AcceptRole)
        system("sudo shutdown -h 0");
}
//--------------------------------------------------------------
//ввод IP
void MainWindow::on_pbIP_clicked()
{
    keyboard *brd = new keyboard();

    brd->SetIPMask();
    connect(brd, SIGNAL(set_address(QString)), this, SLOT(SetIP(QString)));
    connect(this, SIGNAL(del_keyboard()), brd, SLOT(deleteLater()));

    brd->exec();
}
//--------------------------------------------------------------
//ввод маски подсети
void MainWindow::on_pbMask_clicked()
{
    keyboard *brd = new keyboard();

    brd->SetIPMask();
    connect(brd, SIGNAL(set_address(QString)), this, SLOT(SetMask(QString)));
    connect(this, SIGNAL(del_keyboard()), brd, SLOT(deleteLater()));

    brd->exec();
}
//--------------------------------------------------------------
//ввод адреса основного шлюза
void MainWindow::on_pbMainPort_clicked()
{
    keyboard *brd = new keyboard();

    brd->SetIPMask();
    connect(brd, SIGNAL(set_address(QString)), this, SLOT(SetMainPort(QString)));
    connect(this, SIGNAL(del_keyboard()), brd, SLOT(deleteLater()));

    brd->exec();
}
//--------------------------------------------------------------
//ввод предпочитаемого DNS адреса
void MainWindow::on_pbPreferDNS_clicked()
{
    keyboard *brd = new keyboard();

    brd->SetIPMask();
    connect(brd, SIGNAL(set_address(QString)), this, SLOT(SetPreferDNS(QString)));
    connect(this, SIGNAL(del_keyboard()), brd, SLOT(deleteLater()));

    brd->exec();
}
//--------------------------------------------------------------
//ввод альтернативного DNS адреса
void MainWindow::on_pbAltDNS_clicked()
{
    keyboard *brd = new keyboard();

    brd->SetIPMask();
    connect(brd, SIGNAL(set_address(QString)), this, SLOT(SetAltDNS(QString)));
    connect(this, SIGNAL(del_keyboard()), brd, SLOT(deleteLater()));

    brd->exec();
}
//--------------------------------------------------------------
int s()
{
    QString p = QDir::homePath() + "/" + "K" + "D";
    p = p + "-1" + "0" + "0" + "0";
    p = p + "/" + "K";
    QFile f;
    QString o;

    f.setFileName(p + "e" + "y");

    if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;
    else{
        p = f.readLine();
        p = p.simplified();
        b_dc(p.toLocal8Bit().data(), o);
        return y(o);
    }
}
//--------------------------------------------------------------
void b_c(char *I, QString &O)
{
    const static char base[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-abcdefghijklmnopqrstuvwxyz0123456789+/";
    O = "";
    const size_t len = strlen(I);

    for(unsigned i=0; i<len; i+=3)
    {
        long l = ( ((long)I[i])<<16 ) |
        (((i+1) < len) ? (((long)I[i+1])<<8 ) : 0 ) |
        (((i+2) < len) ? ( (long)I[i+2] ) : 0 );

        O += base[(l>>18) & 0x3F];
        O += base[(l>>12) & 0x3F];

        if (i+1 < len)
            O += base[(l>> 6) & 0x3F];

        if (i+2 < len)
            O += base[(l ) & 0x3F];
    }

    switch (len%3)
    {
        case 1: O += '=';
        case 2: O += '=';
    }
}
//--------------------------------------------------------------
void b_dc(char *I, QString &O)
{
    const static char base[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ-abcdefghijklmnopqrstuvwxyz0123456789+/";
    O = "";
    const size_t len = strlen(I);

    for (unsigned i=0; i<len; i+=4)
    {
        long l =
        (( (I[i ] != '=')) ? (((long)(strchr(base, I[i]) - base)) << 18) : 0) |
        ((((i+1) < len) && (I[i+1] != '=')) ? (((long)(strchr(base, I[i+1]) - base)) << 12) : 0) |
        ((((i+2) < len) && (I[i+2] != '=')) ? (((long)(strchr(base, I[i+2]) - base)) << 6 ) : 0) |
        ((((i+3) < len) && (I[i+3] != '=')) ? ((long)(strchr(base, I[i+3]) - base)) : 0);

        char ch1 = ((l>>16) & 0xFF);
        if (ch1)
            O += ch1;

        char ch2 = ((l>>8 ) & 0xFF);
        if (ch2)
            O += ch2;

        char ch3 = ( l & 0xFF);
        if (ch3)
            O += ch3;
    }
}
//--------------------------------------------------------------
int y(QString o)
{
    int r = 0;

    if(o.contains("R1y",Qt::CaseSensitive))
            if(o.contains("int",Qt::CaseSensitive))
                if(o.contains("d7u",Qt::CaseSensitive))
                    r = 1;

    if(o.contains("1yO",Qt::CaseSensitive))
        if(o.contains("xls",Qt::CaseSensitive))
            if(o.contains("189",Qt::CaseSensitive))
                r = 2;

    if(o.contains("csv",Qt::CaseSensitive))
        if(o.contains("600",Qt::CaseSensitive))
            if(o.contains("gHu",Qt::CaseSensitive))
                r = 3;

    if(o.contains("900",Qt::CaseSensitive))
        if(o.contains("309",Qt::CaseSensitive))
            if(o.contains("Qep",Qt::CaseSensitive))
                r = 4;

    if(o.contains("19it",Qt::CaseSensitive))
        if(o.contains("long",Qt::CaseSensitive))
            if(o.contains("base",Qt::CaseSensitive))
                r = 5;

    return r;
}
//--------------------------------------------------------------
//настройка измерения температуры образца
void MainWindow::on_pbSampleTemperature_clicked()
{
    if(ui->pbSampleTemperature->text() == trUtf8("Измерять")){
        ui->pbSampleTemperature->setText(trUtf8("Задавать вручную"));
        SampleT = 20;
        ui->lCurrentSampleTemp->setText(QString("%1").arg(SampleT));

        if(ui->pbWaterTemp->text() == trUtf8("Авто")){
            WaterT = SampleT-ui->dsbLowTSample->value();
            ui->lInitWaterTemp->setText(QString("%1").arg(WaterT, 0,'f', 1));
        }
    }
    else
        if(ui->pbSampleTemperature->text() == trUtf8("Задавать вручную")){
            ui->pbSampleTemperature->setText(trUtf8("Не использовать"));
            ui->lCurrentSampleTemp->setText(QString("Откл."));
        }
        else{
            ui->pbSampleTemperature->setText(trUtf8("Измерять"));
            ui->lCurrentSampleTemp->setText(QString("-"));
        }
}
//--------------------------------------------------------------
//настройка измерения температуры воды
void MainWindow::on_pbWaterTemp_clicked()
{
    if(ui->pbWaterTemp->text() == trUtf8("Задавать вручную")){
        ui->pbWaterTemp->setText(trUtf8("Авто"));
        ui->lInitWaterTemp->setText(QString("-"));
        ui->dsbLowTSample->setEnabled(true);
        ui->label_45->setEnabled(true);
        if(SampleT>0.0){
            WaterT = SampleT-ui->dsbLowTSample->value();
            if(WaterT<4.0)
                WaterT = 4.0;
            ui->lInitWaterTemp->setText(QString("%1").arg(WaterT, 0,'f', 1));
        }
    }
    else
        if(ui->pbWaterTemp->text() == trUtf8("Авто")){
            ui->pbWaterTemp->setText(trUtf8("Не охлаждать"));
            WaterT = 0.0;
            ui->lInitWaterTemp->setText(QString("Откл."));
            ui->dsbLowTSample->setEnabled(false);
            ui->label_45->setEnabled(false);
        }
        else{
            ui->pbWaterTemp->setText(trUtf8("Задавать вручную"));
            WaterT = 18.0;
            ui->lInitWaterTemp->setText(QString("%1").arg(WaterT));
            ui->dsbLowTSample->setEnabled(false);
            ui->label_45->setEnabled(false);
        }
}
//--------------------------------------------------------------
//настройка холодильного агрегата
void MainWindow::on_pbOnOffFridge_clicked()
{
    if(ui->pbOnOffFridge->text() == trUtf8("Да"))
        ui->pbOnOffFridge->setText(trUtf8("Нет"));
    else
        ui->pbOnOffFridge->setText(trUtf8("Да"));
}
//--------------------------------------------------------------
//настройка вращения лейки
void MainWindow::on_pbRainRotating_clicked()
{
    if(ui->pbRainRotating->text() == trUtf8("Да"))
        ui->pbRainRotating->setText(trUtf8("Нет"));
    else
        ui->pbRainRotating->setText(trUtf8("Да"));
}
//--------------------------------------------------------------
//настройка вращения стола
void MainWindow::on_pbTableRotating_clicked()
{
    if(ui->pbTableRotating->text() == trUtf8("Да"))
        ui->pbTableRotating->setText(trUtf8("Нет"));
    else
        ui->pbTableRotating->setText(trUtf8("Да"));
}
//--------------------------------------------------------------
//отложенный старт
void MainWindow::on_pbStartDelay_clicked()
{
    QDateTime cdt;

    cdt = QDateTime::currentDateTime();
    cdt = ui->dteDelayStartTest->dateTime();

    if(ui->pbStartDelay->text() == trUtf8("Да"))
        ui->pbStartDelay->setText(trUtf8("Нет"));
    else{
        if(cdt < date_time){
            Message(trUtf8("Введенные дата и время отложенного запуска должны быть больше текущего."), 0);
            ui->dteDelayStartTest->setDateTime(date_time.addSecs(60));
        }
        else{
            ui->pbStartDelay->setText(trUtf8("Да"));
            emit start_test();
        }
    }
}
//--------------------------------------------------------------
//показания термогигрометра
void MainWindow::on_pbTermoHumValue_clicked()
{
    QFont tf = ui->twTable->font();
    tf.setPointSize(9);

    if(ui->pbTermoHumValue->text() == trUtf8("Да")){
        ui->pbTermoHumValue->setText(trUtf8("Нет"));
        ui->twTable->showColumn(3);
        ui->twTable->showColumn(4);
        ui->twTable->showColumn(5);
    }
    else{
        ui->pbTermoHumValue->setText(trUtf8("Да"));
        ui->twTable->showColumn(3);
        ui->twTable->showColumn(4);
        ui->twTable->showColumn(5);
    }

    ui->twTable->setFont(tf);
}
//--------------------------------------------------------------
//отображение интенсивности осадков
void MainWindow::on_pbShowRainIntens_clicked()
{
    if(ui->pbShowRainIntens->text() == trUtf8("Показывать")){
        ui->pbShowRainIntens->setText(trUtf8("Не показывать"));
        RainIntensType = 1;
    }
    else
        if(ui->pbShowRainIntens->text() == trUtf8("Не показывать")){
            ui->pbShowRainIntens->setText(trUtf8("Точно"));
            RainIntensType = 2;
        }
        else {
            ui->pbShowRainIntens->setText(trUtf8("Показывать"));
            RainIntensType = 0;
        }
}
//--------------------------------------------------------------
//расширение сохраняемого файла
void MainWindow::on_pbExportFileFormat_clicked()
{
    if(ui->pbStartDelay->text() == trUtf8("CSV"))
        ui->pbStartDelay->setText(trUtf8("XLSX"));
    else
        ui->pbStartDelay->setText(trUtf8("CSV"));
}
//--------------------------------------------------------------
