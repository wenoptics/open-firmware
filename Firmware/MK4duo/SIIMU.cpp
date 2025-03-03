#include "SIIMU.h"
bool SIIMUClass::init()
{
    //Init filter
    initFilter();
    //Save imu status
    m_imuSTATUS=myIMU.begin();
    //Simulate ok
    m_imuSTATUS=IMU_SUCCESS;

    return (m_imuSTATUS == IMU_SUCCESS);
}

void SIIMUClass::evaluateError()
{
    if(checkVerticality())
    {
        float l_gyroX = 0;
        float l_gyroY = 0;
        float l_gyroZ = 0;

        int l_counter = 0;

        char l_buffer[64];

        uint32_t l_startTime = (uint32_t)millis();

        while(millis() < l_startTime + 2000)
        {
            l_gyroX += myIMU.readFloatGyroX();
            l_gyroY += myIMU.readFloatGyroY();
            l_gyroZ += myIMU.readFloatGyroZ();

            l_counter++;
        }

        m_avgXGyro = l_gyroX/l_counter;
        m_avgYGyro = l_gyroY/l_counter;
        m_avgZGyro = l_gyroZ/l_counter;

        sprintf(l_buffer, "Averages (x, y, z) = %f %f %f", m_avgXGyro, m_avgYGyro, m_avgZGyro);

        Serial.println(l_buffer);
        Serial.flush();
    }
    else
    {
        Serial.println("Evaluate error failed, not vertical.");
    }
}

void SIIMUClass::update()
{
    //Skip if updated too soon
    if (millis() - m_lastCall < SI_IMU_MIN_TIME_BETWEEN_UPDATE_MS)
        return;

    //Save looptime
    int loopTime = (uint32_t)millis() - m_lastCall;
    m_lastCall = millis();

    //Measure values-----------------
    float accX = myIMU.readFloatAccelX();
    float accY = myIMU.readFloatAccelY();
    float accZ = myIMU.readFloatAccelZ();
    float gyroX = myIMU.readFloatGyroX() - m_avgXGyro;
    float gyroY = myIMU.readFloatGyroY() - m_avgYGyro;
    float gyroZ = myIMU.readFloatGyroZ() - m_avgZGyro;

    updateFilter(accX, accY, accZ, gyroX, gyroY, gyroZ, loopTime);

#ifdef SI_PRINT_VALUE_MS
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint > SI_PRINT_VALUE_MS)
    {
        Serial.print(m_roll);
        Serial.print(" ");
        Serial.print(m_pitch);
        Serial.print(" ");
        Serial.print(m_yaw);
        Serial.println();
        lastPrint = millis();
    }
#endif
}

bool SIIMUClass::checkVerticality()
{
    bool l_retVal = false;

    if(SIIMU.getStatus() == IMU_SUCCESS)
    {
      uint8_t l_counter = 0;
      float l_currentMeasure = 0;

      Serial.println("Unit is heating.");
      Serial.flush();

      for(int i = 0; i < 20; i++)
      {
        l_currentMeasure = getZAcc();
        
        Serial.println(l_currentMeasure);
        Serial.flush();

        if(l_currentMeasure > Z_ACC_ABSOLUTE_VALUE_LIMIT)
        {
          l_counter++;
        }
        else if(l_currentMeasure < (Z_ACC_ABSOLUTE_VALUE_LIMIT * -1))
        {
          l_counter++;
        }
      }

      if(l_counter >= 10)
      {
        Serial.println("Vertical check failed!");
        Serial.flush();
      }
      else
      {
        Serial.println("Vertical check ok!");
        Serial.flush();
        l_retVal = true;
      }
    }

    return l_retVal;
}

SIIMUClass SIIMU;
