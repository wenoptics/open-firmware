#define CODE_G101

float g_timeLimit[4] = {30.0f, 30.0f, 30.0f, 30.0f};

inline uint8_t getSelectedPen()
{
    uint8_t l_retVal = -1;
    
    if(mechanics.destination[Z_AXIS] == 89)
    {
        l_retVal = 0;
    }
    else if(mechanics.destination[Z_AXIS] == 161)
    {
        l_retVal = 1;
    }
    else if(mechanics.destination[Z_AXIS] == 233)
    {
        l_retVal = 2;
    }
    else if(mechanics.destination[Z_AXIS] == 305)
    {
        l_retVal = 3;
    }
    else
    {
        Serial.print("ERROR! Cannot identify pen! Value is: ");
        Serial.println(mechanics.destination[Z_AXIS]);
        Serial.flush();
    }

    Serial.print("Selected pen: ");
    Serial.println(l_retVal+1);
    Serial.flush();
    
    return l_retVal;
}

inline extern void setMovingTime(float p_movingAngle, uint8_t p_selectedPen)
{
    g_timeLimit[p_selectedPen] = p_movingAngle;

    Serial.print("New Angle is: ");
    Serial.println(g_timeLimit[p_selectedPen]);
    Serial.flush();
}

inline void gcode_G101(void)
{ 
    uint8_t l_selectedPen = getSelectedPen();
    
    Serial.println("********************** G101");
    Serial.print("Movement time is:");
    Serial.println(g_timeLimit[l_selectedPen]);
    Serial.flush();

    mechanics.data.travel_acceleration = 1000;
    mechanics.feedrate_mm_s = MMM_TO_MMS(2000);

    if(l_selectedPen >= 0 && l_selectedPen <= 3)
    {
        mechanics.destination[Z_AXIS] = mechanics.destination[Z_AXIS] - g_timeLimit[l_selectedPen];
    }
    else
    {
        mechanics.destination[Z_AXIS] = mechanics.destination[Z_AXIS] - 30;
    }

    mechanics.prepare_move_to_destination();

    while(planner.has_blocks_queued() || planner.cleaning_buffer_flag)
    {
        printer.keepalive(InProcess);
        printer.idle();
    }
}
