#include "SIMQTT.hpp"
#include "SPIFFS.h"
#include "SIConfig.hpp"

#define SIMQTT_TOPIC_MAX_LEN 128
#define SIMQTT_CERTIFICATE "/ca_bundle.crt"

#define TAG "SIMQTT"

const char *ca_cert = "-----BEGIN CERTIFICATE-----\n"
                      "MIIEkjCCA3qgAwIBAgIQCgFBQgAAAVOFc2oLheynCDANBgkqhkiG9w0BAQsFADA/\n"
                      "MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n"
                      "DkRTVCBSb290IENBIFgzMB4XDTE2MDMxNzE2NDA0NloXDTIxMDMxNzE2NDA0Nlow\n"
                      "SjELMAkGA1UEBhMCVVMxFjAUBgNVBAoTDUxldCdzIEVuY3J5cHQxIzAhBgNVBAMT\n"
                      "GkxldCdzIEVuY3J5cHQgQXV0aG9yaXR5IFgzMIIBIjANBgkqhkiG9w0BAQEFAAOC\n"
                      "AQ8AMIIBCgKCAQEAnNMM8FrlLke3cl03g7NoYzDq1zUmGSXhvb418XCSL7e4S0EF\n"
                      "q6meNQhY7LEqxGiHC6PjdeTm86dicbp5gWAf15Gan/PQeGdxyGkOlZHP/uaZ6WA8\n"
                      "SMx+yk13EiSdRxta67nsHjcAHJyse6cF6s5K671B5TaYucv9bTyWaN8jKkKQDIZ0\n"
                      "Z8h/pZq4UmEUEz9l6YKHy9v6Dlb2honzhT+Xhq+w3Brvaw2VFn3EK6BlspkENnWA\n"
                      "a6xK8xuQSXgvopZPKiAlKQTGdMDQMc2PMTiVFrqoM7hD8bEfwzB/onkxEz0tNvjj\n"
                      "/PIzark5McWvxI0NHWQWM6r6hCm21AvA2H3DkwIDAQABo4IBfTCCAXkwEgYDVR0T\n"
                      "AQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwfwYIKwYBBQUHAQEEczBxMDIG\n"
                      "CCsGAQUFBzABhiZodHRwOi8vaXNyZy50cnVzdGlkLm9jc3AuaWRlbnRydXN0LmNv\n"
                      "bTA7BggrBgEFBQcwAoYvaHR0cDovL2FwcHMuaWRlbnRydXN0LmNvbS9yb290cy9k\n"
                      "c3Ryb290Y2F4My5wN2MwHwYDVR0jBBgwFoAUxKexpHsscfrb4UuQdf/EFWCFiRAw\n"
                      "VAYDVR0gBE0wSzAIBgZngQwBAgEwPwYLKwYBBAGC3xMBAQEwMDAuBggrBgEFBQcC\n"
                      "ARYiaHR0cDovL2Nwcy5yb290LXgxLmxldHNlbmNyeXB0Lm9yZzA8BgNVHR8ENTAz\n"
                      "MDGgL6AthitodHRwOi8vY3JsLmlkZW50cnVzdC5jb20vRFNUUk9PVENBWDNDUkwu\n"
                      "Y3JsMB0GA1UdDgQWBBSoSmpjBH3duubRObemRWXv86jsoTANBgkqhkiG9w0BAQsF\n"
                      "AAOCAQEA3TPXEfNjWDjdGBX7CVW+dla5cEilaUcne8IkCJLxWh9KEik3JHRRHGJo\n"
                      "uM2VcGfl96S8TihRzZvoroed6ti6WqEBmtzw3Wodatg+VyOeph4EYpr/1wXKtx8/\n"
                      "wApIvJSwtmVi4MFU5aMqrSDE6ea73Mj2tcMyo5jMd6jmeWUHK8so/joWUoHOUgwu\n"
                      "X4Po1QYz+3dszkDqMp4fklxBwXRsW10KXzPMTZ+sOPAveyxindmjkW8lGy+QsRlG\n"
                      "PfZ+G6Z6h7mjem0Y+iWlkYcV4PIWL1iwBi8saCbGS5jN2p8M+X+Q7UNKEkROb3N6\n"
                      "KOqkqm57TH2H3eDJAkSnh6/DNFu0Qg==\n"
                      "-----END CERTIFICATE-----\n";


