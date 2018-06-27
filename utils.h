#include <QWidget>
#include <QMutex>
#include "qcustomplot.h"
#if TARGET_OS==RASPBIAN_OS
    #include "wiringPi.h"
    #include "wiringPiSPI.h"
#endif
#include "errno.h"

#define SPI_DELAY_MS    1

#define	SPI_CHAN		0           //channel num
#define SPI_SPEED       5000      //20000 in Hz
#define SPI_PACK_SIZE   10          //packet size in bytes
#define MAX_TEST_TIME   144400      //minutes
#define VALUE_DELITEL   100.0       //делитель для полученных значений через SPI
#define ZOOM_AXIS_INC   1.2

#define TIME_FOR_T_MEAS         30      //длительность измерения температуры образца in secs
#define MAX_ROWS_ALARM_TABLE    100     //rows in alarm table

#define SPI_READ_VALUE  0x00
#define SPI_WRITE_VALUE  0x10

#define READ_DATA       0
#define WRITE_DATA      1

#define TIMER_TIMEOUT       1       //timeout in secs
#define TIMER_READ_DATA     1
#define TIME_FOR_WAITING    1800 //in sec
#define T_WATER_ERR_BAND    0.2     //разница заданной и измеренной температур, когда температура считается установленной

#define LIGHT_OFF_DELAY 600     // in sec

#define TO_CH               3142

//addresses
#define STATUS                  0x0000
#define START_STOP              0x0001  //0-СТОП или 1-ПУСК
#define SET_T_WATER             0x0004  //установка температуры воды
#define SENSOR_T_WATER          0x0005  //Датчик температуры воды
#define SENSOR_T_SAMPLE         0x0006  //Датчик температуры образца
#define SENSOR_T_OUT            0x0007  //Датчик температуры Окр среды
#define SENSOR_PRESSURE_OUT     0x0008  //Датчик давления Окр среды
#define SENSOR_HUMIDITY_OUT     0x0009  //Датчик влажности Окр среды
#define SENSOR_PRESSURE_RAMP    0x0010  //Датчик давления в водяной рампе
#define SENSOR_P_AFTER_PUMP     0x0011  //Давление после насоса, кПа
#define SENSOR_FLOWMETER        0x0013  //расходомер (л/мин)
#define SET_FLOWMETER           0x0014  //(л/мин)
#define TEST_ADRESS             0x0015  //Всегда возвращает 0xFF00FF00
#define T_WATER_REACHED         0x0016

#define START_BUTTON            0x0023  //1 была нажата 0 не была нажата
#define STOP_BUTTON             0x0024  //1 была нажата 0 не была нажата
#define ALARM_BUTTON            0x0025  //1 была нажата 0 не была нажата
#define JANITOR_BUTTON          0x0026  //1 была нажата 0 не была нажата

#define SENSOR_DOOR_OPEN        0x0027  //датчик открытия двери, 1 сработал 0 не сработал
#define SENSOR_LOW_WATER_LEVEL  0x0028  //Низкий уровень воды сработал
#define SENSOR_MED_WATER_LEVEL  0x0029  //Средний уровень воды сработал
#define SENSOR_HIGH_WATER_LEVEL 0x0030  //Высокий уровень воды сработал

#define LIGHT                   0x0034  //Освещение
#define RAIN_DRIVE              0x0035  //Двигатель лейки, 0-выкл, 1-равномерный, 2-шаговый
#define RAIN_DRIVE_TURN         0x0036  //Направления вращения двигателя лейки, 0-по часовой, 1-против часовой
#define COMPRESSOR              0x0037  //Холодильный агрегат 0-выкл, 1-вкл
#define WATER_VALVE             0x0038  //Клапан набора воды
#define SIREN                   0x0039  //Сирена
#define TABLE_DRIVE             0x0040  //Двигатель столика
#define FAN                     0x0042  //Вентилятор нагнетающий

//values
#define RAIN_TYPE_OFF       0
#define RAIN_TYPE_CONST     1
#define RAIN_TYPE_STEP      2


//status error
#define SUCCESS_OPERATION           0x00000000  //успешная операция
#define STATUS_ALARM_BUTTON         0x00000001  //аварийная кнопка нажата
#define STATUS_NO_WATER             0x00000002  //нет воды
#define STATUS_PUMP_OVERHEATED      0x00000004  //Перегрев насоса
#define STATUS_T_WATER_NOTREACHED   0x00000008  //температуры воды пока не достигнута
#define STATUS_JANITOR_BLOCKED      0x00000010  //Дворник заклинило
#define STATUS_DOOR_OPENED          0x00000020  //дверь открыта
#define STATUS_SPI_ERR              0x00000040  //ошибка SPI
#define STATUS_ERR                  0x10000000  //ошибка


#define RPI_PIN    10

class OvenThread: public QObject
{
    Q_OBJECT
public:
    OvenThread (QWidget *parent=0);
    ~OvenThread();

    uint TestDuration;          //длительность испытания in secs
    double WaterT;              //температура воды в начале испытаний
    short RainIntens;           //интенсивоность дождя (л/мин)
    short WriteDataStep;        //шаг записи данных в файл
    short RainDrive;            //тип движения лейки
    bool  RainMovingType;       //напрвление вращения лейки, true-по часовой, false - против часовой
    bool CoolingStop;
    bool TableDrive;            //тип движения столика, true-вкл, false-выкл

    QTimer *pTestTimer;         //таймер отчета длительности испытаний
    bool    NeedMeasTSample,    //необходимость измерения температуры образца
            NeedSetTWater,      //необходимость установки температуры воды
            bStartDelay;        //необходимость отложенного запуска

    double  IntensCoeff,         //множитель интенсивности осадков
            GisterezisT;         //гистерезис в режиме термостата

    uint delay;
    bool bStopped;
    bool bGetMeas;
private:
    uint cnt;           //счетчик секунд при тестировании

public slots:
    void WaitingTestStart();
    void ReadSPI(uint address, uint &value, uint &status);
    void WriteSPI(uint address, uint value, uint &status);
    void StopTest(uint status);
    void ReadSampleT();
    void SetWaterTemp();
    void WaitCoolingWater();
    void StartTest();
    void ChangeTestTime();
    void ReadData();
    void StoppedTest();
    void GetMeas(bool bOk);
signals:
    void finished(); //окончание испытаний
    void finished_SampleT_Meas();   //окончание измерения температуры образца
    void change_value_sampleT(uint, double);    //изменение температуры образца на экране
    void change_value_waterT(double);   //изменение температуры воды на экране
    void check_error(uint);         //проверка ошибки
    void water_level_waiting();     //ожидание нужного уровня воды
    void water_level_ok();          //нужный уровень достигнут
    void start_testing();       //начало тестирования
    void change_test_time(uint); // изменение длительности испытаний на главном экране
    void data_readed(uint, double, double); //отображение измеренных данных на экране и графике
    void waiting(uint cnt); //ожидание перед запуском
    void end_of_waiting();  //окончание ожидания начала испытаний
    void change_color_to_green();
    void stop_test(uint);       //остановка испытаний
    void start_meas();
};

bool SetSPIParam(uint address, uint value);
bool GetSPIParam(uint address, uint &value);
