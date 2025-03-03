#include "SISerialManager.hpp"
#include "SIMQTT.hpp"
#include "SIConfig.hpp"

#define SI_SM_MAX_REPLY_LEN 128
#define SI_SM_ACK_TIMEOUT_MS 5000

#define TAG "SerialManager"

SISerialManager::SISerialManager() : 
    extraLines(SI_SM_EXTRA_LINE_BUFFER_LEN), 
    sentLines(SI_SM_SENT_LINE_BUFFER_LEN), 
    m_isPaused(false), m_temperature(0.0), 
    m_newIMUDataAvailable(false), 
    m_startingPositionLine(""),
    m_smartCylinder(true),
    m_penSensitivity(true)    
{};

int SISerialManager::begin()
{
    char buffer[SI_SM_MAX_REPLY_LEN];
    m_streamEnded = true;
    m_waitingForAck = false; //Reset waitng for ack flag

#ifdef PAUSE_AFTER_Z
    m_needsPause = false;
#endif

    //Dump read data
    while (Serial.available())
        Serial.read();

    SIMQTT.debug(TAG, "Waiting sync with Mk4Duo");

    //Assume IMU works until first error
    m_isIMUWorking=true;

    return 0;
}

bool SISerialManager::streamLocalFile(String fileName)
{
    if (!m_streamEnded)
    {
        //File not ended
        return false;
    }

    //Empty sent buffer------------
    String s;
    while (!sentLines.empty())
    {
        sentLines.remove(&s);
    }
#ifdef SI_DEBUG_BUILD
    //Reset resends number
    md_resends = 0;
#endif
    //Reset pause flag
    m_isPaused = false;

    //Open file
    m_inFile = SPIFFS.open(fileName, FILE_READ);
    if (!m_inFile)
    {
        SIMQTT.error(String("Unable to open gcode ") + fileName + "file in read mode", SIMQTT_ERROR_DOWNLOAD_FILE_IO_ERROR);
        return false;
    }

    //Reset line number
    addLineToStream("N-1 M110*15");
    //Add line to sent buffer
    sentLines.add("N-1 M110*15");

    //Reset waiting for ACK
    m_waitingForAck = true;
    //Reset resend
    m_resend = 0;
    //Signal stream not ended
    m_streamEnded = false;
    //Start from line 0
    m_lineNumber = 0;

#ifdef PAUSE_AFTER_Z
    //Reset needpause flag
    m_needsPause = false;
#endif
}

void SISerialManager::writeLine()
{
    //Write line to serial
    Serial.println(currLine);
    //Signal waiting for ack
    m_waitingForAck = true;
    //Save the time
    m_lastSend = millis();

#ifdef SI_ECHO_GCODE
    //Echo gcode
    SIMQTT.debug(TAG, String("W: \"") + currLine + "\"", 9);
#endif
}

bool SISerialManager::loadNextLine()
{
    char buffer[SI_SM_MAX_REPLY_LEN];
    //Check for resend-----------------------
    if (m_resend > 0)
    {
        //Load first line to resend
        String oldLine;
        sentLines.peekBack(&oldLine, m_resend - 1);
        strcpy(currLine, oldLine.c_str());
        //Decrease line to be resent
        m_resend--;

        return true;
    }

    //Check for extra line-----------------------------------
    if (!extraLines.empty())
    {
        String extraLine;
        //Get next extra line
        extraLines.remove(&extraLine);
        //Copy in currLine
        strcpy(currLine, extraLine.c_str());
    }
#ifdef PAUSE_AFTER_Z
    //Check for need of z pause-------------------------------
    else if (m_needsPause) //If need pause add as next line
    {
        encapsulate("G4 P100");
        m_needsPause = false;
    }
#endif
    else
    {
        //Do not load next line of stream if stream ended or pause
        if (isStreamEnded() || m_isPaused)
        {
            return false;
        }

//If calibration is not in debug mode send starting position command
#ifndef SI_CALIBRATION_DEBUG
        if (m_startingPositionLine.length() > 0)
        {
            //Send as next line
            encapsulate(m_startingPositionLine.c_str());
            //Reset position line
            m_startingPositionLine="";
        }
#endif

        do
        {
            //Check if file ended
            if (!m_inFile.available())
            {
                //Signal stream end
                m_streamEnded = true;
                m_inFile.close(); //Close file
#ifdef SI_DEBUG_BUILD
                SIMQTT.debug(TAG, String("Print ended: ") + md_resends + " resends");
#endif
                return false;
            }
            uint16_t len = m_inFile.readBytesUntil('\n', buffer, SI_SM_MAX_REPLY_LEN);

            //Add string terminator
            if (buffer[len - 1] == 0x0D) //Remove 0x0D if present
            {
                buffer[len - 1] = 0;
            }
            else
            {
                buffer[len] = 0;
            }
        } while (buffer[0] == ';'); // read until first line is not comment
#ifdef PAUSE_AFTER_Z
        if (strstr(buffer, "Z"))
            m_needsPause = true;
#endif

        //Encapsulate and save buffer
        encapsulate(buffer);
    }

    return true;
}

