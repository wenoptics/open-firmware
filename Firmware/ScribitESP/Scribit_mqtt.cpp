#include "ScribIt.hpp"
#include "SIMQTT.hpp"
#include <Update.h>
#define TAG "ScribIt_mqtt"
void ScribIt::evaluateMQTTIn()
{
    SIMQTTMessage msg;
    char tempTarget[SI_MQTT_MAX_PAYLOAD_LEN]; //Temporary target
    //Get MQTT message
    if (!SIMQTT.getNextMessage(&msg))
        return; //No new message

    switch (msg.type)
    {
    //Print/Erase===============================================================================================
    case SIMQTTMessage::PRINT:
    case SIMQTTMessage::ERASE:
        //If not idle send error
        if (m_state != SI_IDLE)
        {
            SIMQTT.error(String("Unable to print, printer in status: ") + m_state, SIMQTT_ERROR_CANNOT_PRINT);
        }
        else
        {
            //Parse print parameters
            if (!parsePETarget(msg.payload))
            {
                SIMQTT.error(String("Unable to parse print target: ") + msg.payload, SIMQTT_ERROR_MQTT);
                return;
            }
            //Parse print/erase
            m_isErase = (msg.type == SIMQTTMessage::ERASE);
            //Download target and start print
            downloadAndStart();
        }
        break;
    //======================================================================================================
    //Status req============================================================================================
    case SIMQTTMessage::STATUSREQ:
        SIMQTT.debug(TAG, String("Parsed status req"));
        if (m_state == SI_BOOT)  //If booting switch to IDLE
        { 
            setState(SI_IDLE);
            setSmartConfig(msg.payload);
        }
        else
            sendStatus();
        break;
    //=======================================================================================================
    //Update=================================================================================================
    case SIMQTTMessage::UPDATE:
        if (m_state != SI_BOOT)
        {
            SIMQTT.error("Update requested but device not in boot mode", SIMQTT_ERROR_OTA_NOT_RESET);
        }
        else
        {
            SIMQTT.debug(TAG, String("Updating..."));

            //Parse target
            sprintf(tempTarget, "%s", &(msg.payload[4]));
            //Check device
            if (strncmp(msg.payload, "ESP", 3) == 0)
            {
                SIMQTT.debug(TAG, String("ESP"));
                updateFW(U_FLASH, tempTarget);
            }
            else if (strncmp(msg.payload, "SAM", 3) == 0)
            {
                SIMQTT.debug(TAG, String("SAM"));
                updateFW(U_COMPANION, tempTarget);
            }
            else if (strncmp(msg.payload, "SPI", 3) == 0)
            {
                SIMQTT.debug(TAG, String("SPI"));
                updateFW(U_SPIFFS, tempTarget);
            }
            else
            {
                SIMQTT.error(String("Unknown device to update"), SIMQTT_ERROR_WRONG_DEVICE_IN_UPDATE);
            }
        }
        break;
    //==============================================================================================
    //Reset=========================================================================================
    case SIMQTTMessage::RESET:
        //Check hard reset
        if (msg.payload[0] == 'Y')
        {
            resetSamd(); //Reset SAMD
            delay(500);
            ESP.restart(); //Reset ESP
        }
        else
        {
            //Just stop stream
            sm.stopStream();
            m_target = "";

            if(m_isErase)
            {
                sm.addLineToStream("M104 S0");
            }

            //Send stop string if present
            if (m_sendOnStop.length() > 0)
                sm.addLineToStream(m_sendOnStop.c_str());
        }
        break;
    //==============================================================================================
    //Pause=========================================================================================
    case SIMQTTMessage::PAUSE:
        //Check printing
        if (m_state != SI_PRINTING && m_state != SI_ERASING)
            SIMQTT.error(String("Cannot pause in state ") + m_state, SIMQTT_ERROR_PAUSE_NOT_MOVING);
        else
        {
            PausedState ps = sm.getPausedState();
            //Check if command start pause
            if (msg.payload[0] == 'Y')
            {
                //If not running send error
                if (ps != SIPS_RUNNING)
                    SIMQTT.error(String("Pause already ") + (ps == SIPS_PAUSED ? "set" : "requested"), SIMQTT_ERROR_PAUSE_NOT_MOVING);
                else
                {
                    sm.setPause(true);
                    //Stop printing timer
                    m_printingTime += millis() - m_startPrintingT;
                }
            }
            else //Unpause command
            {
                if (ps == SIPS_RUNNING)
                {
                    SIMQTT.error(String("Not paused, cannot unpause"), SIMQTT_ERROR_PAUSE_NOT_MOVING);
                }
                else
                {
                    sm.setPause(false);
                    //Restart printing timer
                    m_startPrintingT = millis();
                }
            }

            //Send a status to notify pause/unpause
            sendStatus();
        }
        break;
#ifdef SI_DEBUG_BUILD
        //Single gcode=========================================================================================
    case SIMQTTMessage::GCODE:
        if (sm.addLineToStream(msg.payload))
            SIMQTT.debug(TAG, String("Written line: ") + msg.payload);
        else
        {
            SIMQTT.debug(TAG, String("Unable to add line: ") + msg.payload);
        }
        break;
#endif
        //====================================================================================================
        //Wifi Config=========================================================================================
    case SIMQTTMessage::SETCONFIG:
        if (m_state != SI_IDLE) //Check if idle
        {
            SIMQTT.error("Cannot config Wi-Fi if status not idle", SIMQTT_ERROR_STATUS_INCORRECT);
        }
        else
        {
            char ssid[32];
            char pass[64];
            bool status = parseWifiConfigJSON(msg.payload, ssid, pass);
            bool l_connectionResult = false;

            if (!status) //If error parsing
            {
                if (ssid[0] == 0 && pass[0] == 0)
                {
                    //Should reset config
                    SIMQTT.debug(TAG, "Payload has no data, resetting config and restarting");
                    resetConfig();
                }
                else
                {
                    SIMQTT.error(String("Malformed setWifiConfig payload: \"") + msg.payload + "\"", SIMQTT_ERROR_WIFICONFG_MALFORMED);
                }
            }
            else //Parameter correct
            {
                /**
                 * CHECK WORKING ON CONNECTION TO OPEN WIFI
                 */

                if(strcmp(pass, ""))
                {
                    l_connectionResult = connectToWiFi(ssid, nullptr);
                }
                else
                {
                    l_connectionResult = connectToWiFi(ssid, pass);
                }
                

                SIMQTT.debug(TAG, "Try to connect to wifi " + String(ssid) + " : " + String(pass));
                //Disconnect and delete current credentials
                WiFi.disconnect(false, true);

                if (l_connectionResult == false) //If connection fails
                {
                    SIMQTT.debug(TAG, "Connected to wifi " + String(ssid) + " : " + String(pass));
                }
                else
                {    
                    //Restart ESP
                    ESP.restart();                
                }
            }
        }

        break;
        //==================================================================================================
        //Manual gcode command==============================================================================
    case SIMQTTMessage::MANUALMOVE:
        //Check status IDLE
        if (m_state != SI_IDLE)
        {
            SIMQTT.error("Cannot move manually when not idle", SIMQTT_ERROR_STATUS_INCORRECT);
        }
        else
        {
            if (parseAndSaveGcodeString(msg.payload))
            {
                //Start file stream
                sm.streamLocalFile();
                //Set status to manual
                setState(SI_MANUAL);
            }
            else
            {
                SIMQTT.error("Error saving GCODE string", SIMQTT_ERROR_STATUS_INCORRECT);
            }
        }
        break;
        //==================================================================================================
        //Calibrate command ==============================================================================
    case SIMQTTMessage::CALIBRATION:
        if(m_state == SI_IDLE)
        {
            if(parseCalibPyaload(msg.payload))
            {
                m_calibrationAttempts = 0;
                m_printAfterCalibration = false;
                startCalibration();
            }
            else
            {
                SIMQTT.error("Error parsing calibration payload.", SIMQTT_ERROR_BAD_CALIBRATION_PAYLOAD);
            }
        }
        else
        {
            SIMQTT.error("Cannont calibrate when not IDLE", SIMQTT_ERROR_STATUS_INCORRECT);
        }

        break;
    default:
        SIMQTT.error("Unimplemented MQTT message", SIMQTT_ERROR_MQTT);
        break;
    }
}

