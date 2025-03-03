#pragma once
#include "RGBLEDs.hpp"
#include "SISerialManager.hpp"
#include "SIFileDownloader.hpp"
#include "ScribitVersion.hpp"
#include "AxxxLib/CircBufInfinite.h"
#include "ArduinoJson.h"

#define SI_CALIBRATION_GCODE_FILE "/calib.gcode"
#define CALIBRATION_ATTEMPTS_LIMIT 2

enum SI_State
{
  SI_RESET = 0,
  SI_BOOT=0x01,
  SI_ERROR = 0xFF, //Fatal error encountered
  SI_IDLE = 0x0A,
  SI_PRINTING = 0x02, //Printing
  SI_ERASING = 0x03,
  SI_HEATING=0x04,
  SI_MANUAL=0x05,
  SI_DISCONNECTED=0x06,
  SI_CALIBRATING=0x07
};

class ScribIt
{
  uint8_t m_ID[6]; //MAC address

  SI_State m_state;
  SISerialManager sm; //Serial manager
  SIFileDownloader downloader;
  uint32_t m_startPrintingT;  //Print start time (Or start from last pause)
  uint32_t m_printingTime;    //Saved printing time
  uint32_t m_lastStatusSentT; //Time when last status message was sent

  //Print data
  String m_target; //String containing download target
  String m_nextTarget;
  bool m_isErase;  //True if is erase false if is printing
  uint8_t m_calibrationAttempts = 0;
  bool m_printAfterCalibration;
  String m_sendOnStop; //GCODE to be sent in case of print stopped
  uint8_t m_wallID; //Wall ID (1-9)

  //Firmware versions
  uint16_t m_samdVer;
  uint16_t m_spiffsVer;

  //Error variables
  uint8_t errorCode;
  String errorMessage;

  //LED thread handle
  TaskHandle_t LEDThreadHandle;

  //Gcode test mode
  bool m_testMode = false;

  //IMU data
  CircBufInfinite<int16_t> m_imuData;

  /**
   * @brief Generates access point for receiving ssid and password to connect to a wifi network
   */
  void configureWifi();

  /**
   * @brief Evaluates input commands from MQTT
   * 
   */
  void evaluateMQTTIn();

  /**
   * @brief Sync with SAMD and asks for FW version
   * 
   * @return true sync successful
   * @return false sync timeout
   */
  bool syncWithSamd();

  /**
   * @brief Resets the SAMD processor
   */
  void resetSamd();

  /**
   * @brief Sets stepper microstep pin
   */
  void setStepperStepPin();

  /**
   * @brief Used to manage reset function when holding the left part of the LED stripe. 
   */ 
  void processButtonInterrupt();
  
  /**
   * @brief Try to connect to given wi-fi
   * 
   * @param ssid[in] Wifi ssid
   * @param pass[in] Wifi password
   * 
   * @return true connection success
   * @return false connection failed
   */
  bool connectToWiFi(const char *ssid, const char *pass);

  /**
   * @brief It parses new wifi connection configuration given the mqtt message payload.
   * 
   * @param buffer[in] the buffer containing the mqtt message payload
   * @param ssid[out] variable to store wifi network ssid
   * @param pass[out] variable to store wifi network password
   * 
   * @return true if at least the ssid is sent, false if not.
   */
  bool parseWifiConfigJSON(const char *buffer, char *ssid, char *pass);

  /**
   * @brief When receiving status message at boot it's payload is parsed to activate or deactivate smart features
   *        Smart features are pens sensitivity and smart cylinder.
   * 
   * @param p_payload[in] The mqtt message payload
   * 
   * @return true if payload is a valid JSON, false if not. 
   */
  bool setSmartConfig(const char *p_payload);

  /**
   * @brief Checks if the left part of the led stripe has been pressed three or more times and eventually streams test GCODE to SAMD21
   */
  bool isTestGcodeRequested();