SIMKOperation SISerialManager::loop()
{
    //Check if received ACK
    if (!m_waitingForAck)
    {
        //Load next line if present
        if (loadNextLine())
        {
            //Write line
            writeLine();
        }
    }
    //Handle printer reply---------------------------------
    while (Serial.available())
    {
        SIMQTT.loop();
        char samdSerialBuffer[SI_SM_MAX_REPLY_LEN];
        //Read a line
        int len = Serial.readBytesUntil(SM_PRINTER_ENDLINE, samdSerialBuffer, SI_SM_MAX_REPLY_LEN);
        //If something was read
        if (len > 0)
        {
            //Terminate string
            if (samdSerialBuffer[len - 1] == 0x0D) //Delete 0x0D if present
            {
                samdSerialBuffer[len - 1] = 0;
            }
            else
            {
                samdSerialBuffer[(len < SI_SM_MAX_REPLY_LEN) ? len : SI_SM_MAX_REPLY_LEN - 1] = 0;
            }

#ifdef SI_ECHO_GCODE
            SIMQTT.debug(TAG, String("R: \"") + samdSerialBuffer + "\"", 9);
#endif
            //OK-----------------------------------------------------
            if (strstr(samdSerialBuffer, "ok") != nullptr) //Printer ready for line
            {

                m_waitingForAck = false;   //Reset ACK
                m_mkStatus = SIMK_WORKING; //Set MK4Duo as working

                //Check for temperature
                parseTemperature(samdSerialBuffer);
                //Check for IMU data
                if (parseIMUData(samdSerialBuffer))
                    m_newIMUDataAvailable = true;

                parseVerticalStatus(samdSerialBuffer);
            }
            //RESEND-------------------------------------------------
            else if (strstr(samdSerialBuffer, "Resend") != nullptr) //Printer requested resend
            {
#ifdef SI_DEBUG_BUILD
                md_resends++; //Incremend resends number
#endif

                //Parse line number
                char *p = strstr(samdSerialBuffer, ":");
                if (p != nullptr)
                {
                    p++;
                    //Get line number to resend
                    int64_t reqLine = atoll(p);
                    //Restart streaming from line
                    restartFromLine(reqLine);
                }
                else
                {
                    //If no line number restart from last one
                    restartFromLine(m_lineNumber - 1);
                }
            }
            //Error--------------------------------------------------------
            else if (strstr(samdSerialBuffer, "Error") != nullptr)
            {
                if (strstr(samdSerialBuffer, "Trying to command thermistor pin") != nullptr)
                {
                    SIMQTT.error(samdSerialBuffer, SIMQTT_ERROR_PIN26);
                }
                else if (strstr(samdSerialBuffer, "IMU unavailable") != nullptr)
                {
                    SIMQTT.error(samdSerialBuffer, SIMQTT_ERROR_HARDWARE_FAIL);
                    m_isIMUWorking=false;
                }
                else if (strstr(samdSerialBuffer, "Hall sensor") != nullptr)
                {
                    SIMQTT.error(samdSerialBuffer, SIMQTT_ERROR_HARDWARE_FAIL);
                }
                else
                    SIMQTT.debug(TAG, String("Printer reported unhandled error: ") + samdSerialBuffer);
            }
            //WAIT----------------------------------------------------------
            else if (strstr(samdSerialBuffer, "wait") != nullptr) //Idle
            {
                m_mkStatus = SIMK_IDLE;

                //If timeout expired when waiting for ACK
                if (m_waitingForAck && (millis() - m_lastSend) > SI_SM_ACK_TIMEOUT_MS)
                {
                    //If stream is not ended
                    if (!m_streamEnded)
                        //Resend last line
                        restartFromLine(m_lineNumber - 1);
                    //Clear wait for ack
                    m_waitingForAck = false;
                    SIMQTT.debug(TAG, "Timeout waiting for ACK, resending");
                }
            }
            //Busy-------------------------------------------------------
            else if (strstr(samdSerialBuffer, "busy") != nullptr) //Busy
            {
                //Printer busy
                if (strstr(samdSerialBuffer, "heating") != nullptr)
                    m_mkStatus = SIMK_HEATING;
                else
                {
                    m_mkStatus = SIMK_BUSY;
                }
            }
            //Unknown----------------------------------------------------
            else
            {
                SIMQTT.debug(TAG, String("Unknown printer reply: ") + samdSerialBuffer);
            }
        }
    }

    //Return mk4duo status
    return m_mkStatus;
}

bool SISerialManager::addLineToStream(const char *line)
{
    //Check for extralines buffer full
    if (extraLines.full())
    {
        SIMQTT.debug(TAG, "Extra line buffer full, unable to add");
        return false;
    }
    else
    {
        //Add line to extralines buffer
        extraLines.add(line);
        return true;
    }
}

