#include "SITestBanco.h"


#define RPM 120
#define DELTA 20    // differenza minima per la temperatura
#define TIMEON 2800 // tempo di ON del riscaldatore
#define MOTOR_STEPS 200
#define MICROSTEPS 1
#define ENABLE EN_XY
#define ENABLE_2 EN_Z
#define HALL A4
#define HALL_ANALOG 850

#define RED1 (2)
#define GREEN1 (4)
#define BLUE1 (35)

#define RED2 (0)
#define GREEN2 (1)
#define BLUE2 (3)

#define RED3 (36)
#define GREEN3 (37)
#define BLUE3 (34)

#define MS3 (33)
#define MS2 (9)
#define MS1 (10)


#define IMU_SCL (24) //PA13
#define IMU_SDA (23) //PA12
#define IMU_INT (25) //PA11
#define EN_XY (13)   //PA10


//PB11 non popolato
#define HEATER (22) //PB10
#define RST_Z (16)  //PA27
#define RST_XY (20) //PB09
#define EN_Z (19)   //PB08


#define XMAX (29)   //PA05
#define ZMAX (28)   //PA04
#define YMAX (27)   //PA03
#define THERMO (26) //PA02



#define SW


void SITestBanco::initTest()
{

    //MOTOR
    stepperY.begin(RPM, MICROSTEPS);
    stepperX.begin(RPM, MICROSTEPS);
    stepperZ.begin(RPM, MICROSTEPS);
    pinMode(RST_Z, OUTPUT);
    pinMode(RST_XY, OUTPUT);
    digitalWrite(RST_Z, LOW);
    digitalWrite(RST_XY, LOW);
    pinMode(HEATER, OUTPUT);
    //digitalWrite(HEATER, onoff);
    analogWrite(HEATER, 0);
}

void SITestBanco::runTest()
{
    for (;;)
    {
        while (analogRead(A4) >= HALL_ANALOG) // attendo che la calamita si allontani
        {
            delay(200);
            Serial.println("--->[][]");
        }

        while (analogRead(A4) <= HALL_ANALOG) // attendo che la calamita si avvicini ( fa partire il test )
        {
            delay(200);
            Serial.println("--->[]    []");
        }

        pinMode(RST_Z, INPUT);
        pinMode(RST_XY, INPUT);
        //------------test riscaldatore--------------------------
        float Measure_1 = 0;
        float Measure_2 = 0;
        myIMU.begin();
        for (int a = 0; a < 5; a++)
        {
            Measure_1 = Measure_1 + analogRead(THERMO);
        }
        analogWrite(HEATER, heater_val);
        delay(TIMEON);
        for (int b = 0; b < 5; b++)
        {
            Measure_2 = Measure_2 + analogRead(THERMO);
        }
        analogWrite(HEATER, 0);
        float average_M1 = Measure_1 / 5;
        float average_M2 = Measure_2 / 5;
        //------------test IMU e Themo--------------------------
        float imuTemp = float(myIMU.readTemperatureC());

        if ((average_M1 > average_M2) && (imuTemp >= 15 && imuTemp <= 45))
        {
            // test ok imu e riscaldatore...  muovo i motori
            for (;;)
            {
                // test ok
                Serial.println("OK");
                stepperY.rotate(360);
                stepperX.rotate(360);
                digitalWrite(RST_Z, HIGH);
                stepperZ.rotate(360);
                digitalWrite(RST_Z, LOW);
                delay(1300);
                Serial.println("OK");
                delay(1300);
                stepperY.rotate(-360);
                stepperX.rotate(-360);
                digitalWrite(RST_Z, HIGH);
                stepperZ.rotate(-360);
                digitalWrite(RST_Z, LOW);
                Serial.println("OK");
                delay(1300);
            }
        }
        else
        {
            // tets fail invio fail
            for (;;)
            {
                if (!(imuTemp >= 15 && imuTemp <= 45))
                {
                    // errore imu
                    Serial.println("imu");
                    delay(100);
                }
                else
                {
                    Serial.println("hotp");
                    delay(100);
                }
            }
        }
    }
}

void SITestBanco::run()
{
        initTest();
        runTest();
}
