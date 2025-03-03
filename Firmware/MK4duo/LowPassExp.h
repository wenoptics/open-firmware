/***********************************************************************
 * Exponential low filter class
 * 
 * Author: Alessio Graziano
 * Created: 2019-05-22
 * Last edit:
 * Version: 1.0
 * License: MIT
 ***********************************************************************/

#pragma once
template <class T>
class LowPassExp
{
    float m_tau;
    T m_value;
    bool m_firstValueRead;

public:
    LowPassExp()
    {
        m_firstValueRead = false;
    }
    LowPassExp(float tau)
    {
        LowPassExp();
        init(tau);
    }
    void init(float tau) { m_tau = tau; };
    /**
     * @brief Get the Value 
     * 
     * @return Filtered value
     */
    T getValue(){return m_value;};
    /**
     * @brief Add a value to filter and return filtered
     * 
     * @param newValue value to add
     * @param loopTimeS time since last value
     * @return filtered value
     */
    T addValue(T newValue, float loopTimeS)
    {
        //If first value
        if (!m_firstValueRead)
        {
            //Just save value
            m_firstValueRead = true;
            m_value = newValue;
            return m_value;
        }
        //Evaluate a
        float a = exp((float)-loopTimeS / m_tau);
        return (m_value = a * m_value + (1 - a) * newValue);
    };
};