/**
 * g77.h
 * 
 * Scribit Z homing command
 *
 * Created by: Alessio Graziano (alessio.graziano@shinsekai.it)
 * Created:   2019-06-17
 * Last edit: 2019-08-07
 */

#define CODE_G77

#include "../../../../../SIHall.h"

#define SERIAL_DEBUG //Enable to debug read data

//filter parameters----------------------------------
#define N_SAMPLES 16
#define N_SAMPLES2 4 // number of element to be shifted to make the division - based on buffer size
uint16_t numArray[N_SAMPLES];

//Thresholds-----------------------------------------
#define HALL_HIGH_THRESHOLD_K 0.9f
#define HALL_LOW_THRESHOLD_K 0.3f

//Hall calibration validation parameters------------------
#define THRESHOLD_DELTA_NO_HALL 80 //If delta is over this value assume there is no hall sensor
#define THRESHOLD_DELTA_MIN 8      //If delta below this value no magnet has been detected

enum HallState
{
    HALL_RESET, //Need calibration
    HALL_OK,
    HALL_NOT_FOUND, //Old hardware
    HALL_ERROR      //Cannot find magnet
};

//Hall state
HallState hallState = HALL_RESET;
uint8_t highThreshold, lowThreshold;
uint16_t MEAN;
float g_threshold;

/**
 * @brief Signals hall sensor hasn't been found
 * 
 * @param delta calibration delta
 */
inline void signalHallNotFound(uint16_t delta = 65535)
{
    SERIAL_STR(ECHO);
    SERIAL_PS("No hall sensor found. G77 Disabled.");
    if (delta != 65535)
    {
        SERIAL_PS("Delta:");
        SERIAL_VAL(delta);
    }
    SERIAL_EOL();
#ifdef SERIAL_DEBUG
    Serial.println("No hall sensor found. G77 Disabled");
#endif
}
/**
 * @brief Signals a hall sensor fail
 * 
 */
inline void signalHallError()
{
    SERIAL_STR(ER);
    SERIAL_PS("Hall sensor fail");
    SERIAL_EOL();
#ifdef SERIAL_DEBUG
    Serial.println("Hall sensor fail");
#endif
}
/**
 * @brief Initialize and calibrate hall sensing algorithm
 * 
 */
