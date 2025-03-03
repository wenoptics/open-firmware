/**
 * Request for IMU data Gcode
 * 
 */

#define CODE_M777

#define SI_WAIT_BEFORE_INERTIAL_DATA_MS 500

/**
 * M777: Finish all moves and ask for IMU value
 */
inline void gcode_M777(void)
{
    float roll, pitch, yaw;
    char buffer[64];
    millis_t waitFor;

    //Wait for buffer to end (While updating IMU)
    planner.synchronize();

    //Wait given ms
    waitFor = millis() + SI_WAIT_BEFORE_INERTIAL_DATA_MS;
    while (PENDING(millis(), waitFor))
    {
        printer.keepalive(InProcess);
        printer.idle();
    }

    //Check imu state
    if (SIIMU.getStatus() != IMU_SUCCESS)
    {
        //Reply with error
        SERIAL_STR(ER);
        SERIAL_PS("IMU unavailable, error ");
        SERIAL_CHR('0'+SIIMU.getStatus());
        SERIAL_EOL();
        //And ok too to avoid stopping execution
        SERIAL_STR(OK);
        SERIAL_EOL();
        return;    
    }

    //Get IMU values
    //roll = SIIMU.getRoll();
    //yaw = SIIMU.getYaw();

    for(int i = 0; i < 10; i++)
    {
        pitch += SIIMU.getPitch();
    }

    pitch /= 10;

    //Create output string
    sprintf(buffer, "I:%.1f", pitch);
    //sprintf(buffer, "I:%.1f %.1f", roll, yaw);

    //Evaluate checksum
    /*uint8_t cs = 0;
    for (int i = 0; i < strlen(buffer) && buffer[i] != NULL; i++)
    {
        cs = cs ^ buffer[i];
        //Serial.println(cs);
    }
    cs &= 0xff;*/
    SERIAL_STR(OK);
    SERIAL_CHR(' ');
    SERIAL_MSG(buffer);
    /*SERIAL_CHR('*');
    SERIAL_VAL(cs);*/
    SERIAL_EOL();
}
