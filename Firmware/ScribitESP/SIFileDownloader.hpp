#pragma once
#include "WiFiClientSecure.h"
class SIFileDownloader
{
  String m_host,m_url;
  uint16_t m_httpPort;

  /**
   * @brief Parses download url 
   * 
   * @param target[in] the download url
   * 
   * @return WiFiClient instance to perform download
   */
  WiFiClient* parseTarget(String target);

  public:
    SIFileDownloader(){};

    /**
     * @brief Downloads given files and saves it in SPIFFS
     * 
     * @param target[in] Complete url of the file
     * @param forcemd5Check[in] Forces check on md5 (Download fails if server does not provide md5)ù
     * 
     * @return true if download succeeds, false if not
     */
    bool download(String target,bool forcemd5Check=true);
    
    /**
     * @brief Given inetial data sent to CDN wait a given time and retrieve the response
     * 
     * @param startPositionGcode[out] Variable where response from CDN is stored
     * @param measures[in] Array containing inertial data
     * @param wallID[in] It contains the integer describing the canvas dimension
     * @param deviceID[in] The device serial SI_CALIBRATION_POINT_NUMBER
     * 
     * @return true if the CDN replies, false if not
     */
    bool getStartingPosition(String &startPositionGcode,int16_t measures[SI_CALIBRATION_POINT_NUMBER],uint8_t wallID,uint8_t deviceID[6]);
};