inline void initHall()
{
    uint64_t valueSum = 0;
    uint16_t nValues = 0;
    uint16_t minValue = 65535;
    uint16_t temp;
    uint8_t index = 0;

    //Move to disengage if down-----------------------------------------------
    mechanics.do_blocking_move_to_z(mechanics.current_position[Z_AXIS] + 90, MMM_TO_MMS(HOMING_SPEED_MMM));

    //Calibrate hall---------------------------------------------------------------------

    //Save current acceleration
    float initialAcceleration = mechanics.data.travel_acceleration;

#ifdef SERIAL_DEBUG
    int32_t startStepperPosition = stepper.position(Z_AXIS);
#endif
    //Move 360 and evaluate median
    //Plan movement
    mechanics.data.travel_acceleration = 1000; //Pump acceleration to have better median
    mechanics.destination[X_AXIS] = mechanics.current_position[X_AXIS];
    mechanics.destination[Y_AXIS] = mechanics.current_position[Y_AXIS];
    mechanics.destination[Z_AXIS] = mechanics.current_position[Z_AXIS] + (360);
    mechanics.feedrate_mm_s = MMM_TO_MMS(HOMING_SPEED_MMM);
    mechanics.prepare_move_to_destination();

    //Drop old hall data
    hall_newData = false;

    //Loop until movement ends
    while (planner.has_blocks_queued() || planner.cleaning_buffer_flag)
    {
        uint16_t sum = 0;
        //Check for new data
        if (hall_newData)
        {
            //Read data
            temp = hall_temp;
            hall_newData = false;

            //Sum new data
            valueSum += temp;
            nValues++;

            //Add data to circular buffer
            numArray[index % N_SAMPLES] = temp;

            //Evaluate filtered value
            for (int i = 0; i <= index && i < N_SAMPLES; i++)
                sum += numArray[i];

            if (index < N_SAMPLES)
                sum /= index + 1;
            else
                sum >>= N_SAMPLES2;
            index++;

            //Save minimum
            if (sum < minValue)
                minValue = sum;

#ifdef SERIAL_DEBUG
            //Prints value to serial
            //Step;Unfiltered hall;Filtered hall
            Serial.print(startStepperPosition - stepper.position(Z_AXIS));
            Serial.print(";");
            Serial.print(temp);
            Serial.print(";");
            Serial.println(sum);
#endif
        }

        //Keep printer alive while looping
        printer.keepalive(InProcess);
        printer.idle();
    }

    //Evaluate median and values
    MEAN = valueSum / nValues;
    uint16_t delta = MEAN - minValue;
    highThreshold = (float)delta * HALL_HIGH_THRESHOLD_K;
    lowThreshold = (float)delta * HALL_LOW_THRESHOLD_K;

    g_threshold = (delta*delta) * 0.95;

    Serial.print("Threshold: ");
    Serial.println(g_threshold);
    Serial.print("Average: ");
    Serial.println(MEAN);
    Serial.flush();

    //Check for initialization results-----------
    if (delta > THRESHOLD_DELTA_NO_HALL) //Old hardware
    {
        //Bring it back to initial position
        mechanics.do_blocking_move_to_z(mechanics.current_position[Z_AXIS] + 270, MMM_TO_MMS(HOMING_SPEED_MMM));

        //Signal no hall sensor
        hallState = HALL_NOT_FOUND;
        signalHallNotFound(delta);
    }
    else if (delta < THRESHOLD_DELTA_MIN) //Magnet not found
    {
#ifdef SERIAL_DEBUG
        Serial.println("Difference between threshold too low");
#endif
        SERIAL_STR(ER);
        SERIAL_PS("Hall sensor cannot find magnet field. Delta: ");
        SERIAL_VAL(delta);
        SERIAL_EOL();

        hallState = HALL_ERROR;
        //Bring it back to initial position
        mechanics.do_blocking_move_to_z(mechanics.current_position[Z_AXIS] + 270, MMM_TO_MMS(HOMING_SPEED_MMM));
    }
    else //Parameters ok
    {
        //Set hall sensor as initialized
        hallState = HALL_OK;
    }

    //Set acceleration to initial
    mechanics.data.travel_acceleration = initialAcceleration;
    //Set Z to 0
    mechanics.current_position[Z_AXIS] = 0;
    mechanics.sync_plan_position();

#ifdef SERIAL_DEBUG
    Serial.print("Delta:");
    Serial.print(delta);
    Serial.print(" High threshold: ");
    Serial.print(highThreshold);
    Serial.print(" Low threshold: ");
    Serial.print(lowThreshold);
    Serial.print(" Median: ");
    Serial.println(MEAN);

#endif
}
/**
 * @brief Execute cylinder homing routine
 * 
 * @return true cylinder homing success
 * @return false homing fail
 */
