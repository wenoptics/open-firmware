#include "LSM6DSL.h"


LSM6DSLCore::LSM6DSLCore(TwoWire *i2cPeri, uint8_t pSDA, uint8_t pSCL, uint8_t addr): opMode(LSM6DSL_MODE_I2C), i2cAddress(addr) {
    _i2c = i2cPeri;
    _pinSCL = pSCL;
    _pinSDA = pSDA;
}

LSM6DSLCore::LSM6DSLCore(TwoWire *i2cPeri, uint8_t pSDA, uint8_t pSCL, lsm6dsl_mode_t operationMode, uint8_t arg) {
    this->opMode = operationMode;
    if (operationMode == LSM6DSL_MODE_I2C) {
        this->i2cAddress = arg;
        _i2c = i2cPeri;
        _pinSCL = pSCL;
        _pinSDA = pSDA;
    } else if (operationMode == LSM6DSL_MODE_SPI) {
        this->slaveSelect = arg;
    }
}

lsm6dsl_status_t LSM6DSLCore::beginCore() {
    lsm6dsl_status_t returnStatus = IMU_SUCCESS;

    if (opMode == LSM6DSL_MODE_I2C) {
        _i2c->begin();
        pinPeripheral(_pinSDA, PIO_SERCOM);
		pinPeripheral(_pinSCL, PIO_SERCOM);
    } else if (opMode == LSM6DSL_MODE_SPI) {
        SPI.begin();
        SPI.setClockDivider(SPI_CLOCK_DIV4);
        pinMode(slaveSelect, OUTPUT);
        digitalWrite(slaveSelect, HIGH);
    }

    volatile uint8_t temp = 0;
    for (uint16_t i = 0; i < 10000; i++) {
        temp++;
    }

    uint8_t result;
    readRegister(&result, LSM6DSL_ACC_GYRO_WHO_AM_I_REG);

    if (result != LSM6DSL_ACC_GYRO_WHO_AM_I) {
        returnStatus = IMU_HW_ERROR;
    }

    return returnStatus;
}

lsm6dsl_status_t LSM6DSLCore::readRegister(uint8_t* output, uint8_t offset) {
    uint8_t result = 0;
    uint8_t numBytes = 1;
    lsm6dsl_status_t returnStatus = IMU_SUCCESS;

    if (opMode == LSM6DSL_MODE_I2C) {
        _i2c->beginTransmission(i2cAddress);
        _i2c->write(offset);

        if (_i2c->endTransmission() != 0) {
            returnStatus = IMU_HW_ERROR;
        }

        _i2c->requestFrom(i2cAddress, numBytes);
        while (_i2c->available()) {
            result = _i2c->read();
        }
    } else if (opMode == LSM6DSL_MODE_SPI) {
        digitalWrite(slaveSelect, LOW);
        SPI.transfer(offset | 0x80);

        result = SPI.transfer(0x00);

        digitalWrite(slaveSelect, HIGH);

        if (result == 0xFF) {
            returnStatus = IMU_ALL_ONES_WARNING;
        }
    }

    *output = result;
    return returnStatus;
}

lsm6dsl_status_t LSM6DSLCore::readRegisterRegion(uint8_t* output, uint8_t offset, uint8_t length) {
    lsm6dsl_status_t returnStatus = IMU_SUCCESS;
    uint8_t i = 0;
    uint8_t c = 0;
    uint8_t allOnesCounter = 0;

    if (opMode == LSM6DSL_MODE_I2C) {
        _i2c->beginTransmission(i2cAddress);
        _i2c->write(offset);
        if (_i2c->endTransmission() != 0) {
            returnStatus = IMU_HW_ERROR;
        } else {
            _i2c->requestFrom(i2cAddress, length);
            while (_i2c->available() && (i < length)) {
                c = _i2c->read();
                *output = c;
                output++;
                i++;
            }
        }
    } else if (opMode == LSM6DSL_MODE_SPI) {
        digitalWrite(slaveSelect, LOW);

        SPI.transfer(offset | 0x80);
        while (i < length) {
            c = SPI.transfer(0x00);
            if (c == 0xFF) {
                allOnesCounter++;
            }
            *output = c;
            output++;
            i++;
        }

        if (allOnesCounter == i) {
            returnStatus = IMU_ALL_ONES_WARNING;
        }

        digitalWrite(slaveSelect, HIGH);
    }

    return returnStatus;
}

lsm6dsl_status_t LSM6DSLCore::readRegisterInt16(int16_t* output, uint8_t offset) {
    uint8_t buffer[2];
    lsm6dsl_status_t returnStatus = readRegisterRegion(buffer, offset, 2);
    *output = (int16_t)buffer[0] | (int16_t)buffer[1] << 8;

    return returnStatus;
}

