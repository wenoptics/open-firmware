/*******************
 * Scribit MQTT message
 * *****************/
#pragma once
#include "SIConfig.hpp"
#define SIMQTTMESSAGE_MAX_LEN SI_MQTT_MAX_PAYLOAD_LEN
struct SIMQTTMessage
{
    enum MsgType
    {
        NONE,
        PRINT,
        STATUSREQ,
        RESET,
        ERASE,
        UPDATE,
        PAUSE,
        SETCONFIG,
        MANUALMOVE,
        GCODE,
        CALIBRATION
    };
    //Payload content
    char payload[SIMQTTMESSAGE_MAX_LEN];
    MsgType type;

    //Constructor
    SIMQTTMessage() : type(NONE){};

    void setPayload(const char *newPayload)
    {
        int i;
        //Copy payload string
        for (i = 0; i < SIMQTTMESSAGE_MAX_LEN && newPayload[i] != 0; i++)
            payload[i] = newPayload[i];
        //Add string terminator
        payload[(i < SIMQTTMESSAGE_MAX_LEN ? i : SIMQTTMESSAGE_MAX_LEN - 1)] = 0;
    }

    SIMQTTMessage &operator=(SIMQTTMessage other)
    {
        //Copy payload
        setPayload(other.payload);
        //Copy type
        type = other.type;
        return *this;
    }
};
