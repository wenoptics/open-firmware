#include "SIIMU.h"

#define TAU 0.05

/**
 * @brief Update filter value (To be edited according to filter)
 *        N.B. this function must update with current value the variables m_roll, m_pitch e m_yaw
 * 
 * @param accX accelerometer value X
 * @param accY accelerometer value Y
 * @param accZ accelerometer value Z
 * @param gyroX gyro value X
 * @param gyroY gyro value Y
 * @param gyroZ gyro value Z
 * @param loopTime time since last call (ms)
 */
void SIIMUClass::updateFilter(float accX, float accY, float accZ, float gyroX, float gyroY, float gyroZ, uint16_t loopTime)
{
    float loopTimeS=(float)loopTime / 1000.0;
    imu.updateIMU(gyroX, gyroY, gyroZ, accX, accY, accZ, loopTimeS);

    /*m_roll = imu.getRoll();
    m_pitch = imu.getPitch();
    m_yaw = imu.getYaw();*/

    m_roll = m_lpRoll.addValue(imu.getRoll(),loopTimeS);
    m_pitch = m_lpPitch.addValue(imu.getPitch(),loopTimeS);
    m_yaw = m_lpYaw.addValue(imu.getYaw(),loopTimeS);
}

void SIIMUClass::initFilter()
{
    //Set frequency to maximum possible
    imu.begin((float)1000.0 / SI_IMU_MIN_TIME_BETWEEN_UPDATE_MS);
    m_lpRoll.init(TAU);
    m_lpPitch.init(TAU);
    m_lpYaw.init(TAU);
}