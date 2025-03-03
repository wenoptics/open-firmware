#include "RGBLEDs.hpp"

#include <Arduino.h>

uint32_t RGBLEDs::evalDuty(uint8_t val)
{
    uint32_t duty = (8191 * (255 - val)) / 255;
    return duty;
}

void RGBLEDs::setDuty(ledc_channel_t channel, uint8_t val)
{
    uint32_t duty = evalDuty(val) /*valueMax*/;
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, channel);
}

void RGBLEDs::fade(ledc_channel_t channel, uint8_t val, float FadeTime)
{
    uint32_t duty = evalDuty(val) /*valueMax*/;
    uint32_t scale = 80;
    uint32_t cicle_num = scale * FadeTime * LEDC_BASE_FREQ / (8191);

    if (cicle_num == 0)
        cicle_num = 1;

    ledc_set_fade_step_and_start(LEDC_HIGH_SPEED_MODE, channel, duty, scale, cicle_num, LEDC_FADE_NO_WAIT);
}

void RGBLEDs::reset()
{
    setDuty(LED_R_CHANNEL, 0);
    setDuty(LED_G_CHANNEL, 0);
    setDuty(LED_B_CHANNEL, 0);
}

RGBLEDs::RGBLEDs()
{
    uint8_t LEDArray[9] = {RED1, GREEN1, BLUE1, RED2, GREEN2, BLUE2, RED3, GREEN3, BLUE3};
    for (uint8_t i = 0; i < 9; i++)
    {
        pinMode(LEDArray[i], OUTPUT);
    }

    normalSetup();

    // Initialize fade service.
    ledc_fade_func_install(0);

    reset();

    state = RGBLED_RESET;

    //Create mutex
    mutex = xSemaphoreCreateMutex();

    //Set led to off
    for (int i = 0; i < 3; i++)
        ledValues[i] = 0;
}

