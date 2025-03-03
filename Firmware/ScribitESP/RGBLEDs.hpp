#pragma once

#include "esp32-hal-ledc.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define RED1 (2)
#define GREEN1 (4)
#define BLUE1 (35)

#define RED2 (0)
#define GREEN2 (1)
#define BLUE2 (3)

#define RED3 (36)
#define GREEN3 (37)
#define BLUE3  (34)

// use first channel of 16 channels (started from zero)
#define LED_R_CHANNEL LEDC_CHANNEL_0
#define LED_G_CHANNEL LEDC_CHANNEL_1
#define LED_B_CHANNEL LEDC_CHANNEL_2

#define LED_GROUP_0 LEDC_CHANNEL_0
#define LED_GROUP_1 LEDC_CHANNEL_1
#define LED_GROUP_2 LEDC_CHANNEL_2

// use 13 bit precission for LEDC timer
#define LEDC_TIMER_13_BIT 13

// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ 5000

//Function
#define BLINK_UP 0
#define BLINK_DOWN 1
#define DOUBLE_BLINK_UP 2
#define DOUBLE_BLINK_DOWN 3

#define RAINBOW_RED 0
#define RAINBOW_YELLOW 1
#define RAINBOW_GREEN 2
#define RAINBOW_BLUE 3
#define RAINBOW_VIOLET 4

#define PULSE_IN 0
#define PULSE_OUT 1
enum RGBLEDState
{
    RGBLED_RESET,
    RGBLED_BLINK,
    RGBLED_RAINBOW,
    RGBLED_DOUBLE_BLINK,
    RGBLED_PULSE
};

class RGBLEDs
{
    RGBLEDState state; //Active function
    uint8_t ledValues[3];
    uint32_t lastT;
    float period;
    float m_pulseFadeIn;
    float m_pulseFadeOut;
    uint8_t function;
    SemaphoreHandle_t mutex;
    
    /**
     * @brief Convert duty cycle from 0-255 to timer value
     * 
     * @param val[in] duty 0-255
     * 
     * @return uint32_t duty for a 13 bit timer
     */
    uint32_t evalDuty(uint8_t val);
    
    /**
     * @brief Groups leds by color in RGB arrangement.
     */
    void normalSetup();

    /**
     * @brief * DISCLAIMER * Feature has never been tested.
     *        This will be used to mimic the Kitt car lights effect.
     */
    void KITTSetup();

    /**
     * @brief Set led duty cycle
     * 
     * @param channel[in] led channel
     * @param val[in] 0-255
     */
    void setDuty(ledc_channel_t channel, uint8_t val);

    /**
     * @brief Apply fade
     * 
     * @param channel[in] led channel
     * @param val[in] target duty cycle
     * @param FadeTime[in] fade time in seconds
     */
    void fade(ledc_channel_t channel, uint8_t val, float FadeTime);
    
    /**
     * @brief Turn all leds off
     */
    void reset();

  public:
    /**
     * @brief Construct a new RGBLEDs::RGBLEDs object
     */
    RGBLEDs();

    /**
     * @brief Set leds solid color
     * 
     * @param redVal[in] Red component 0-255
     * @param greenVal[in] Green component 0-255
     * @param blueVal[in] Blue component 0-255
     */
    void setColor(uint8_t redVal, uint8_t greenVal, uint8_t blueVal);

    /**
     * @brief Blink with given period
     * 
     * @param redVal[in] Red component 0-255
     * @param greenVal[in] Green component 0-255
     * @param blueVal[in] Blue component 0-255
     * @param t_period[in] period in seconds
     */
    void blink(uint8_t redVal, uint8_t greenVal, uint8_t blueVal, float t_period);

    /**
     * @brief Double blink led with given components
     * 
     * @param redVal[in] Red component 0-255
     * @param greenVal[in] Green component 0-255
     * @param blueVal[in] Blue component 0-255
     * @param t_period[in] period in seconds
     */
    void doubleBlink(uint8_t redVal, uint8_t greenVal, uint8_t blueVal, float t_period);

    /**
     * @brief Rainbows leds
     * 
     * @param t_period[in] period in seconds
     */
    void rainbow(float t_period);

    /**
     * @brief Similar to blikn function is used to aplly pulse effect on the leds allowing to have a different fade in time and fade out time
     * 
     * @param p_r[in] The value for the red channel
     * @param p_g[in] The value for the green channel
     * @param p_b[in] The value for the blue channel
     * @param p_fadeIn[in] The fade in time
     * @param p_fadeOut[in] The fade out time
     */
    void pulse(uint8_t p_r, uint8_t p_g, uint8_t p_b, float p_fadeIn, float p_fadeOut);

    /**
     * @brief Update function to be called continuously (Or at least every fade end)
     * 
     * @param maxSemaphoreWait[in] maximum time to wait on semaphore in ms
     */
    void update(uint32_t maxSemaphoreWait=0xFFFFFFFF);
};