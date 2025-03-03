#pragma once

#include <MQTT.h>
#include <WiFiClientSecure.h>
#include "SIConfig.hpp"
#include "AxxxLib/CircBuf.h"
#include "SIMQTTMessage.hpp"

#define SI_MQTT_TOPIC_IN_PRINT "print"
#define SI_MQTT_TOPIC_IN_STATUS "status"
#define SI_MQTT_TOPIC_IN_RESET "reset"
#define SI_MQTT_TOPIC_IN_ERASE "erase"
#define SI_MQTT_TOPIC_IN_UPDATE "update"
#define SI_MQTT_TOPIC_IN_RESETCONFIG "resetConfig"
#define SI_MQTT_TOPIC_IN_PAUSE "pause"
#define SI_MQTT_TOPIC_IN_SETCONFIG "wifiConfig"
#define SI_MQTT_TOPIC_IN_MANUALMOVE "manualMove"
#define SI_MQTT_TOPIC_IN_CALIBRATION "calibration"

#define SI_MQTT_RESET_WIFI_TIMEOUT_MS 30000
#define SI_MQTT_MAX_SAVED_MESSAGES 3

enum SIMQTT_ERRORS
{
  SIMQTT_ERROR_WRONG_DEVICE_IN_UPDATE = 0x01,
  SIMQTT_ERROR_DOWNLOAD_UNKNOWN_PROTOCOL = 0x02,
  SIMQTT_ERROR_DOWNLOAD_CONNECTION_FAILED = 0x03,
  SIMQTT_ERROR_DOWNLOAD_TIMEOUT = 0x04,
  SIMQTT_ERROR_DOWNLOAD_FILE_IO_ERROR = 0x05,
  SIMQTT_ERROR_DOWNLOAD_FILE_TOO_BIG = 0x06,
  SIMQTT_ERROR_DOWNLOAD_404 = 0x07,
  SIMQTT_ERROR_DOWNLOAD_UNKNOWN = 0x08,
  SIMQTT_ERROR_MQTT = 0x09,
  SIMQTT_ERROR_CANNOT_SYNC_MK4DUO = 0x0A,
  SIMQTT_ERROR_CANNOT_PRINT = 0x0B,
  SIMQTT_ERROR_UNIMPLEMENTED = 0x0C,
  SIMQTT_ERROR_OTA_FAILED = 0x0D,
  SIMQTT_ERROR_HARDWARE_FAIL = 0x0E,
  SIMQTT_ERROR_OTA_NOT_RESET = 0x0F,
  SIMQTT_ERROR_PAUSE_NOT_MOVING = 0x10,
  SIMQTT_ERROR_WIFICONFG_MALFORMED = 0x11,
  SIMQTT_ERROR_STATUS_INCORRECT = 0x12,
  SIMQTT_ERROR_PIN26 = 0x26,
  SIMQTT_ERROR_MESSAGE_BUFFER_FULL = 0x13,
  SIMQTT_ERROR_CANNOT_CALIBRATE = 0x14,
  SIMQTT_ERROR_BAD_CALIBRATION_PAYLOAD = 0x15

};

class SIMQTTClass
{

  WiFiClientSecure net;
  MQTTClient client;

  MQTTClientCallbackSimple m_cb; //Callback function

  uint8_t m_ID[6];
  bool m_initialized = false;

  bool connect();

  /**
   * @brief Creates topic string
   * 
   * @param outTopic[in] topic complete name
   * @param topicName[in] topic name
   * @param isOutTopic[in] true=out false=in
   */
  void makeTopicString(char *outTopic, const char *topicName, bool isOutTopic = true);

  /**
   * @brief Subscribe to given topic
   * 
   * @param topic[in] Topic name (without in/ID)
   */
  void subscribe(const char *topic);
  void setCallback(MQTTClientCallbackSimple cb);

  //Callback function
  static void messageReceived(String &topic, String &payload);

public:
  enum SIMQTTState
  {
    OK = 0x00,
    DISCONNECTED = 0x01,
    NOT_INITIALIZED = 0x02
  };

  SIMQTTClass() : client(SI_MQTT_MAX_PAYLOAD_LEN + 32),         //Set max payload len (Plus 32B for protocol)
                  mqttMessageBuffer(SI_MQTT_MAX_SAVED_MESSAGES) //Set max number of saved messages
  {}

  /**
   * @brief Init MQTT
   * 
   * @param t_ID[in] Device ID
   * @param skip[in] skip connection
   * 
   * @return int 0 if connected -1 if not
   */
  int begin(uint8_t t_ID[6], bool skipConnection = false);

  /**
   * @brief Class loop, to be called frequently. Auto reconnects if client disconnected
   */
  SIMQTTClass::SIMQTTState loop();

  /**
   * @brief Publish the payload on the topic
   * 
   * @param topic[in] last word of the topic path
   * @param payload[in] payload
   */
  void publish(String topic, String payload);
  
  /**
   * @brief Sends an error to the SIMQTT server
   * 
   * @param message[in] error message
   * @param code[in] error code
   */
  void error(String message, int code);
  
  /**
   * @brief Sends a debug on the server (If SI_DEBUG_BUILD is active)
   * 
   * @param tag[in] calling function tag
   * @param message[in] log message
   * @param level[in] log level
   */
  void debug(String tag, String message, int level = 0);
  void statusPrintErase(uint32_t elapsedTimeSec, bool isErase, char isPaused);
  void statusIdle(int8_t RSSI, float temp);
  void statusManual();
  
  /**
   * @brief Sends a success message
   * 
   * @param operation[in] name of operation
   */
  void success(String operation);

  /**
   * @brief Sends boot message
   * 
   * @param samdVer[in] samd firmware version
   * @param spiffsVer[in] spiffs firmware version
   */
  void boot(uint16_t espVer, uint16_t samdVer, uint16_t spiffsVer);

  //Message buffer
  CircBuf<SIMQTTMessage> mqttMessageBuffer;

  /**
   * @brief Get message from mqtt buffer
   * 
   * @param msg[in] pointer to a SIMQTTMessage object that will contain the message
   * 
   * @return true msg contains next message
   * @return false no message in buffer (msg not modified)
   */
  bool getNextMessage(SIMQTTMessage *msg);
};

extern SIMQTTClass SIMQTT;