int SIMQTTClass::begin(uint8_t t_ID[6], bool skip)
{
    //Store ID
    for (int i = 0; i < 6; i++)
        m_ID[i] = t_ID[i];

    //Set certificate
    net.setCACert(ca_cert);
    client.begin(SI_MQTT_HOST, SI_MQTT_PORT, net);
    bool status = connect();
    m_initialized = true;

    setCallback(messageReceived);

    return status ? 0 : -1;
}

SIMQTTClass::SIMQTTState SIMQTTClass::loop()
{
    bool results = true;
    if (!m_initialized)
        return NOT_INITIALIZED;
    //Loop MQTT client
    if (!client.loop())
    {
        //In case of error connect and loop again
        results = connect();
        if (results)
            client.loop();
    }
    delay(10);

    //Retrun status
    return (results ? SIMQTTClass::OK : SIMQTTClass::DISCONNECTED);
}

void SIMQTTClass::publish(String topic, String payload)
{
    if (!m_initialized)
        return;

    char fullTopic[SIMQTT_TOPIC_MAX_LEN];

    //Create full topic
    makeTopicString(fullTopic, topic.c_str());

    //If too long divide in two
    if (payload.length() > SI_MQTT_MAX_PAYLOAD_LEN)
    {
        client.publish(fullTopic, payload.substring(0, SI_MQTT_MAX_PAYLOAD_LEN - 1));
        client.publish(fullTopic, payload.substring(SI_MQTT_MAX_PAYLOAD_LEN - 1));
    }
    else
    {
        //Publish
        client.publish(fullTopic, payload);
    }
#ifdef SI_DEBUG_ESP
    Serial.printf("Published on %s :%s\n", topic.c_str(), payload.c_str());
#endif
}

void SIMQTTClass::error(String message, int code)
{
    String payload = String("{ \"Code\":") + code + ", \"Description\":\"" + message + "\"}";
    publish("error", payload);
}

void SIMQTTClass::debug(String tag, String message, int level)
{
#ifndef SI_DEBUG_BUILD
    //Do nothing if SI_DEBUG_BULD is disabled
    return;
#endif

    if (level == 9)
    {
        publish("serialecho", message);
    }
    else
    {
        publish("debug", tag + ": " + message);
    }
}

void SIMQTTClass::statusPrintErase(uint32_t elapsedTimeSec, bool isErase, char isPaused)
{
    //Create payload
    String payload = String("{\"ET\":") + elapsedTimeSec + ", \"Paused\":\"" + (isPaused) + "\"}";
    publish((isErase) ? "erasing" : "printing", payload);
}

void SIMQTTClass::statusIdle(int8_t RSSI, float temp)
{
    //Publish message
    publish("idle", String("{\"RSSI\":") + RSSI + ", \"Temp\":" + temp + "}");
}

bool SIMQTTClass::connect()
{
    static uint32_t lastConnected = millis(); //Last time was connected
    char localName[32];

    //Skip if already connected
    if (client.connected())
    {
        lastConnected = millis();
        return true;
    }
    //Check WiFi status
    bool wifiConnected = WiFi.isConnected();

    //Evaluate local name
    sprintf(localName, SI_MQTT_LOCALNAME_FORMAT, m_ID[3], m_ID[4], m_ID[5]);
    //Connect
    if (wifiConnected && client.connect(localName, SI_MQTT_USER, SI_MQTT_PASS))
    {
        //Subscribe to all topics
        subscribe("#");
        //Save connected time
        lastConnected = millis();

        //Signal connection success
        return true;
    }
    else
    {
        //If last time connected too old reset WiFi
        if (millis() - lastConnected > SI_MQTT_RESET_WIFI_TIMEOUT_MS)
        {
            //Turn off device
            WiFi.disconnect(true, false);
            //Save time to avoid frequent reset
            lastConnected = millis();
            delay(100);
            //Restart WiFi
            WiFi.begin();
        }
        //Signal connection fail
        return false;
    }
}

void SIMQTTClass::success(String operation)
{
    String payload = String("{ \"Op\": \"") + (operation) + "\"}";
    publish("success", payload);
}

void SIMQTTClass::boot(uint16_t espVer, uint16_t samdVer, uint16_t spiffsVer)
{
    String payload = String("{ \"SAM\":") + String((long)samdVer) + ", \"ESP\":" + String((long)espVer) + ",\"SPI\":" + spiffsVer + ",\"RSSI\":" + WiFi.RSSI() + "}";
    publish("boot", payload);
}


