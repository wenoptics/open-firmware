#pragma once
#include "BasicStepperDriver.h"
#include "SIIMU.h"
#define MOTOR_STEPS 200
#define DIR_X (11)   //PA23
#define STEP_X (12)  //PA14
#define DIR_Z (14)   //PA09
#define STEP_Z (15)  //PA08
#define STEP_Y (18) //PB03
#define DIR_Y (17)  //PB02
#define TESTMODE 90
#define X_MAX_TEST A3

#define mySDA 23
#define mySCL 24

class SITestBanco
{
    void initTest();
    void runTest();

    // 2-wire basic config, microstepping is hardwired on the driver
    BasicStepperDriver stepperY;
    BasicStepperDriver stepperX;
    BasicStepperDriver stepperZ;

    TwoWire myWire;
    // Using I2C mode by default.
    LSM6DSL myIMU;

    const uint8_t heater_val = 215; // pwm riscaldatore

public:
    void run();

    SITestBanco() :
    stepperY(MOTOR_STEPS, DIR_Y, STEP_Y), stepperX(MOTOR_STEPS, DIR_X, STEP_X), stepperZ(MOTOR_STEPS, DIR_Z, STEP_Z),
    myWire(&sercom2, mySDA, mySCL), myIMU(&myWire, mySDA, mySCL, LSM6DSL_MODE_I2C, 0x6B) {}
};
