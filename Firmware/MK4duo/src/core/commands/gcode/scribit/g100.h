#define CODE_G100
#define MOBILE_AVG_WINDOW 50

#define MOVEMENT_ANGLE 30

#include "g101.h"

uint8_t g_selectedPen;
int g_samplesNumber;
float g_maxValue;
float g_average;

inline void evaluate(float *p_computedArray)
{
    int l_index = 0;
    float l_current = 0.0f;
    float l_threshold = 0.0f;

    l_threshold = g_maxValue - g_average;
    l_threshold *= l_threshold;
    l_threshold *= 0.65f;

    Serial.print("Average :");
    Serial.println(g_average);
    Serial.print("Max value: ");
    Serial.println(g_maxValue);
    Serial.print("Threshold: ");
    Serial.println(l_threshold);
    Serial.flush();

    while((l_index < g_samplesNumber - MOBILE_AVG_WINDOW && l_current < l_threshold) || l_index < g_samplesNumber * 0.50f)
    {
        l_current = p_computedArray[l_index] - g_average;
        l_current *= l_current;
        
        l_index++;
    }

    Serial.print("Stopped at value: ");
    Serial.print(l_current);
    Serial.print(" at index: ");
    Serial.println(l_index);
    Serial.flush();

    setMovingTime((float)((MOVEMENT_ANGLE * l_index-1)/(g_samplesNumber - MOBILE_AVG_WINDOW)), g_selectedPen);
}

inline float* calculateAverage(float *p_xValues, float *p_yValues, float *p_zValues)
{
    int l_mobileAvgIndex;
    int l_index;

    float *l_computedArray = new float[g_samplesNumber];
    float l_currentMobileAvg;
    float l_average;

    if(l_computedArray != nullptr)
    {
        for(l_index = 0; l_index < g_samplesNumber; l_index++)
        {
            l_computedArray[l_index] = (p_xValues[l_index] * p_xValues[l_index]) +
                                       (p_yValues[l_index] * p_yValues[l_index]) +
                                       (p_zValues[l_index] * p_zValues[l_index]);

            g_average += l_computedArray[l_index];
        }

        g_average /= g_samplesNumber;

        for(l_index = MOBILE_AVG_WINDOW; l_index < g_samplesNumber; l_index++)
        {
            l_currentMobileAvg = 0;

            for(l_mobileAvgIndex = l_index - MOBILE_AVG_WINDOW; l_mobileAvgIndex < l_index; l_mobileAvgIndex++)
            {
                l_currentMobileAvg += l_computedArray[l_mobileAvgIndex];
            }

            l_currentMobileAvg /= MOBILE_AVG_WINDOW;
            
            if(l_currentMobileAvg > g_maxValue)
            {
                g_maxValue = l_currentMobileAvg;
            }
            
            l_computedArray[l_index - MOBILE_AVG_WINDOW] = l_currentMobileAvg;
        }
    }
    else
    {
        Serial.println("** ERROR ** UNABLE TO ALLOCATE MEMORY ON evaluate().");
    }

    return l_computedArray;
}

inline void move()
{
    bool l_ready = false;
    
    float *l_xValues = new float [900];
    float *l_yValues = new float [900];
    float *l_zValues = new float [900];

    if(l_xValues == nullptr || l_yValues == nullptr || l_zValues == nullptr)
    {
        Serial.println("** ERROR ** UNABLE TO ALLOCATE MEMORY ON move().");        
    }
    else
    {
        float *l_computedArray = nullptr;
        
        mechanics.destination[Z_AXIS] = mechanics.destination[Z_AXIS] - MOVEMENT_ANGLE;
        mechanics.prepare_move_to_destination();

        while(planner.has_blocks_queued() || planner.cleaning_buffer_flag)
        {
            l_xValues[g_samplesNumber] = SIIMU.getXGyroscope();
            l_yValues[g_samplesNumber] = SIIMU.getYGyroscope();
            l_zValues[g_samplesNumber] = SIIMU.getZGyroscope();

            g_samplesNumber++;
        }

        Serial.print("Number of samples: ");
        Serial.println(g_samplesNumber);
        Serial.flush();

        l_computedArray = calculateAverage(l_xValues, l_yValues, l_zValues);

        delete[] l_xValues;
        delete[] l_yValues;
        delete[] l_zValues;

        if(l_computedArray != nullptr)
        {
            evaluate(l_computedArray);
            l_ready = true;
        }
        else
        {
            Serial.println("ERROR! No computed array!");
        }

        delete[] l_computedArray;
    }
    
    mechanics.destination[Z_AXIS] = mechanics.destination[Z_AXIS] + MOVEMENT_ANGLE;
    mechanics.prepare_move_to_destination();

    while(planner.has_blocks_queued() || planner.cleaning_buffer_flag)
    {
        printer.keepalive(InProcess);
        printer.idle();
    }

    if(!l_ready) //CAUTION! THE CONDITION IS **NOT** READY
    {
        setMovingTime(MOVEMENT_ANGLE, g_selectedPen);
    }
}

inline void gcode_G100(void)
{
    g_selectedPen = getSelectedPen();
    g_samplesNumber = 0;
    g_maxValue = 0;
    g_average = 0;
    
    if(SIIMU.getStatus() == IMU_SUCCESS)
    {
        float l_initialAcceleration = mechanics.data.travel_acceleration; //OLD SPEED
        mechanics.data.travel_acceleration = 1000;
        mechanics.feedrate_mm_s = MMM_TO_MMS(2000);
        move();
        mechanics.data.travel_acceleration = l_initialAcceleration;
    }  
    else
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
}