inline bool home_cylinder()
{
    bool awayFromZero = false, triggered = false;
    uint16_t temp = 0;
    float initialAcceleration = mechanics.data.travel_acceleration;

    mechanics.data.travel_acceleration = 1000; //Pump acceleration to have better median
    mechanics.destination[X_AXIS] = mechanics.current_position[X_AXIS];
    mechanics.destination[Y_AXIS] = mechanics.current_position[Y_AXIS];
    mechanics.destination[Z_AXIS] = mechanics.current_position[Z_AXIS] + (720);

    mechanics.feedrate_mm_s = MMM_TO_MMS(HOMING_SPEED_MMM);
    mechanics.prepare_move_to_destination();

    uint8_t index = 0;
    uint32_t clearStartT = 0;
    //Loop until movement ends
    while (planner.has_blocks_queued() || planner.cleaning_buffer_flag)
    {
        uint16_t sum;
        //Check for new data
        if (hall_newData)
        {
            //Read data
            temp = hall_temp;
            hall_newData = false;

            numArray[index % N_SAMPLES] = temp;

            sum = 0;
            for (int i = 0; i <= index && i < N_SAMPLES; i++)
                sum += numArray[i];

            if (index < N_SAMPLES)
                sum /= index + 1;
            else
                sum >>= N_SAMPLES2;

            index++;

            sum = (MEAN - sum);
            sum *= sum;

            if(sum >= g_threshold)
            {
                printer.quickstop_stepper();
                triggered = true;

                Serial.print("Triggered, value is: ");
                Serial.println(sum);
                Serial.flush();
            }            
        }
        
        printer.keepalive(InProcess);
        printer.idle();
    }

    //Set acceleration to initial
    mechanics.data.travel_acceleration = initialAcceleration;

    //If not triggered an error must be occurred
    if (!triggered)
    {
        hallState = HALL_ERROR;
#ifdef SERIAL_DEBUG
        Serial.println("Hall sensor failure");
#endif
    }

    //Set Z to 0
    mechanics.current_position[Z_AXIS] = 0;
    mechanics.sync_plan_position();

    return triggered;
}

inline void faultyMagnetCalibration()
{
    float initialAcceleration = mechanics.data.travel_acceleration;

    mechanics.data.travel_acceleration = 1000; //Pump acceleration to have better median
    mechanics.feedrate_mm_s = MMM_TO_MMS(HOMING_SPEED_MMM);

    mechanics.destination[X_AXIS] = mechanics.current_position[X_AXIS];
    mechanics.destination[Y_AXIS] = mechanics.current_position[Y_AXIS];
    mechanics.destination[Z_AXIS] = mechanics.current_position[Z_AXIS] + (59);

    mechanics.prepare_move_to_destination();

    while (planner.has_blocks_queued() || planner.cleaning_buffer_flag)
    {
        printer.keepalive(InProcess);
        printer.idle();
    }

    mechanics.destination[Z_AXIS] = mechanics.current_position[Z_AXIS] - (70);

    mechanics.prepare_move_to_destination();

    while (planner.has_blocks_queued() || planner.cleaning_buffer_flag)
    {
        printer.keepalive(InProcess);
        printer.idle();
    }

    mechanics.destination[Z_AXIS] = mechanics.current_position[Z_AXIS] + (352);

    mechanics.prepare_move_to_destination();

    while (planner.has_blocks_queued() || planner.cleaning_buffer_flag)
    {
        printer.keepalive(InProcess);
        printer.idle();
    }

    //G92 LIKE
    mechanics.current_position[Z_AXIS] = 0;
    mechanics.sync_plan_position();
    
    mechanics.data.travel_acceleration = initialAcceleration;
}

/**
 * G777: Home extruder with hall sensor
 */
inline void gcode_G77(void)
{
#ifdef SERIAL_DEBUG
    Serial.begin(115200);
#endif

    //Wait for other commands to end
    planner.synchronize();

    //If not inizialized or had an error calibrate hall
    if (hallState == HALL_RESET || hallState == HALL_ERROR)
    {
        // make an ADC calibration before starting the acquisition - just once.
        analogCalibrate();
        // speed-up the AD conversion to lower the accuracy filtering some noise - we are interested in a digital-like signal
        analogPrescaler(ADC_CTRLB_PRESCALER_DIV16_Val);
        //Calibrate hall sensor
        initHall();
    }
    else if (hallState == HALL_NOT_FOUND) //If not found just signal it
    {
        signalHallNotFound();
    }

    if(hallState != HALL_NOT_FOUND)
    {
        if (!home_cylinder())
        {
            //If cylinder home fails signal error
            hallState = HALL_ERROR;
            signalHallError();
        }
    }


    faultyMagnetCalibration();

    //Signal position
    //mechanics.report_current_position();
}