lsm6dsl_status_t LSM6DSLCore::readRegisterInt16(int16_t* output, uint8_t offsetL, uint8_t offsetM) {
    lsm6dsl_status_t returnStatus = IMU_SUCCESS;
    uint8_t nBytes = 2;

    _i2c->beginTransmission(i2cAddress);
    _i2c->write(offsetM);
    _i2c->write(offsetL);
    if (_i2c->endTransmission() != 0) {
        returnStatus = IMU_HW_ERROR;
    }

    int16_t out = 0;
    uint8_t i = 0;

    _i2c->requestFrom(i2cAddress, nBytes);
    while (_i2c->available()) {
        out = (out << (i * 8)) | (int16_t)_i2c->read();
        i++;
    }

    *output = out;
    return returnStatus;
}

lsm6dsl_status_t LSM6DSLCore::writeRegister(uint8_t offset, uint8_t data) {
    lsm6dsl_status_t returnStatus = IMU_SUCCESS;

    if (opMode == LSM6DSL_MODE_I2C) {
        _i2c->beginTransmission(i2cAddress);
        _i2c->write(offset);
        _i2c->write(data);
        if (_i2c->endTransmission() != 0) {
            returnStatus = IMU_HW_ERROR;
        }
    } else if (opMode == LSM6DSL_MODE_SPI) {
        digitalWrite(slaveSelect, LOW);

        SPI.transfer(offset);
        SPI.transfer(data);

        digitalWrite(slaveSelect, HIGH);
    }

    return returnStatus;
}

lsm6dsl_status_t LSM6DSLCore::embeddedPage() {
    return writeRegister(LSM6DSL_ACC_GYRO_FUNC_CFG_ACCESS, 0x80);
}

lsm6dsl_status_t LSM6DSLCore::basePage() {
    return writeRegister(LSM6DSL_ACC_GYRO_FUNC_CFG_ACCESS, 0x00);
}


/* LSM6DSL class definitions */

LSM6DSL::LSM6DSL(TwoWire *i2c, uint8_t pSDA, uint8_t pSCL, uint8_t address): LSM6DSLCore(i2c, pSDA, pSCL, LSM6DSL_MODE_I2C, address) {
     initSettings();
 }

LSM6DSL::LSM6DSL(TwoWire *i2c, uint8_t pSDA, uint8_t pSCL, lsm6dsl_mode_t mode, uint8_t arg): LSM6DSLCore(i2c, pSDA, pSCL, mode, arg) {
    initSettings();
 }

void LSM6DSL::initSettings() {
    settings.gyroEnabled = 1;
    settings.gyroRange = 125;
    settings.gyroSampleRate = 833;//416;
    settings.gyroFifoEnabled = 1;
    settings.gyroFifoDecimation = 1;

    settings.accelEnabled = 1;
    settings.accelODROff = 1;
    settings.accelRange = 2;
    settings.accelSampleRate = 833;//416;
    settings.accelBandWidth = 100;
    settings.accelFifoEnabled = 1;
    settings.accelFifoDecimation = 1;

    settings.tempEnabled = 1;

    //FIFO control data
    settings.fifoThreshold = 3000;
    settings.fifoSampleRate = 10;
    settings.fifoModeWord = 0;
}

