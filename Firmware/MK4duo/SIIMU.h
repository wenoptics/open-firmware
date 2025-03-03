#pragma once
#include "LSM6DSL.h"
#include "Wire.h"
#include "MadgwickAHRS.h"
#include "LowPassExp.h"

#define mySDA 23
#define mySCL 24

#define FIL_L 100

#define Z_ACC_ABSOLUTE_VALUE_LIMIT 0.50f

//Minimum time between filter update (Sensors works at 833Hz)
#define SI_IMU_MIN_TIME_BETWEEN_UPDATE_MS 2

//If defined prints output to serial with given timeout
//#define SI_PRINT_VALUE_MS 100
class SIIMUClass
{
    //HW handler
    TwoWire myWire;
    LSM6DSL myIMU;

    //output variables
    float m_roll, m_pitch, m_yaw;
    float m_avgXGyro = 0;
    float m_avgYGyro = 0;
    float m_avgZGyro = 0;

    void updateFilter(float accX, float accY, float accZ, float gyroX, float gyroY, float gyroZ, uint16_t loopTime);
    void initFilter();
    uint32_t m_lastCall = 0;
    lsm6dsl_status_t m_imuSTATUS;
    //Filter variables (To be edited according to filter)---------------------------
    Madgwick imu;
    LowPassExp<float> m_lpRoll, m_lpPitch, m_lpYaw;
    //-------------------------------------------------------------------------------

public:
    /**
    * @brief Update IMU
    * 
    */
    void update();
    /**
     * @brief Init IMU
     * 
     * @return true IMU inited
     * @return false Error
     */
    bool init();

    inline float getXAcc(){return myIMU.readFloatAccelX();}
    inline float getYAcc(){return myIMU.readFloatAccelY();}
    inline float getZAcc(){return myIMU.readFloatAccelZ();}
    
    inline float getXGyroscope(){return myIMU.readFloatGyroX() - m_avgXGyro;}
    inline float getYGyroscope(){return myIMU.readFloatGyroY() - m_avgYGyro;}
    inline float getZGyroscope(){return myIMU.readFloatGyroZ() - m_avgZGyro;}

    inline float getXGyroAvg(){return m_avgXGyro;}
    inline float getYGyroAvg(){return m_avgYGyro;}
    inline float getZGyroAvg(){return m_avgZGyro;}

    void evaluateError();
    
    float getRoll(){return m_roll;}
    float getPitch(){return m_pitch;}
    float getYaw(){return m_yaw;}

    bool checkVerticality();

    lsm6dsl_status_t getStatus(){return m_imuSTATUS;}

    SIIMUClass() : myWire(&sercom2, mySDA, mySCL), myIMU(&myWire, mySDA, mySCL, LSM6DSL_MODE_I2C, 0x6B) {}
};

extern SIIMUClass SIIMU;