void SIMQTTClass::makeTopicString(char *outTopic, const char *topicName, bool isOutTopic)
{
    //Make topic string
    sprintf(outTopic, isOutTopic ? SIMQTT_TOPIC_FORMAT_OUT : SIMQTT_TOPIC_FORMAT_IN, m_ID[0], m_ID[1], m_ID[2], m_ID[3], m_ID[4], m_ID[5], topicName);
}

void SIMQTTClass::setCallback(MQTTClientCallbackSimple cb)
{
    client.onMessage(cb);
    m_cb = cb;
}

void SIMQTTClass::subscribe(const char *topic)
{
    char buffer[SIMQTT_TOPIC_MAX_LEN];
    makeTopicString(buffer, topic, false);
    if (!client.subscribe(buffer))
    {
        SIMQTT.error(String("Unable to subscribe to ") + buffer, SIMQTT_ERROR_MQTT);
    }
}

bool SIMQTTClass::getNextMessage(SIMQTTMessage *msg)
{
    if (mqttMessageBuffer.empty())
        return false;

    mqttMessageBuffer.remove(msg);
    return true;
}
//=======================================================================
// Callback
void SIMQTTClass::messageReceived(String &topic, String &payload)
{
    SIMQTTMessage msg;

    SIMQTT.debug(TAG, String("incoming: ") + topic + " - " + payload);

    //Print--------------------------------------------------
    if (topic.endsWith(SI_MQTT_TOPIC_IN_PRINT))
    {
        msg.type = SIMQTTMessage::PRINT;
    }
    //Erase-------------------------------------------------
    else if (topic.endsWith(SI_MQTT_TOPIC_IN_ERASE))
    {
#ifdef SI_DEBUG_BUILD
        SIMQTT.debug(TAG, "Received erase " + payload);
#endif
        msg.type = SIMQTTMessage::ERASE;
    }
    //Status req--------------------------------------------
    else if (topic.endsWith(SI_MQTT_TOPIC_IN_STATUS))
    {
#ifdef SI_DEBUG_BUILD
        SIMQTT.debug(TAG, "Received status req ");
#endif
        msg.type = SIMQTTMessage::STATUSREQ;
    }
    //Update-------------------------------------------------
    else if (topic.endsWith(SI_MQTT_TOPIC_IN_UPDATE))
    {
        SIMQTT.debug(TAG, "Received update " + payload);
        msg.type = SIMQTTMessage::UPDATE;
    }
    //Reset-------------------------------------------------
    else if (topic.endsWith(SI_MQTT_TOPIC_IN_RESET))
    {
        SIMQTT.debug(TAG, "Received reset");
        msg.type = SIMQTTMessage::RESET;
    }
    //Pause--------------------------------------------------------------
    else if (topic.endsWith(SI_MQTT_TOPIC_IN_PAUSE))
    {
        msg.type = SIMQTTMessage::PAUSE;
    }
#ifdef SI_DEBUG_BUILD
    //Single GCODE command------------------------------------------------
    else if (topic.endsWith("GCODE"))
    {
        msg.type = SIMQTTMessage::GCODE;
        SIMQTT.debug(TAG, "Received gcode: " + payload);
    }
#endif
    //SetConfig-----------------------------------------------------------
    else if (topic.endsWith(SI_MQTT_TOPIC_IN_SETCONFIG))
    {
        msg.type = SIMQTTMessage::SETCONFIG;
        SIMQTT.debug(TAG, "Received wifi config: " + payload);
        //wifiConfigString = payload;
    }
    //ManualMove----------------------------------------------------------
    else if (topic.endsWith(SI_MQTT_TOPIC_IN_MANUALMOVE))
    {
        SIMQTT.debug(TAG, "Received manual movements: " + payload);
        msg.type = SIMQTTMessage::MANUALMOVE;
    }
    else if (topic.endsWith(SI_MQTT_TOPIC_IN_CALIBRATION))
    {
        SIMQTT.debug(TAG, "Received calibration: " + payload);
        msg.type = SIMQTTMessage::CALIBRATION;
    }
    //Unknown------------------------------------------------------------
    else
    {
        SIMQTT.debug(TAG, String("Unknown topic: ") + topic + " - " + payload);
        //Do not add message to buffer
        return;
    }

    //Copy payload
    msg.setPayload(payload.c_str());
    //Check buffer full
    if (SIMQTT.mqttMessageBuffer.full())
    {
        SIMQTT.error("MQTT message buffer full, please wait before resend", SIMQTT_ERROR_MESSAGE_BUFFER_FULL);
    }
    else
    {
        //Add message to buffer
        SIMQTT.mqttMessageBuffer.add(msg);
    }
}

//===================================================================
SIMQTTClass SIMQTT;