bool SISerialManager::restartFromLine(int64_t line)
{
    char buffer[SI_SM_MAX_REPLY_LEN];
    int linesToResend = m_lineNumber - line - m_resend;

    //Check for requested future line
    if (linesToResend < 0)
    {
        //Reset resend
        m_resend = 0;
        SIMQTT.debug(TAG, String("Requested future line: ") + linesToResend);
    }
    //If line is older than buffer can store
    else if (linesToResend > SI_SM_SENT_LINE_BUFFER_LEN)
    {
        SIMQTT.debug(TAG, String("Requested too many lines ago: ") + linesToResend);

        //Go back to file start
        m_inFile.close();

        m_inFile = SPIFFS.open(SI_TEMPORARY_GCODE_PATH, FILE_READ);
        if (!m_inFile)
        {
            SIMQTT.error("Unable to reopen temporary gcode file in read mode", SIMQTT_ERROR_DOWNLOAD_FILE_IO_ERROR);
            return false;
        }

        for (uint32_t nLine = 0; nLine < line; nLine++)
        {
            do
            {
                //Check if file ended
                if (!m_inFile.available())
                {
                    //Error
                    SIMQTT.error("Unable to resume print, eof reached before line", SIMQTT_ERROR_DOWNLOAD_FILE_IO_ERROR);
                    return false;
                }
                uint16_t len = m_inFile.readBytesUntil('\n', buffer, SI_SM_MAX_REPLY_LEN);

            } while (buffer[0] == ';'); // read until first line is not comment
        }

        //Signal next line as requested
        m_lineNumber = line;
        m_resend = 0; //Reset resend

        //Empty buffer
        String s;
        while (!sentLines.empty())
            sentLines.remove(&s);
    }
    else
    {
        //Save number of lines to be resend
        m_resend += linesToResend;
    }

    //Return success
    return true;
}

void SISerialManager::stopStream()
{
    //Signal stream as ended
    m_streamEnded = true;
    //If file still open close it
    if (m_inFile)
    {
        m_inFile.close();
    }
    //Reset pause flag
    if (m_isPaused)
        m_isPaused = false;
}

void SISerialManager::setPause(bool state)
{
    //Check stream running
    if (!m_streamEnded)
    {
        //Set pause flag
        m_isPaused = state;
        //Send pause command
        addLineToStream("G4 P1");
    }
    else
    {
        SIMQTT.error("Can't pause because not printing/erasing", SIMQTT_ERROR_STATUS_INCORRECT);
    }
}

PausedState SISerialManager::getPausedState()
{
    //If pause requested
    if (m_isPaused)
    {
        //If buffer ended is paused
        if (m_mkStatus == SIMK_IDLE)
        {
            return SIPS_PAUSED;
        }
        else
        {
            return SIPS_REQUESTED;
        }
    }
    else
    {
        return SIPS_RUNNING;
    }
}

void SISerialManager::forceLineToSAMD(String p_forcedLine)
{
    encapsulate(p_forcedLine.c_str());
    writeLine();
}

void SISerialManager::parseTemperature(const char *samdSerialBuffer)
{
    char *tp = strstr(samdSerialBuffer, "T:");
    if (tp != nullptr) //Temperature data present
    {
        tp += 2;                //Skip "T:"
        double temp = atof(tp); //Get temperature

        if (temp != 0) //If temperature valid save it
        {
            m_temperature = temp;
        }
    }
}

void SISerialManager::parseVerticalStatus(const char *samdSerialBuffer)
{
    if(strstr(samdSerialBuffer, "C:Not vertical, stop erasing.") != nullptr)
    {
        stopStream();
        addLineToStream("M104 S0");
        SIMQTT.publish("verticalDebug", "SamdStop identified.");
    }
}

bool SISerialManager::parseIMUData(const char *samdSerialBuffer)
{
    char *tp = strstr(samdSerialBuffer, "I:"), *eon;
    double angle;
    if (tp != nullptr) //Temperature data present
    {
        tp += 2; //Skip "I:"
        angle = std::strtod(tp, &eon);
        if (tp == eon)
        {
            SIMQTT.debug(TAG, String("Error converting measure to string: ") + samdSerialBuffer);
            return false;
        }

#ifdef SI_DEBUG_BUILD
        SIMQTT.debug(TAG, String("Angle parsed: ") + angle);
#endif
        //Save
        m_imuData = angle * 10;
        //Signal new data
        return true;
    }

    return false;
}

bool SISerialManager::getIMUData(int16_t &data)
{
    //If no new data return false
    if (!m_newIMUDataAvailable)
        return false;
    //Copy imu data
    data = m_imuData;
    //Reset flag
    m_newIMUDataAvailable = false;
    return true;
}