bool ScribIt::parseCalibPyaload(const char *p_payload)
{
    bool l_retVal = false;
    uint8_t l_tmpWallId = 0;
    String l_tmp = String(p_payload);

    m_sendOnStop = l_tmp.substring(l_tmp.indexOf("G1"), l_tmp.indexOf(";"));
    
    if(m_sendOnStop.length() > 0)
    {
        l_tmpWallId = (uint8_t) l_tmp.substring(l_tmp.indexOf(";")+1).toInt();

        if(l_tmpWallId != 0 && l_tmpWallId != m_wallID)
        {
            m_wallID = l_tmpWallId;
        }

        l_retVal = true;
    }

#ifdef SI_DEBUG_BUILD
    SIMQTT.publish("calibDebug", "Parsing, send on stop is: " + m_sendOnStop +
                   " wall ID is :" + l_tmpWallId);
#endif

    return l_retVal;
}

bool ScribIt::setSmartConfig(const char *p_payload)
{
    bool l_retVal = false;

    StaticJsonBuffer<JSON_MAX_LEN> l_jsonBuffer;
    JsonObject &l_root = l_jsonBuffer.parseObject(p_payload);

#ifdef SI_DEBUG_BUILD
    SIMQTT.publish("smartFeature", String("Received payload is: ") + String(p_payload));
#endif

    if(l_root.success())
    {
        if(l_root.containsKey("ps"))
        {
            sm.setPenSensitivity(l_root["ps"].as<bool>());
        }

        if(l_root.containsKey("sc"))
        {
            sm.setSmartCylinder(l_root["sc"].as<bool>());
        }

        l_retVal = true;
    }

    return l_retVal;
}

bool ScribIt::parsePETarget(char *payload)
{
    uint8_t wallID;
    char *p = strtok(payload, ";"), *eon;

    //Target---------------------------------
    if (p == nullptr)
    {
        return false;
    }

    m_target = String(p);
    
    //Send on stop command--------------------
    p = strtok(NULL, ";");
    if (p == nullptr)
    {
         return false; //No parameter
    }   

    m_sendOnStop = String(p);

    return true;
}

void ScribIt::setDisconnected(bool isDisconnected)
{
    static SI_State last = SI_IDLE;
    if (isDisconnected && m_state != SI_DISCONNECTED)
    {
        //Save state before disconnection
        last = m_state;
        //Signal disconnected
        setState(SI_DISCONNECTED);
    }
    else if (!isDisconnected && m_state == SI_DISCONNECTED)
    {
        //Signal reconnection
        setState(last);
        SIMQTT.debug(TAG, String("Connected, state:") + last);
    }
}