  /**
   * @brief Parse Print and Erase target
   * 
   * @param payload[in] string payload
   * 
   * @return true target parsed
   * @return false target parse error
   */
  bool parsePETarget(char *payload);

  /**
   * @brief It parses payload coming from mqtt calibration topic to store data used for calibration
   * 
   * @param mqtt[in] message to be parsed.
   * 
   * @return true if parsing is ok, false if not
   */
  bool parseCalibPyaload(const char *p_payload);

  /**
   * @brief Parse semicolon separated gcode string and saves it in temporary file
   * 
   * @param gcode[in] The gcode string
   * 
   * @return true parsing successiful, file saved
   * @return false parsing failed
   */
  bool parseAndSaveGcodeString(char *gcodeString);

  /**
   * @brief Used to switch from connected state to disconnected and viceversa.
   */ 
  void setDisconnected(bool isDisconnected);

  /**
   * @brief Save given gcode string in temporary file
   * 
   * @param Gcode[in] gcode string
   * @param fileName[in] name of the file where will be saved
   * 
   * @return true file saved
   * @return false error occurred
   */
  bool saveGcodeStringInFile(String Gcode,String fileName=SI_TEMPORARY_GCODE_PATH);

  /**
   * @brief Downloads saved target and starts streaming
   * 
   * @param p_showDownloadLeds[in] Flag to show or not downloading leds, It doesn't show leds in case of splitted gcode
   * 
   * @return true download successiful and stream started
   * @return false download error
   */
  bool downloadAndStart(bool p_showDownloadLeds = true);
  
  /**
   * @brief Starts calibration
   */
  void startCalibration();

  /**
   * @brief Complete calibration by sending inertial data and getting calculated initial position
   * 
   * @return true calibration completed and eventual successful download
   * @return false calibration or download error
   */
  bool completeCalibration();

  /**
   * @brief Parses gcode download link and checks if it is a splitted gcode or not.
   * 
   * @return true if is a splitted gcode and it has a next part to download, false if not
   */
  bool hasNexLink();

public:
  RGBLEDs leds; //RGBLed
  ScribIt() : m_imuData(SI_CALIBRATION_POINT_NUMBER)
  {
    m_state = SI_RESET;
    m_samdVer = 0;
    m_spiffsVer = 0;
    m_target = String("");
    m_isErase = false;
    m_printingTime = 0;
    m_wallID=0;
  };

  /**
   * @brief Init scribit
   * 
   * Actions performed:
   *    + Stepper step pin setting
   *    + Creation of LED task thread
   *    + Serial connected to SAMD21 init
   *    + Button interrupt is connected to a methods that handles the event
   *    + Device mac address is read and stored
   *    + SPIFFS checks
   *    + Wifi attempt of connection, if it fails access point is raised
   *    + SerialManager init
   *    + Wait to receive status message from server
   */
  void begin();

  /**
   * @brief Resets scribit configuration and restarts ESP32
   */
  void resetConfig();

  /**
   * @brief Main scribit loop, to be called continuously
   * 
   * Actions performed:
   *      + Evaluate eventual incoming mqtt messages
   *      + Send actual state if a certain time is elapsed
   *      + if streaming something on serial check if the stream is still in progress
   *      + Update IMU data 
   */
  void loop();

  /**
   * @brief Set Scribit state and change leds according to the new state.
   * 
   * @param newState[in] new scribit state
   */
  void setState(const SI_State newState);
  SI_State getState() { return m_state; };

  /**
   * @brief Sends scribit status via MQTT
   */
  void sendStatus();

  /**
   * @brief Given a device and a download url performs an update on the targeted device
   * 
   * @param device[in] The targeted device.
   * @param url[in] The new firmware download url
   * 
   * @return true if the update has been performed correctly, false if not
   */ 
  bool updateFW(int device,String url);

  /**
   * @brief Get the Target of print
   * 
   * @param target[out] print target
   * 
   * @return true is erase
   * @return false is print
   */
  bool getTarget(String &target)
  {
    target = m_target;
    m_target = "";

    return m_isErase;
  }
};
