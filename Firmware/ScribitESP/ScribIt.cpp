#include "WiFi.h"
#include "Esp.h"
#include "WiFiServer.h"
#include "ArduinoJson.h"
#include <SPIFFS.h>

#include "ScribIt.hpp"
#include "SIConfig.hpp"
#include "SIMQTT.hpp"
#include "SIPins.hpp"

#define TAG "ScribIt"

const char SIPS_PKT_ARRAY[] = "NRY";
#define SIPS_TO_PKT(A) (SIPS_PKT_ARRAY[A])

/******************************
 *  LED Thread
 * 
 *******************************/
void LEDthread(void *pvParameters)
{
    RGBLEDs *leds = static_cast<RGBLEDs *>(pvParameters);
    while (true)
    {
        leds->update();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

/******************************
 * Button interrupt
 * 
 ******************************/
volatile uint32_t buttonRequestedReset = 0;
void IRAM_ATTR buttonISR()
{
    if (buttonRequestedReset == 0)
        buttonRequestedReset = millis();
}

void ScribIt::processButtonInterrupt()
{
    //Process button interrupt
    if (buttonRequestedReset != 0 && millis() - buttonRequestedReset > SI_BUTTON_RESET_TIME_MS)
    {
        //Check if button still pressed
        if (digitalRead(PIN_SWITCH) == LOW)
        {
            leds.doubleBlink(0, 0, 255, 0.5);
            resetConfig();
        }
        else
        {
            //If released reset starting time
            buttonRequestedReset = 0;
        }
    }
}

void ScribIt::begin()
{
    bool syncd = false;
    //Set microstepping pin
    setStepperStepPin();
    //Detach LED Thread------------------
    //create a task that will be executed pinned on the other core
    xTaskCreatePinnedToCore(
        LEDthread,        /* Task function. */
        "LEDthread",      /* name of task. */
        10000,            /* Stack size of task */
        &leds,            /* parameter of the task */
        1,                /* priority of the task */
        &LEDThreadHandle, /* Task handle to keep track of created task */
        0);               //Run on the core 0
    //Init Serial------------------------------
    Serial.begin(SAMD_SERIAL_BAUDRATE);
    Serial.setDebugOutput(false);
    Serial.setTimeout(SAMD_SERIAL_TIMEOUT_MS);
#ifdef SI_DEBUG_ESP
    Serial.println("~~~~WARNING~~~~~\nESP DEBUG Active");
#endif

    //Init reset button
    pinMode(PIN_SWITCH, INPUT);
    attachInterrupt(PIN_SWITCH, buttonISR, FALLING);

    //Turn led blue
    leds.blink(0, 0, 255, 2);

    //Load MAC Address-------------------------
    if (esp_efuse_mac_get_default(m_ID) != ESP_OK)
    {
#ifdef SI_DEBUG_ESP
        Serial.println("Unable to read MAC address");
#endif
    }

    //Init SFPIFFS----------------------------
    if (!SPIFFS.begin(true)) //If fails (Try to format)
    {
#ifdef SI_DEBUG_ESP
        Serial.println("SPIFFS Mount failed");
#endif
    }
    //Get SPIFFS version if available
    if (SPIFFS.exists("/FwVer"))
    {
        File in = SPIFFS.open("/FwVer");
        if (in)
        {
            String ver = in.readString();
            m_spiffsVer = ver.toInt();
            in.close();
        }
    }

    //Wifi config---------------------------
    WiFi.begin();
    if (WiFi.waitForConnectResult() == WL_CONNECTED)
    {
#ifdef SI_DEBUG_ESP
        Serial.printf("Connected to wifi \"%s\", RSSI:%d dB IP:", WiFi.SSID().c_str(), WiFi.RSSI());
        Serial.println(WiFi.localIP());
#endif
    }
    else
    {
        configureWifi();
    }

    if (!m_testMode) //Skip if test mode
    {
        leds.doubleBlink(255, 255, 255, 0.5);
        //Init MQTT----------------------------
        SIMQTT.begin(m_ID);

        SIMQTT.debug(TAG, "MQTT Started");
    }

    //Init serial manager
    if (sm.begin() != 0)
    {
        errorCode = SIMQTT_ERROR_CANNOT_SYNC_MK4DUO;
        errorMessage = "Unable to load SerialManager";
        SIMQTT.error(errorMessage, errorCode);
        setState(SI_ERROR);
    }
    else
    {
        //Wait for printer---------------------------------------------------
        //Try 5 time
        for (uint8_t i = 0; i < 5 && !syncd; i++)
        {
            syncd = syncWithSamd();
            if (!syncd)
            {
                SIMQTT.debug(TAG, "Resetting Samd...");
                resetSamd();
                delay(500);
            }
        }
        if (!syncd)
        {
            errorCode = SIMQTT_ERROR_CANNOT_SYNC_MK4DUO;
            errorMessage = "Sync with Mk4Duo failed";
            SIMQTT.error(errorMessage, errorCode);
            setState(SI_ERROR);
        }
        else if (!m_testMode)
        {
            SIMQTT.boot(SI_ESP_FIRMWARE_VERSION, m_samdVer, m_spiffsVer);
            //Set state to boot
            setState(SI_BOOT);
        }
    }

    //Wait for go message (Status req)-------------------------------
    uint32_t startTime = millis();
    //Loop until state req received or always if in error
    while (m_state == SI_BOOT || m_state == SI_ERROR)
    {
        //If timeout elapsed send boot message again
        if (m_state == SI_BOOT && millis() - startTime > BOOT_MESSAGE_RESEND_TIMEOUT_MS)
        {
            SIMQTT.boot(SI_ESP_FIRMWARE_VERSION, m_samdVer, m_spiffsVer);
            startTime = millis();
        }
        //Check for MQTT messages
        SIMQTT.loop();
        evaluateMQTTIn();
        //Check for reset button
        processButtonInterrupt();
        delay(10);
    }
    m_lastStatusSentT = millis(); //Reset last send status

    //If test mode start GCODE test
    if (m_testMode)
    {
        if (saveGcodeStringInFile(SI_TEST_GCODE_STRING))
        {
            setState(SI_PRINTING);
            //Stream local file
            sm.streamLocalFile();
        }
        else
            setState(SI_ERROR);
    }
    else
    {
        //Ask for initial temperature
        sm.addLineToStream("M105");
        //Ask for inertial data to check IMU
        sm.addLineToStream("M777");
    }
}

void ScribIt::loop()
{
    int16_t imuData;
#ifdef SI_DEBUG_BUILD
    static uint32_t lastSend = millis();
#endif
    SIMKOperation mkstatus;

    if (!m_testMode)
    {
        //Process MQTT
        SIMQTTClass::SIMQTTState mqttState = SIMQTT.loop();
        //Check connection status
        if (mqttState == SIMQTTClass::DISCONNECTED)
            setDisconnected(true);
        else if (mqttState == SIMQTTClass::OK)
            setDisconnected(false);

        //Check for reset button
        processButtonInterrupt();

        //Evaluate MQTT requests
        evaluateMQTTIn();

        //Send status message if timeout elapsed
        if (millis() - m_lastStatusSentT > SI_MQTT_SEND_STATUS_TIMEOUT_MS)
        {
            sendStatus();
            //If idle send disable motors
            if (m_state == SI_IDLE)
            {
                //Disable steppers
                sm.addLineToStream("M18");
                //Ask for temperature
                sm.addLineToStream("M105");
            }
        }
    }

    //Check print/erase end
    if (m_state == SI_PRINTING || m_state == SI_ERASING || m_state == SI_MANUAL)
    {
        //Check for end of streaming
        if (sm.isStreamEnded())
        {
            if(hasNexLink())
            {
                m_target = m_nextTarget;
                m_nextTarget = "";

                downloadAndStart(false);
            }
            else
            {
                setState(SI_IDLE); //Go to idle   
            }
        }
    }
    else if (m_state == SI_CALIBRATING)
    {
        //Check for end of streaming
        if (sm.isStreamEnded())
        {
            if (completeCalibration())
            {
                SIMQTT.publish("calibrating", "{\"status\":0}");
                leds.setColor(0, 255, 0);
                delay(2500);
                setState(SI_IDLE);
            }
            else
            {
                //setState(SI_IDLE);
                if(m_calibrationAttempts < CALIBRATION_ATTEMPTS_LIMIT)
                {
#ifdef SI_DEBUG_BUILD
                    SIMQTT.publish("calibDebug","Reatempting calibration.");
#endif
                    startCalibration();
                    m_calibrationAttempts++;
                }
                else
                {
                    SIMQTT.publish("calibrating", "{\"status\":1}");
                    leds.setColor(255, 0, 0);
                    delay(5000);
                    setState(SI_IDLE);
                }
            }
        }
    }

    //Execute serial manager and get current status
    mkstatus = sm.loop();

    //Check Heating/Erasing
    if (m_state == SI_ERASING && mkstatus == SIMK_HEATING)
    {
        setState(SI_HEATING);
    }
    else if (m_state == SI_HEATING && mkstatus != SIMK_HEATING)
    {
        setState(SI_ERASING);
    }

    //Get new imu data if available
    if (sm.getIMUData(imuData))
    {
        SIMQTT.debug(TAG, String("New imu data available ( ") + imuData + " )");
        m_imuData.add(imuData);
    }
}

void ScribIt::resetConfig()
{
#ifdef SI_DEBUG_ESP
    //Serial.println("Resetting and rebooting...");
    delay(3000);
#endif
    SIMQTT.debug(TAG, "Resetting and rebooting");
    WiFi.disconnect(true, true);
    ESP.restart();
}

void ScribIt::setState(const SI_State newState)
{
    //Skip same state transition
    if (newState == m_state)
        return;

    //Never exit from fatal error status
    if (m_state == SI_ERROR)
        return;

#ifdef SI_DEBUG_BUILD
    SIMQTT.debug(TAG, String("Changing status from ") + String((int)m_state) + " to " + String((int)newState));
#endif

    //Save time if Starting printing from idle
    if (m_state == SI_CALIBRATING && (newState == SI_PRINTING || newState == SI_ERASING))
    {
        m_startPrintingT = millis();
        m_printingTime = 0;
    }
    //Send success if going back IDLE from printing/erasing
    else if (newState == SI_IDLE)
    {
        switch (m_state)
        {
        case SI_PRINTING:
            SIMQTT.success("printing");
            break;
        case SI_ERASING:
            SIMQTT.success("erasing");
            break;
        case SI_MANUAL:
            SIMQTT.success("manualMove");
            break;
        default:
            break;
        }
    }

    //Update leds---------------------------
    switch (newState)
    {
    case SI_IDLE:
        if (!m_testMode)
        {
            leds.setColor(255, 255, 255);
        }
        else
        {
            leds.blink(0, 255, 0, 0.1);
        }

        break;
    case SI_PRINTING:
        if (!m_testMode)
        {
            leds.rainbow(18);
        }
        else
        {
            //Set led rose
            leds.setColor(244, 66, 235);
        }
        break;
    case SI_ERASING:
        leds.setColor(255, 255, 0); // SUPPOSED TO BE ORANGE NEAR TO YELLOW
        break;
    case SI_HEATING:
        leds.pulse(255, 255, 0, 1, 1);
        break;
    case SI_ERROR:
        leds.blink(255, 0, 0, 0.5);
        break;
    case SI_MANUAL:
        leds.blink(0, 255, 255, 0.5);
        break;
    case SI_DISCONNECTED:
        leds.blink(255, 0, 0, 0.5);
    case SI_CALIBRATING:
        leds.blink(0, 255, 255, 2);
    default:
        break;
    }

    //Update state
    m_state = newState;
}

void ScribIt::sendStatus()
{
    if (m_state == SI_PRINTING || m_state == SI_ERASING || m_state == SI_HEATING)
    {
        //Get printing time
        uint32_t et = m_printingTime;
        //If not paused add timer time
        if (sm.getPausedState() == SIPS_RUNNING)
            et += (millis() - m_startPrintingT);
        SIMQTT.statusPrintErase(et / 1000, (m_state == SI_ERASING || m_state == SI_HEATING), SIPS_TO_PKT(sm.getPausedState()));
    }
    else if (m_state == SI_IDLE)
        SIMQTT.statusIdle(WiFi.RSSI(), sm.getTemperature());
    else if (m_state == SI_ERROR)
        SIMQTT.error(errorMessage, errorCode);
    else if (m_state == SI_MANUAL)
        //SIMQTT.debug(TAG, "Manual status to be defined");
        SIMQTT.publish("manualMove", "{}");
    else if (m_state == SI_CALIBRATING)
        //SIMQTT.debug(TAG, "Manual status to be defined");
        SIMQTT.publish("calibrating", "{}");

    m_lastStatusSentT = millis(); //Reset last send status
}

bool ScribIt::syncWithSamd()
{
    //Search for fw line
    char samdSerialBuffer[128], *fwp = nullptr;
    int len;
    uint32_t startT = millis();
    do
    {
        //Check timeout
        if (millis() - startT > SI_SAMD_SYNC_TIMEOUT)
        {
            SIMQTT.debug(TAG, "Timeout while waiting for SAMD firmware");
            return false;
        }
        len = Serial.readBytesUntil(SM_PRINTER_ENDLINE, samdSerialBuffer, 128);
        SIMQTT.loop();
        //Find SCRIBITFW
        fwp = strstr(samdSerialBuffer, "SCRIBITFW:");
    } while (fwp == nullptr);

    //Add string terminator
    samdSerialBuffer[len] = 0;

    uint16_t fwVer = atoll(strstr(fwp, ":") + 1); //atoll(&samdSerialBuffer[10]);
    SIMQTT.debug(TAG, String("Received: ") + samdSerialBuffer);

    //If firmware version error fail
    if (fwVer <= 0)
    {
        SIMQTT.debug(TAG, String("Samd firmware error:") + fwVer);
        return false;
    }

    //Save firmware version
    m_samdVer = fwVer;

    SIMQTT.debug("SyncSamd", String("Received FW Version: ") + fwVer);

    //Reply with start
    Serial.println("SCRIBITSTART");

    //Wait for wait
    startT = millis(); //Reset timeout
    do
    {
        //Check timeout
        if (millis() - startT > SI_SAMD_SYNC_TIMEOUT)
        {
            SIMQTT.debug(TAG, "Timeout while waiting for MK4DUo wait");
            return false;
        }
        len = Serial.readBytesUntil(SM_PRINTER_ENDLINE, samdSerialBuffer, 128);
        SIMQTT.loop();
    } while (strstr(samdSerialBuffer, "wait") == nullptr);

    return true;
}

void ScribIt::resetSamd()
{
    //Init pin
    pinMode(COMPANION_RESET, OUTPUT);
    digitalWrite(COMPANION_RESET, LOW);
    delay(50);
    digitalWrite(COMPANION_RESET, HIGH);
    //Deinit Pin
    pinMode(COMPANION_RESET, INPUT);
}

void ScribIt::setStepperStepPin()
{
    //Half step
    pinMode(PIN_MS1, OUTPUT);
    pinMode(PIN_MS2, OUTPUT);
    pinMode(PIN_MS3, OUTPUT);

    // half stepping setting
    digitalWrite(PIN_MS1, HIGH);
    digitalWrite(PIN_MS2, LOW);
    digitalWrite(PIN_MS3, LOW);
}

bool ScribIt::isTestGcodeRequested()
{
    static uint8_t buttonPressed = 0;
    static uint32_t firstButtonPress = 0;
    static uint32_t lastButtonPress = 0;

    //If button pressed
    if (buttonRequestedReset != 0)
    {
        //Debounce 200 ms
        if (millis() - lastButtonPress < 200)
        {
#ifdef SI_DEBUG_ESP
            Serial.println("Debounced button");
#endif
            //Reset button press
            buttonRequestedReset = 0;
            return false;
        }
        //First time button pressed or timeouted save time
        if (firstButtonPress == 0 || millis() - firstButtonPress > GCODE_TEST_BUTTON_PRESS_WINDOWS_MS)
        {
            //Save time button was pressed
            firstButtonPress = buttonRequestedReset;
            //Reset counter
            buttonPressed = 0;
#ifdef SI_DEBUG_ESP
            Serial.println("First button press");
#endif
        }
#ifdef SI_DEBUG_ESP
        else
        {
            Serial.printf("%d button press\n", buttonPressed + 1);
        }
#endif
        //Save time for debouncing
        lastButtonPress = buttonRequestedReset;
        //Reset button interrupt
        buttonRequestedReset = 0;
        //Increase counter
        buttonPressed++;

        //Button pressed 3 times
        if (buttonPressed >= 3)
        {
#ifdef SI_DEBUG_ESP
            Serial.println("Starting test");
#endif
            //Set led rose for feedback
            leds.setColor(244, 66, 235);
            return true;
        }
    }
    return false;
}

bool ScribIt::parseAndSaveGcodeString(char *gcode)
{
    //Delete old temp file
    if (!SPIFFS.remove(SI_TEMPORARY_GCODE_PATH))
    {
        //Not fatal might happen if no file existing
        SIMQTT.debug(TAG, String("Unable to remove file ") + SI_TEMPORARY_GCODE_PATH);
    }
    //Open file
    File file = SPIFFS.open(SI_TEMPORARY_GCODE_PATH, FILE_WRITE);
    if (!file)
    {
        SIMQTT.error("Unable to open file to store manual gcode", SIMQTT_ERROR_DOWNLOAD_FILE_IO_ERROR);
        return false;
    }
    else
    {
        //Parse string and save
        for (uint16_t i = 0; gcode[i] != 0 && i < SI_MQTT_MAX_PAYLOAD_LEN; i++)
        {
            //If command ended end line
            if (gcode[i] == ';')
                file.println();
            else //Just write byte
                file.write(gcode[i]);
        }
        //Terminate file with endline
        file.println();
        //Close file
        file.close();
    }

    return true;
}

bool ScribIt::downloadAndStart(bool p_showDownloadLeds)
{
    //Send download start message
    SIMQTT.publish("download", String("{\"Status\":\"Start\"}"));

    //Set download led
    if(m_isErase)
    {
	    if(p_showDownloadLeds)
        {
            	leds.doubleBlink(255, 255, 0, 0.5);
	    }
    }
    else
    {
	    if(p_showDownloadLeds)
        {
            	leds.doubleBlink(255, 255, 255, 0.5);
	    }
    }
    
#ifdef SI_DEBUG_BUILD
    uint32_t downloadStartT = millis();
#endif
    //Download file
    bool status = downloader.download(m_target, false);
#ifdef SI_DEBUG_BUILD
    SIMQTT.debug(TAG, String("Download took ") + ((millis() - downloadStartT) / 1000) + " sec");
#endif
    if (status)
    {
        SIMQTT.debug(TAG, String("Starting streaming of") + m_target);
        //Start streaming of cached file
        sm.streamLocalFile();
        //Set state accordingly
        setState(m_isErase ? SI_ERASING : SI_PRINTING);
    }
    else
    {
#ifdef SI_DEBUG_BUILD
        SIMQTT.publish("download","returning false");
#endif
        return false;
    }

#ifdef SI_DEBUG_BUILD
    SIMQTT.publish("download","returning true");
#endif
    return true;
}

void ScribIt::startCalibration()
{
    //Check IMU
    if (!sm.isIMUWorking())
    {
        SIMQTT.error("IMU hardware error", SIMQTT_ERROR_CANNOT_CALIBRATE);
        return;
    }

    //Flush inertial data
    m_imuData.flush();
    //Delete old temp file to avoid space issue
    if (!SPIFFS.remove(SI_TEMPORARY_GCODE_PATH))
    {
        SIMQTT.debug(TAG, String("Unable to remove file ") + SI_TEMPORARY_GCODE_PATH);
    }
    //Save correct gcode in file
    //if (!saveGcodeStringInFile(SICalibrationGcodes[m_wallID - 1], SI_CALIBRATION_GCODE_FILE))
    if (!downloader.download(SI_CALIBRATION_GCODE_URL))
    {
        SIMQTT.error("Unable to save calibration GCODE", SIMQTT_ERROR_CANNOT_CALIBRATE);
        return;
    }
    if (!sm.streamLocalFile(SI_TEMPORARY_GCODE_PATH))
    {
        SIMQTT.error("Unable to start stream of calibration Gcode", SIMQTT_ERROR_CANNOT_CALIBRATE);
        return;
    }
    //Start calibration
    setState(SI_CALIBRATING);
}

bool ScribIt::saveGcodeStringInFile(String Gcode, String fileName)
{
    //Delete old temp file
    SPIFFS.remove(fileName);
    //Open file
    File file = SPIFFS.open(fileName, FILE_WRITE);
    if (!file)
    {
        //Serial.println("Unable to remove file");
        SIMQTT.error("Unable to open file to store manual gcode", SIMQTT_ERROR_DOWNLOAD_FILE_IO_ERROR);
        return false;
    }
    else
    {
        //Serial.println("Creating file");
        file.println(Gcode);
        file.close();

        return true;
    }
}

bool ScribIt::completeCalibration()
{
    int16_t angles[SI_CALIBRATION_POINT_NUMBER];
    String startPosition;
    bool l_retVal = false;
    //Get inertial data
    m_imuData.dump(angles);

    //Send data and get position
    if (!downloader.getStartingPosition(startPosition, angles, m_wallID, m_ID))
        return false;

#ifdef SI_DEBUG_BUILD
    SIMQTT.publish("calibEcho", String("Lambda answered: ") + startPosition);
#endif

    if(startPosition.indexOf("G1") > 0  && m_calibrationAttempts < CALIBRATION_ATTEMPTS_LIMIT)
    {
        String l_cmd1 = startPosition.substring(startPosition.indexOf("G92"), startPosition.indexOf(";"));
        String l_cmd2 = startPosition.substring(startPosition.indexOf("G1"));

        sm.forceLineToSAMD(l_cmd1);
        delay(500);
        sm.forceLineToSAMD(l_cmd2);

        return false;
    }
    else if (startPosition.indexOf("err") >= 0)
    {
        SIMQTT.error("ERROR IN LAMBDA", 0);
        return false;
    }
    else
    {
        sm.forceLineToSAMD(startPosition);
        delay(500);
        sm.forceLineToSAMD(m_sendOnStop);

        if(m_printAfterCalibration)
        {
#ifdef SI_DEBUG_BUILD
            SIMQTT.publish("calibDebug", "OK starting download");
#endif
            //Download file and start print
            l_retVal = downloadAndStart();
        }
        else
        {
#ifdef SI_DEBUG_BUILD
            SIMQTT.publish("calibDebug", "OK, back to IDLE");
#endif
            l_retVal = true;
        }
    }
    
    return l_retVal;
}

bool ScribIt::hasNexLink()
{
#ifdef SI_DEBUG_BUILD
    SIMQTT.publish("splitFile", "entering hasNexLink()");
#endif

    bool l_retVal = false;
    uint8_t l_tmpIndex = m_target.indexOf("~"); 
    int l_startFragment = 0;
    int l_endFragment = 0;

    if(l_tmpIndex > 0)
    {
        l_startFragment = m_target.substring(l_tmpIndex+1, m_target.indexOf("~", l_tmpIndex+1)).toInt();

        l_tmpIndex = m_target.indexOf("~", l_tmpIndex+1);

        l_endFragment = m_target.substring(l_tmpIndex+1, m_target.lastIndexOf("~")).toInt();

        l_startFragment++;

        if(l_startFragment <= l_endFragment)
        {
            m_nextTarget = m_target.substring(0, m_target.indexOf("~")) + "~" + String(l_startFragment) + "~" + String(l_endFragment) + "~" + m_target.substring(m_target.lastIndexOf("~")+1);

            l_retVal = true;
        }
        else
        {
#ifdef SI_DEBUG_BUILD
            SIMQTT.publish("splitFile", "No next link.");
#endif
            m_nextTarget = "";
        }

#ifdef SI_DEBUG_BUILD
        SIMQTT.publish("splitFile", m_nextTarget);
#endif
    }

    return l_retVal;
}
