#include "utils.h"
#define COUNT_KEY 0

//--------------------------------------------------------------
OvenThread::OvenThread(QWidget *)
{
    pTestTimer = new QTimer(this);
    pTestTimer->setInterval(1000);

    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    connect(this, SIGNAL(end_of_waiting()), this, SLOT(ReadSampleT()));
    connect(this, SIGNAL(finished_SampleT_Meas()), this, SLOT(SetWaterTemp()));
    connect(this, SIGNAL(start_testing()), this, SLOT(StartTest()));
    connect(pTestTimer, SIGNAL(timeout()), this, SLOT(ChangeTestTime()));

    NeedMeasTSample = false;
    NeedSetTWater = false;

    delay = 0;
    TestDuration = 0;
    RainIntens = 1;
    IntensCoeff = 1.0;
    GisterezisT = 1.0;
    WriteDataStep = 10;
    RainDrive = 0;
    RainMovingType = true;
    CoolingStop = true;
    TableDrive = false;
    bStartDelay = false;

    bStopped = false;
    bGetMeas = false;
}
//--------------------------------------------------------------
OvenThread::~OvenThread()
{
    delete pTestTimer;
}
//--------------------------------------------------------------
//окончание испытания
void OvenThread::StopTest(uint status)
{
    pTestTimer->stop();

    emit finished();

    if(status != SUCCESS_OPERATION)
        emit check_error(status);

    emit stop_test(status);
}
//--------------------------------------------------------------
//запуск измерения температуры образца
void OvenThread::ReadSampleT()
{
    double T = 0.0;
    uint value, status;
    cnt = 0;

    if(NeedMeasTSample){
        while(cnt<=TIME_FOR_T_MEAS){
                ReadSPI(SENSOR_T_SAMPLE, value, status);
                if(status == SUCCESS_OPERATION || status == STATUS_DOOR_OPENED){
                    T = value/VALUE_DELITEL;
                    emit change_value_sampleT(TIME_FOR_T_MEAS-cnt, T);//изменение на экране
                }
                else{
                    StopTest(status);
                    return;
                }

                if(bStopped){
                   StopTest(SUCCESS_OPERATION);
                   return;
                }


                QApplication::processEvents();
                cnt++;
                QThread::sleep(1);
        }//end while
    }

    emit finished_SampleT_Meas();
}
//--------------------------------------------------------------
//запуск установки температуры воды
void OvenThread::SetWaterTemp()
{
    uint value, status;
    bool WaterLevel = false;

    emit water_level_waiting();
    do{
            ReadSPI(SENSOR_HIGH_WATER_LEVEL, value, status);

#if TARGET_OS!=RASPBIAN_OS
    value = 1;
#endif

            if(value == 1)
                WaterLevel = true;


            if(cnt>TIME_FOR_WAITING){    //ограничение по времени
                StopTest(STATUS_NO_WATER);
                return;
            }

            if(bStopped){
               StopTest(SUCCESS_OPERATION);
               return;
            }

            QApplication::processEvents();
            QThread::sleep(1);
            cnt++;
    }
    while(!WaterLevel);//ограничение по времени

    if(WaterT>4.0){

        cnt = 0;
        if(NeedSetTWater){
            emit water_level_ok();
            WriteSPI(SET_T_WATER, WaterT, status);
            if(status != SUCCESS_OPERATION &&
                    status != STATUS_DOOR_OPENED){
                StopTest(status);
                return;
            }
            else
                WaitCoolingWater();
        }
        else
            emit start_testing();
    }
    else
      emit start_testing();
}
//--------------------------------------------------------------
//считывание температуры воды
void OvenThread::WaitCoolingWater()
{
    uint value, status;
    double T;

    cnt = 0;

    ReadSPI(SENSOR_T_WATER, value, status);
    if(status == SUCCESS_OPERATION){
        T = value/VALUE_DELITEL;
        emit change_value_waterT(T);
    }

    if(T>WaterT){
        WriteSPI(COMPRESSOR, 1, status);
        if(status != SUCCESS_OPERATION &&
                status != STATUS_DOOR_OPENED){
            StopTest(status);
            return;
        }


        while(T>WaterT)
        {
                ReadSPI(SENSOR_T_WATER, value, status);
                if(status == SUCCESS_OPERATION){
                    T = value/VALUE_DELITEL;
                    emit change_value_waterT(T);
                }
                else
                    if(STATUS_DOOR_OPENED!=status)
                        StopTest(status);

                if(cnt>TIME_FOR_WAITING){
                    StopTest(STATUS_T_WATER_NOTREACHED);
                    return;
                }

                if(bStopped){
                   StopTest(SUCCESS_OPERATION);
                   return;
                }

                QApplication::processEvents();
                cnt++;
                QThread::sleep(1);
        //}while(fabs(T - WaterT)>=T_WATER_ERR_BAND);
        };


        if(CoolingStop){
            WriteSPI(COMPRESSOR, 0, status);
            if(status != SUCCESS_OPERATION &&
                    status != STATUS_DOOR_OPENED){
                StopTest(status);
                return;
            }
        }
    }//end if(T>WaterT)

    emit start_testing();
}
//--------------------------------------------------------------
//начало непрерывного считывания данных с датчиков
void OvenThread::StartTest()
{
    uint status;

    //нажатие старт/стоп
    WriteSPI(START_STOP, 1, status);
    if(status != SUCCESS_OPERATION &&
            status != STATUS_DOOR_OPENED){
        StopTest(status+STATUS_ERR);
        return;
    }

    //включение двигателя лейки
    WriteSPI(RAIN_DRIVE, RainDrive, status);
    if(status != SUCCESS_OPERATION &&
            status != STATUS_DOOR_OPENED){
        StopTest(status);
        return;
    }

    //установка направления вращения двигателя лейки
    if(RainDrive>0){
        if(RainMovingType==true)
            WriteSPI(RAIN_DRIVE_TURN, 0, status);
        else
            WriteSPI(RAIN_DRIVE_TURN, 1, status);
        if(status != SUCCESS_OPERATION &&
                status != STATUS_DOOR_OPENED){
            StopTest(status);
            return;
        }
    }

    //вращение столика
    if(TableDrive==true)
        WriteSPI(TABLE_DRIVE, 1, status);
    else
        WriteSPI(TABLE_DRIVE, 0, status);
    if(status != SUCCESS_OPERATION &&
            status != STATUS_DOOR_OPENED){
        StopTest(status);
        return;
    }

    //установка расходомера
    WriteSPI(SET_FLOWMETER, RainIntens*VALUE_DELITEL, status);
    if(status != SUCCESS_OPERATION &&
            status != STATUS_DOOR_OPENED){
        StopTest(status);
        return;
    }

    //запуск испытаний
    WriteSPI(START_STOP, 1, status);
    if(status != SUCCESS_OPERATION){
        StopTest(status);
        return;
    } else
        emit change_color_to_green();

    cnt = 0;
    emit start_meas();
    pTestTimer->start();
}
//--------------------------------------------------------------
//изменение времени тестирования
//отслеживание выключения лампы
//остановка испытания
void OvenThread::ChangeTestTime()
{
    uint value;

    GetSPIParam(STATUS, value);
    if(STATUS_DOOR_OPENED == value)
        return;


    GetSPIParam(STATUS, value);
    if(STATUS_DOOR_OPENED == value)
        return;


    emit change_test_time(cnt);

    if(cnt>=TestDuration){
        StopTest(SUCCESS_OPERATION);
        return;
    }

    if(cnt==LIGHT_OFF_DELAY){
        WriteSPI(LIGHT, 0, value);
        if(value != SUCCESS_OPERATION){
            StopTest(STATUS_SPI_ERR);
            return;
        }
    }

    if(bStopped){
       StopTest(SUCCESS_OPERATION);
       return;
    }

    if(cnt%WriteDataStep == 0 && bGetMeas)
        ReadData();

    cnt++;
}
//--------------------------------------------------------------
//считывание данных с датчиков
void OvenThread::ReadData()
{
    uint value, status;

    double  WaterTemp,
            RainIntens;

    if(NeedSetTWater){
        //считывание температуры воды
        ReadSPI(SENSOR_T_WATER, value, status);
        if(status != SUCCESS_OPERATION){
            StopTest(status);
            return;
        }
        else{
            WaterTemp = value/VALUE_DELITEL;
            if(!CoolingStop){
                if(WaterTemp > WaterT)
                    WriteSPI(COMPRESSOR, 1, status);
                else
                    WriteSPI(COMPRESSOR, 0, status);

                if(status != SUCCESS_OPERATION){
                    StopTest(status);
                    return;
                }
            }
        }
    }//end if(NeedSetTWater)
    else
        WaterTemp = 0.0;

    //считывание интенсивности осадков
    ReadSPI(SENSOR_FLOWMETER, value, status);
    if(status != SUCCESS_OPERATION){
        StopTest(status);
        return;
    }
    else
        RainIntens = value/VALUE_DELITEL * IntensCoeff;

    //отображение параметров в окне
    emit data_readed(cnt, WaterTemp, RainIntens);
}
//--------------------------------------------------------------
void OvenThread::StoppedTest()
{
    bStopped = true;
}
//--------------------------------------------------------------
void OvenThread::GetMeas(bool bOk)
{
    bGetMeas = bOk;
}
//--------------------------------------------------------------
//ожидание начала испытаний
void OvenThread::WaitingTestStart()
{
    cnt = 0;

    if(bStartDelay && delay>0)
        while(delay>cnt){
            if(bStopped){
               StopTest(SUCCESS_OPERATION);
               return;
            }

            emit waiting(delay-cnt);//изменение на экране
            QApplication::processEvents();
            QThread::sleep(1);
            cnt++;
        }

    emit end_of_waiting();
}
//--------------------------------------------------------------
//функция чтения SPI
void OvenThread::ReadSPI(uint address, uint &value, uint &status)
{
    uint st;

    status = SUCCESS_OPERATION;

    if(!GetSPIParam(address, value))
        status = STATUS_SPI_ERR;
    else{
        if(!GetSPIParam(STATUS, st))
            status = STATUS_SPI_ERR;
        else
            status = st;
    }
}
//--------------------------------------------------------------
//функция записи SPI
void OvenThread::WriteSPI(uint address, uint value, uint &status)
{
    uint st;

    status = SUCCESS_OPERATION;

    if(!SetSPIParam(address, value))
        status = STATUS_SPI_ERR;
    else{
        if(!GetSPIParam(STATUS, st))
            status = STATUS_SPI_ERR;
        else
            status = st;
    }
}
//--------------------------------------------------------------
//функция установки значения через SPI
bool SetSPIParam(uint address, uint value)
{
    uchar myData[SPI_PACK_SIZE] = {0};
    bool ok = false;

    myData[0] = 0xAF;                       //стартовый байт
    myData[1] = WRITE_DATA;                 //запись
    myData[3] = address&0xFF;               //старший байт
    myData[2] = (address>>CHAR_BIT)&0xFF;   //младший байт
    myData[4] = 0xFF;                       //промежуточный байт

    for(int i=0; i<4; i++)
        myData[8-i] = (value>>(CHAR_BIT*i))&0xFF;  //передаваемое значение в int

    myData[9] = 0xFA;       //конец пакета

#if TARGET_OS==RASPBIAN_OS
    piLock(COUNT_KEY);
#endif

    QThread::msleep(10);

#if TARGET_OS==RASPBIAN_OS
        digitalWrite(RPI_PIN, LOW);
        delay(SPI_DELAY_MS);
            if(wiringPiSPIDataRW (SPI_CHAN, myData, SPI_PACK_SIZE) == -1)
                ok = false;

        piUnlock(COUNT_KEY);
        delay(10);
#endif

    if(myData[9]==0x01)
        ok = true;
    else
        ok = false;

#if TARGET_OS!=RASPBIAN_OS
    ok = true;
#endif

    return ok;
}
//--------------------------------------------------------------
//функция чтения данных через SPI
bool GetSPIParam(uint address, uint &value)
{
    uchar myData[SPI_PACK_SIZE] = {0};
    bool ok = false;

    value = 0;

    myData[0] = 0xAF;                       //стартовый байт
    myData[1] = READ_DATA;                  //запись
    myData[3] = address&0xFF;               //старший байт
    myData[2] = (address>>CHAR_BIT)&0xFF;   //младший байт
    myData[4] = 0xFF;                       //промежуточный байт

    for(int i=0; i<4; i++)
        myData[8-i] = 0;

    myData[9] = 0xFA;       //конец пакета

#if TARGET_OS==RASPBIAN_OS
    piLock(COUNT_KEY);
#endif

    QThread::msleep(10);

#if TARGET_OS==RASPBIAN_OS
        digitalWrite(RPI_PIN, LOW);
        delay(SPI_DELAY_MS);
            if(wiringPiSPIDataRW (SPI_CHAN, myData, SPI_PACK_SIZE) == -1)
                ok = false;

        piUnlock(COUNT_KEY);
        delay(10);
#endif

    if(myData[9] == 0x01){
        value = (myData[5]<<24) + (myData[6]<<16) + (myData[7]<<8)+ myData[8];
        ok = true;
    }
    else
        ok = false;

#if TARGET_OS!=RASPBIAN_OS
    ok = true;
#endif

    return ok;
}
//--------------------------------------------------------------