lsm6dsl_status_t LSM6DSL::begin() {
    uint8_t data = 0;
    lsm6dsl_status_t returnStatus = beginCore();

    data = 0;
    if (settings.accelEnabled == 1) {
        if (settings.accelSampleRate >= 1660) {
            data |= 0x01;
        }

        switch (settings.accelRange) {
            case 2:
                data |= LSM6DSL_ACC_GYRO_FS_XL_2g;
                break;
            case 4:
                data |= LSM6DSL_ACC_GYRO_FS_XL_4g;
                break;
            case 8:
                data |= LSM6DSL_ACC_GYRO_FS_XL_8g;
                break;
            default:  //set default case to 16(max)
            case 16:
                data |= LSM6DSL_ACC_GYRO_FS_XL_16g;
                break;
        }

        switch (settings.accelSampleRate) {
            case 13:
                data |= LSM6DSL_ACC_GYRO_ODR_XL_13Hz;
                break;
            case 26:
                data |= LSM6DSL_ACC_GYRO_ODR_XL_26Hz;
                break;
            case 52:
                data |= LSM6DSL_ACC_GYRO_ODR_XL_52Hz;
                break;
            default:  //Set default to 104
            case 104:
                data |= LSM6DSL_ACC_GYRO_ODR_XL_104Hz;
                break;
            case 208:
                data |= LSM6DSL_ACC_GYRO_ODR_XL_208Hz;
                break;
            case 416:
                data |= LSM6DSL_ACC_GYRO_ODR_XL_416Hz;
                break;
            case 833:
                data |= LSM6DSL_ACC_GYRO_ODR_XL_833Hz;
                break;
            case 1660:
                data |= LSM6DSL_ACC_GYRO_ODR_XL_1660Hz;
                break;
            case 3330:
                data |= LSM6DSL_ACC_GYRO_ODR_XL_3330Hz;
                break;
            case 6660:
                data |= LSM6DSL_ACC_GYRO_ODR_XL_6660Hz;
                break;;
        }
    }

    writeRegister(LSM6DSL_ACC_GYRO_CTRL1_XL, data);

    data = 0;
    if (settings.gyroEnabled == 1) {
        switch (settings.gyroRange) {
        case 125:
            data |= LSM6DSL_ACC_GYRO_FS_G_125dps;
            break;
        case 245:
            data |= LSM6DSL_ACC_GYRO_FS_G_245dps;
            break;
        case 500:
            data |= LSM6DSL_ACC_GYRO_FS_G_500dps;
            break;
        case 1000:
            data |= LSM6DSL_ACC_GYRO_FS_G_1000dps;
            break;
        default:  //Default to full 2000DPS range
        case 2000:
            data |= LSM6DSL_ACC_GYRO_FS_G_2000dps;
            break;
        }

        switch (settings.gyroSampleRate) {
        case 13:
            data |= LSM6DSL_ACC_GYRO_ODR_G_13Hz;
            break;
        case 26:
            data |= LSM6DSL_ACC_GYRO_ODR_G_26Hz;
            break;
        case 52:
            data |= LSM6DSL_ACC_GYRO_ODR_G_52Hz;
            break;
        default:  //Set default to 104
        case 104:
            data |= LSM6DSL_ACC_GYRO_ODR_G_104Hz;
            break;
        case 208:
            data |= LSM6DSL_ACC_GYRO_ODR_G_208Hz;
            break;
        case 416:
            data |= LSM6DSL_ACC_GYRO_ODR_G_416Hz;
            break;
        case 833:
            data |= LSM6DSL_ACC_GYRO_ODR_G_833Hz;
            break;
        case 1660:
            data |= LSM6DSL_ACC_GYRO_ODR_G_1660Hz;
            break;
        }
    }

    writeRegister(LSM6DSL_ACC_GYRO_CTRL2_G, data);

    return returnStatus;
}

int16_t LSM6DSL::readRawAccelX() {
    int16_t result = 0;
    readRegisterInt16(&result, LSM6DSL_ACC_GYRO_OUTX_L_XL);

    return result;
}

int16_t LSM6DSL::readRawAccelY() {
    int16_t result = 0;
    readRegisterInt16(&result, LSM6DSL_ACC_GYRO_OUTY_L_XL);

    return result;
}

int16_t LSM6DSL::readRawAccelZ() {
    int16_t result = 0;
    readRegisterInt16(&result, LSM6DSL_ACC_GYRO_OUTZ_L_XL);

    return result;
}

float LSM6DSL::readFloatAccelX() {
    return convertAccel(readRawAccelX());
}

float LSM6DSL::readFloatAccelY() {
    return convertAccel(readRawAccelY());
}

float LSM6DSL::readFloatAccelZ() {
    return convertAccel(readRawAccelZ());
}

float LSM6DSL::convertAccel(int16_t axisValue) {
    float sens = 0.031 * settings.accelRange;
    float output = (float)axisValue * sens / 1000;

    return output;
}

int16_t LSM6DSL::readRawTemperature() {
    int16_t result;
    readRegisterInt16(&result, LSM6DSL_ACC_GYRO_OUT_TEMP_L);

    return result;
}

float LSM6DSL::readTemperatureC() {
    float output = (float)(readRawTemperature()) / 256;
    output += 25;

    return output;
}

float LSM6DSL::readTemperatureF() {
    return (readTemperatureC() * 9) / 5 + 32;
}

int16_t LSM6DSL::readRawGyroX() {
    int16_t output;
    readRegisterInt16(&output, LSM6DSL_ACC_GYRO_OUTX_L_G);

    return output;
}

int16_t LSM6DSL::readRawGyroY() {
    int16_t output;
    readRegisterInt16(&output, LSM6DSL_ACC_GYRO_OUTY_L_G);

    return output;
}

int16_t LSM6DSL::readRawGyroZ() {
    int16_t output;
    readRegisterInt16(&output, LSM6DSL_ACC_GYRO_OUTZ_L_G);

    return output;
}

float LSM6DSL::readFloatGyroX() {
    return convertGyro(readRawGyroX());
}

float LSM6DSL::readFloatGyroY() {
    return convertGyro(readRawGyroY());
}

float LSM6DSL::readFloatGyroZ() {
    return convertGyro(readRawGyroZ());
}


float LSM6DSL::convertGyro(int16_t axisValue) {
    uint8_t divisor = settings.gyroRange / 125;
    if (settings.gyroRange == 245) {
        divisor = 2;
    }

    return (float)(axisValue) * 4.375 * divisor / 1000;
}