void RGBLEDs::normalSetup()
{
    ledcSetup(LED_R_CHANNEL, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
    ledcAttachPin(RED1, LED_R_CHANNEL);
    ledcAttachPin(RED2, LED_R_CHANNEL);
    ledcAttachPin(RED3, LED_R_CHANNEL);

    ledcSetup(LED_G_CHANNEL, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
    ledcAttachPin(GREEN1, LED_G_CHANNEL);
    ledcAttachPin(GREEN2, LED_G_CHANNEL);
    ledcAttachPin(GREEN3, LED_G_CHANNEL);

    ledcSetup(LED_B_CHANNEL, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
    ledcAttachPin(BLUE1, LED_B_CHANNEL);
    ledcAttachPin(BLUE2, LED_B_CHANNEL);
    ledcAttachPin(BLUE3, LED_B_CHANNEL);
}

void RGBLEDs::KITTSetup()
{
    ledcSetup(LED_R_CHANNEL, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
    ledcAttachPin(RED1, LED_GROUP_0);
    ledcAttachPin(GREEN1, LED_GROUP_0);
    ledcAttachPin(BLUE1, LED_GROUP_0);

    ledcSetup(LED_G_CHANNEL, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
    ledcAttachPin(RED2, LED_GROUP_1);
    ledcAttachPin(GREEN2, LED_GROUP_1);
    ledcAttachPin(BLUE2, LED_GROUP_1);

    ledcSetup(LED_B_CHANNEL, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
    ledcAttachPin(RED3, LED_GROUP_2);
    ledcAttachPin(GREEN3, LED_GROUP_2);
    ledcAttachPin(BLUE3, LED_GROUP_2);
}

void RGBLEDs::setColor(uint8_t redVal, uint8_t greenVal, uint8_t blueVal)
{
    setDuty(LED_R_CHANNEL, redVal);
    setDuty(LED_G_CHANNEL, greenVal);
    setDuty(LED_B_CHANNEL, blueVal);
    state = RGBLED_RESET;

    //Save color
    ledValues[0] = redVal;
    ledValues[1] = greenVal;
    ledValues[2] = blueVal;
}

void RGBLEDs::blink(uint8_t redVal, uint8_t greenVal, uint8_t blueVal, float t_period)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    state = RGBLED_BLINK;
    lastT = millis();
    period = t_period / 2;
    function = BLINK_DOWN;
    ledValues[0] = redVal;
    ledValues[1] = greenVal;
    ledValues[2] = blueVal;
    xSemaphoreGive(mutex);

    reset();
}

void RGBLEDs::doubleBlink(uint8_t redVal, uint8_t greenVal, uint8_t blueVal, float t_period)
{
    blink(redVal, greenVal, blueVal, t_period);
    xSemaphoreTake(mutex, portMAX_DELAY);
    state = RGBLED_DOUBLE_BLINK;
    period = t_period / 3;
    xSemaphoreGive(mutex);
}

void RGBLEDs::rainbow(float t_period)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    state = RGBLED_RAINBOW;
    lastT = millis();
    period = t_period / 5;
    function = RAINBOW_RED;
    xSemaphoreGive(mutex);

    reset();
}

void RGBLEDs::pulse(uint8_t p_r, uint8_t p_g, uint8_t p_b, float p_fadeIn, float p_fadeOut)
{
    xSemaphoreTake(mutex, portMAX_DELAY);
    state = RGBLED_PULSE;
    lastT = millis();
    //period = p_period/2; //TODO REMEMBER ABOUT ON PURPOSE VARIABLES
    m_pulseFadeIn = p_fadeIn;
    m_pulseFadeOut = p_fadeOut;
    function = PULSE_OUT;
    ledValues[0] = p_r;
    ledValues[1] = p_g;
    ledValues[2] = p_b;

    xSemaphoreGive(mutex);
}

void RGBLEDs::update(uint32_t maxSemaphoreWait)
{
    //Blink---------------------------------------------------
    if (state == RGBLED_BLINK || state == RGBLED_DOUBLE_BLINK)
    {
        if (millis() - lastT > period * 1000)
        {
            xSemaphoreTake(mutex, ((maxSemaphoreWait == 0xFFFFFFFF) ? portMAX_DELAY : maxSemaphoreWait / portTICK_PERIOD_MS));
            //Change function
            if (function == BLINK_UP) //If was blinking up
            {
                if (state == RGBLED_DOUBLE_BLINK)
                {
                    function = DOUBLE_BLINK_DOWN;
                    fade(LED_R_CHANNEL, 0, period / 2);
                    fade(LED_G_CHANNEL, 0, period / 2);
                    fade(LED_B_CHANNEL, 0, period / 2);
                }
                else
                {
                    function = BLINK_DOWN;
                    //printf("down\n");
                    fade(LED_R_CHANNEL, 0, period);
                    fade(LED_G_CHANNEL, 0, period);
                    fade(LED_B_CHANNEL, 0, period);
                }
            }
            else if (function == BLINK_DOWN)
            {
                function = BLINK_UP;
                fade(LED_R_CHANNEL, ledValues[0], period);
                fade(LED_G_CHANNEL, ledValues[1], period);
                fade(LED_B_CHANNEL, ledValues[2], period);
            }
            else if (function == DOUBLE_BLINK_DOWN)
            {
                function = DOUBLE_BLINK_UP;
                fade(LED_R_CHANNEL, ledValues[0], period / 2);
                fade(LED_G_CHANNEL, ledValues[1], period / 2);
                fade(LED_B_CHANNEL, ledValues[2], period / 2);
            }
            else if (function == DOUBLE_BLINK_UP)
            {
                function = BLINK_DOWN;
                fade(LED_R_CHANNEL, 0, period);
                fade(LED_G_CHANNEL, 0, period);
                fade(LED_B_CHANNEL, 0, period);
            }

            lastT = millis();
            xSemaphoreGive(mutex);
        }
    }
    //Rainbow--------------------------------------------------
    else if (state == RGBLED_RAINBOW)
    {
        //Change color
        if (millis() - lastT > period * 1000)
        {
            xSemaphoreTake(mutex, ((maxSemaphoreWait == 0xFFFFFFFF) ? portMAX_DELAY : maxSemaphoreWait / portTICK_PERIOD_MS));
            function = (function + 1) % 5;
            lastT = millis();
            xSemaphoreGive(mutex);
        }

        switch (function)
        {
        case RAINBOW_RED:
            fade(LED_G_CHANNEL, 255, period);
            break;
        case RAINBOW_YELLOW:
            fade(LED_R_CHANNEL, 0, period);
            break;
        case RAINBOW_GREEN:
            fade(LED_G_CHANNEL, 0, period);
            fade(LED_B_CHANNEL, 255, period);
            break;
        case RAINBOW_BLUE:
            fade(LED_R_CHANNEL, 255, period);
            break;
        case RAINBOW_VIOLET:
            fade(LED_B_CHANNEL, 0, period);
            break;
        }
    }
    else if(state == RGBLED_PULSE)
    {
        if(millis() - lastT > period * 1000)
        {
            xSemaphoreTake(mutex, ((maxSemaphoreWait == 0xFFFFFFFF) ? portMAX_DELAY : maxSemaphoreWait / portTICK_PERIOD_MS));
            function = (function + 1) % 2;
            lastT = millis();
            xSemaphoreGive(mutex);
        }

        switch (function)
        {
        case PULSE_IN:
            fade(LED_R_CHANNEL, ledValues[0], m_pulseFadeIn);
            fade(LED_G_CHANNEL, ledValues[1], m_pulseFadeIn);
            fade(LED_B_CHANNEL, ledValues[2], m_pulseFadeIn);
            break;
        case PULSE_OUT:
            fade(LED_R_CHANNEL, 0, m_pulseFadeOut);
            fade(LED_G_CHANNEL, 0, m_pulseFadeOut);
            fade(LED_B_CHANNEL, 0, m_pulseFadeOut);
            break;
        }
    }
    //Fixed (Refresh)
    else
    {
        setDuty(LED_R_CHANNEL, ledValues[0]);
        setDuty(LED_G_CHANNEL, ledValues[1]);
        setDuty(LED_B_CHANNEL, ledValues[2]);
    }
}
