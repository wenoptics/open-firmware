#pragma once

#include "SPIFFS.h"

#include "AxxxLib/CircBufInfinite.h"
#include "SIConfig.hpp"

#define SM_PRINTER_ENDLINE 0x0A
//#define PAUSE_AFTER_Z
#define SI_MAX_GCODE_LINE_LEN 128

enum SIMKOperation
{
    SIMK_IDLE,
    SIMK_WORKING,
    SIMK_BUSY,
    SIMK_HEATING
};

enum PausedState
{
    SIPS_RUNNING = 0,
    SIPS_REQUESTED = 1,
    SIPS_PAUSED = 2
};

class SISerialManager
{
private:
    char currLine[SI_MAX_GCODE_LINE_LEN]; //Currently printing line
    //char extraLine[SI_MAX_GCODE_LINE_LEN]; //Extra line to print at next iteration
    CircBufInfinite<String> extraLines; //Buffer of sent lines
    uint32_t m_lineNumber;              //Next line number
    bool m_waitingForAck;               //Flag to signal is waiting for ack
    uint8_t m_resend;                   //Printer requested resend of last line
    bool m_streamEnded;                 //All lines written and accepted
    File m_inFile;                      //Temporary file handler
    uint32_t m_lastSend;                //Last line sent over Serial
    CircBufInfinite<String> sentLines;  //Buffer of sent lines
    bool m_isPaused;                    //True if print paused
    SIMKOperation m_mkStatus;           //MK4Duo status
    double m_temperature;               //Extruder temp
    int16_t m_imuData;                  //Imu data read
    bool m_newIMUDataAvailable;         //True if new data from imu available since last read
    String m_startingPositionLine;      //Starting position evaluated by remote calibration routine
    bool m_isIMUWorking;
    bool m_penSensitivity;
    bool m_smartCylinder;
#ifdef SI_DEBUG_BUILD
    uint32_t md_resends;
#endif
#ifdef PAUSE_AFTER_Z
    bool m_needsPause;
#endif

    /**
     * @brief Encapsulate line and saves it in currLine
     * 
     * @param line[in] The GCODE about to be sent
     */
    void encapsulate(const char *line)
    {

        char l_actualSentLine[SI_MAX_GCODE_LINE_LEN];

        if(m_penSensitivity &&
          (strstr(line, "G1 Z54")  != nullptr  || 
           strstr(line, "G1 Z126") != nullptr  || 
           strstr(line, "G1 Z198") != nullptr  || 
           strstr(line, "G1 Z270") != nullptr  ||
           //COMMANDS BELOW ARE THE "SAFETY" FIRST MOVEMENTS
           strstr(line, "G1 Z49")  != nullptr  ||
           strstr(line, "G1 Z121") != nullptr  ||
           strstr(line, "G1 Z193") != nullptr  ||
           strstr(line, "G1 Z265") != nullptr))
        {
            sprintf(l_actualSentLine, "%s", "G101");
        }
        else if(!m_smartCylinder && strstr(line, "G77") != nullptr)
        {
            sprintf(l_actualSentLine, "%s", "G");
        }
        else
        {
            sprintf(l_actualSentLine, "%s", line);
        }

        char buffer[SI_MAX_GCODE_LINE_LEN];

        //Write in buffer first part
        sprintf(buffer, "N%d %s", m_lineNumber, l_actualSentLine);

        //Evaluate checksum
        uint8_t cs = 0;
        for (int i = 0; i < strlen(buffer) && buffer[i] != 0; i++)
        {
            cs = cs ^ buffer[i];
        }
        cs &= 0xff;

        //Save all in currLine
        sprintf(currLine, "%s*%d", buffer, cs);
        //Increment line number
        m_lineNumber++;
        //Save line
        sentLines.add(currLine);
    }

    /**
     * @brief Loads next line to write in currLine
     * 
     * @return true next line ready
     * @return false no next line to write right now
     */
    bool loadNextLine();

    /**
     * @brief Writes currLine on printer
     */
    void writeLine();

    /**
     * @brief Allows to restart GCODE stream from a certain line requested by SAMD21
     * 
     * @param line[in] The line from whom the stream is SIPS_REQUESTED
     * 
     * @return true if the restart can be performed, false if not
     */
    bool restartFromLine(int64_t line);
    
    /**
     * @brief If line contains temperature parse it and save it
     * 
     * @param samdSerialBuffer[in] message incoming from SAMD21
     */
    void parseTemperature(const char *samdSerialBuffer);

    /**
     * @brief Parse line from SAMD21 and store inertial data if it contains SIMKOperation
     * 
     * @param samdSerialBuffer[in] message incoming from SAMD21
     */
    bool parseIMUData(const char *samdSerialBuffer);

    /**
     * @brief If the erasing has been stopped during the verticality check parse the message and go back to IDLE
     * 
     * @param samdSerialBuffer[in] message incoming from SAMD21
     */
    void parseVerticalStatus(const char *samdSerialBuffer);

public:
    SISerialManager();

    /**
     * @brief Initialize serial streamer. Waits for first "wait" from printer
     * 
     * @return 0 in any case 
     */
    int begin();

    /**
     * @brief Streams local saved file
     * 
     * @param fileName[in] The path to the file that needs to be streamed in SPIFFS, default path is the one where GOCDE in normally stored
     * 
     * @return true if stream starts correctly, false if there is a stream altready on or cannot open file in SPIFFS
     */
    bool streamLocalFile(String fileName = SI_TEMPORARY_GCODE_PATH);
    bool isStreamEnded() { return m_streamEnded; };

    /**
     * @brief Main serialmanager loop
     * 
     * Actions performed: 
     *          + Check on new line to send
     *          + Reading and parsing messages from SAMD21
     * 
     * @return SIMKOperation current MK4Duo status
     */
    SIMKOperation loop();

    /**
     * @brief Adds a line to the current stream
     * 
     * @param line[in] The current GCODE to be added
     * 
     * @return true if line can be added, false if not
     */
    bool addLineToStream(const char *line);

    /**
     * @brief Stops Gcode stream to SAMD21
     */
    void stopStream();

    /**
     * @brief Pauses or unpauses the actual stream of commands towards SAMD21
     * 
     * @param state[in] if true pauses the stream, if false unpauses it
     */
    void setPause(bool state);

    /**
     * @brief The method is used to force a line in the stream of GCODE without waiting for eventual acks for the previously sent commands
     * 
     * @param p_forcedLine[in] The GCODE is desired to be forced in the stream
     */
    void forceLineToSAMD(String p_forcedLine);

    /**
     * @brief Gets the print status of SAMD21
     * 
     * @return SIPS_PAUSED if the print is actually paused, SIPS_REQUESTED if the printing is about to stop, SIPS_RUNNING if the print is being performed
     */
    PausedState getPausedState();
    double getTemperature() { return m_temperature; }

    /**
     * @brief Get new imu data if available
     * 
     * @param data[out] struct to be filled with new imu data
     * 
     * @return true new imu data available (data contains it)
     * @return false no new imu data available
     */
    bool getIMUData(int16_t &data);
    void setStartingPositionCommand(String t_startingPositionLine) { m_startingPositionLine = t_startingPositionLine; }
    bool isIMUWorking(){return m_isIMUWorking;}
    inline void setPenSensitivity(bool p_status){m_penSensitivity = p_status;}
    inline void setSmartCylinder(bool p_status){m_smartCylinder = p_status;} 
};
