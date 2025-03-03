#include "WiFi.h"
#include "Esp.h"
#include "WiFiServer.h"

#include "SIConfig.hpp"
#include "ScribIt.hpp"

void replyBadRequest(WiFiClient &client, const char *error, const uint8_t t_ID[6])
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:application/json");
    client.println("Access-Control-Allow-Origin: *");
    client.println("Access-Control-Allow-Methods: POST, GET, OPTIONS, DELETE");
    client.println("Access-Control-Max-Age: 86400");
    client.println("Access-Control-Allow-Headers: Authorization, accesstoken, Content-Type");
    client.println();
    client.printf("{ \"Error\":\" %s\", \"code\":\"%.2x%.2x%.2x%.2x%.2x%.2x\"}", error, t_ID[0], t_ID[1], t_ID[2], t_ID[3], t_ID[4], t_ID[5]);
    client.println();
    client.println();
    client.stop();
}

void replyOK(WiFiClient &client, const uint8_t t_ID[6])
{
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:application/json");
    client.println("Access-Control-Allow-Origin: *");
    client.println("Access-Control-Allow-Methods: POST, GET, OPTIONS, DELETE");
    client.println("Access-Control-Max-Age: 86400");
    client.println("Access-Control-Allow-Headers: Authorization, accesstoken, Content-Type");
    client.println();
    client.printf("{ \"code\":\"%.2x%.2x%.2x%.2x%.2x%.2x\"}", t_ID[0], t_ID[1], t_ID[2], t_ID[3], t_ID[4], t_ID[5]);
    client.println();
    client.stop();
}

bool ScribIt::connectToWiFi(const char *ssid, const char *pass)
{
    WiFi.begin(ssid, pass);
    uint8_t status = WiFi.waitForConnectResult();
    if (status == WL_CONNECTED)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool ScribIt::parseWifiConfigJSON(const char *buffer, char *ssid, char *pass)
{
    StaticJsonBuffer<JSON_MAX_LEN> jsonBuffer;
    JsonObject &root = jsonBuffer.parseObject(buffer);
    if (!root.success())
    {
        ssid[0] = 0;
        pass[0] = 0;
        return false;
    }

    //Set-up wifi
    const char *tempSsid = root["ssid"];
    const char *tempPass = root["password"];
    //Check parameters---------------------------
    if (tempSsid != nullptr)
    {
        //Copy string
        strcpy(ssid, tempSsid);
    }
    else
    {
        ssid[0] = 0;
    }

    if (tempPass != nullptr)
    {
        //Copy string
        strcpy(pass, tempPass);
    }

    return tempSsid != nullptr;
}

void ScribIt::configureWifi()
{
    char apSsid[16];
    
    //Create wifi server
    WiFiServer wifiServer(SI_WIFICONFIG_PORT);
    WiFiClient client;

    //Torn off wifi
    WiFi.disconnect(true,false);

#ifdef SI_DEBUG_ESP
    Serial.println("No saved WiFi");
    Serial.printf("Wifi status:%d\n", WiFi.status());
    Serial.println("Known wifi not found. AP created");
#endif
    sprintf(apSsid, SI_AP_SSID, m_ID[3], m_ID[4], m_ID[5]);
    //Set-up soft AP
    WiFi.softAP(apSsid, nullptr);

    wifiServer.begin();

#ifdef SI_DEBUG_ESP
    Serial.print("Wait for client\n");
#endif
//Pulse led
    leds.pulse(255, 255, 255, 1, 1);
    //Loop until wifi configured or gcode test requested
    while (!(m_testMode=isTestGcodeRequested()))
    {
        //Ask new client if none from before
        if (!client)
            client = wifiServer.available();

        //If any client
        if (client)
        {
            uint16_t len = 0;
            while (client.available() > 0)
            {
                String s = client.readStringUntil('\n');

                //Parse content length
                if (s.indexOf("Content-Length:") >= 0)
                {
                    len = s.substring(s.indexOf(" ") + 1).toInt();
#ifdef SI_DEBUG_ESP
                    Serial.printf("Content Len=%d\n", len);
#endif
                }
                //Header terminated
                if (s.length() <= 1)
                {
                    //Check body
                    if (len == 0)
                    {
                        replyBadRequest(client, "No body", m_ID);
                    }
                    else if (len > JSON_MAX_LEN)
                    {
                        replyBadRequest(client, "JSON too long", m_ID);
                    }
                    else
                    {
                        //Parse body---------------------------------
                        char buffer[len + 1] = {};
                        client.readBytes(buffer, len);
#ifdef SI_DEBUG_ESP
                        Serial.printf("Body: %s\n", buffer);
#endif
                        char ssid[32];
                        char pass[64];
                        bool status = parseWifiConfigJSON(buffer, ssid, pass);

                        if (!status) //If any parsing error
                        {
                            if (ssid[0] == 0)
                                replyBadRequest(client, "SSID missing", m_ID);
                            else if (pass[0] == 0)
                                replyBadRequest(client, "pwd missing", m_ID);
                            else
                                replyBadRequest(client, "JSON format error", m_ID);
                        }
                        else
                        {
                            //Set-up wifi

#ifdef SI_DEBUG_ESP
                            Serial.printf("Connecting to: %s : %s\n", ssid, pass);
                            Serial.print("IP: ");
                            Serial.println(WiFi.localIP());
#endif
                            replyOK(client, m_ID);
                            leds.doubleBlink(255, 255, 255, 0.5);

                            status = connectToWiFi(ssid, pass);
                            if (status)
                            {
#ifdef SI_DEBUG_ESP
                                Serial.println("Connected.");
#endif
                                leds.setColor(0, 255, 0);
                                delay(500);
                                //Kill AP
                                wifiServer.stop();
                                WiFi.softAPdisconnect();
                                //Restart ESP to avoid trouble
                                ESP.restart();
                                return;
                            }
                            else
                            {
#ifdef SI_DEBUG_ESP
                                Serial.println("Unable to connect to wi-fi.");
#endif
                                //Turn led red for 2 sec
                                leds.setColor(255, 0, 0);
                                delay(2000);
                            }

                            //Blink led
                            leds.pulse(255, 255, 255, 1, 1);
                        }
                    }
                }
            }
        }
    }
}
