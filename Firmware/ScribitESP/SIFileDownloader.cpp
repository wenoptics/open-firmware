#include <libb64/cdecode.h>

#include <Arduino.h>
#include <SPIFFS.h>
#include <MD5Builder.h>
#include "SIMQTT.hpp"

#include "SIFileDownloader.hpp"
#include "SIConfig.hpp"

#define SI_SERVER_MD5_HEADER "x-goog-hash: md5"
#define SI_MD5_LEN 16 + 1 //One more for string termnator
#define TAG "SIFileDownloader"
#define SI_DOWNLOADER_CHUNK_LEN 1024


bool SIFileDownloader::download(String target, bool forcemd5Check)
{
    WiFiClient *client;
    char serverMd5[SI_MD5_LEN] = {}; //Md5 got from server
    MD5Builder localMd5;             //Md5 to be calculated locally
    uint8_t buffer[SI_DOWNLOADER_CHUNK_LEN];

    bool md5Present = false; //Flag to check Md5 if in header
    uint32_t len = 0;

#ifdef SI_DEBUG_BUILD
    uint8_t percentage = 0; //Download percentage
#endif

    localMd5.begin();

    client = parseTarget(target);

    //Connect to server---------------------------------
    if (!client->connect(m_host.c_str(), m_httpPort))
    {
        SIMQTT.error(String("Connection failed to: ") + target, SIMQTT_ERROR_DOWNLOAD_CONNECTION_FAILED);
        return false;
    }
    else
    {
        SIMQTT.debug(TAG, String("Requesting URL: ") + m_url);

        String s = String("GET ") + m_url + " HTTP/1.1\r\n" +
                   "Host: " + m_host + "\r\n" +
                   //"Range: bytes=" + readStart + "-" + (readStart + byteToDownload) + "\r\n" +
                   "Connection: close\r\n\r\n";

        // This will send the request to the server
        client->print(s);

        unsigned long timeout = millis();
        while (client->available() == 0)
        {
            if (millis() - timeout > SI_DOWNLOAD_TIMEOUT)
            {
                SIMQTT.error("Timeout downloading data", SIMQTT_ERROR_DOWNLOAD_TIMEOUT);
                client->stop();
                delete client;
                return false;
            }
        }

        String line;
        //Read response code--------------------------------
        line = client->readStringUntil(0x0A);
        uint16_t ti = line.indexOf(" ") + 1;
        String code = line.substring(ti, line.indexOf(" ", ti));

        //Evaluate response code----------------------------
        if (code.equals("200")) //OK
        {
            //Delete old temp file
            if (!SPIFFS.remove(SI_TEMPORARY_GCODE_PATH))
            {
                SIMQTT.debug(TAG, String("Unable to remove file ") + SI_TEMPORARY_GCODE_PATH);
            }
            //Open file
            File file = SPIFFS.open(SI_TEMPORARY_GCODE_PATH, FILE_WRITE);
            if (!file)
            {
                SIMQTT.error("Unable to open file to store gcode", SIMQTT_ERROR_DOWNLOAD_FILE_IO_ERROR);
                client->stop();
                delete client;
                return false;
            }
            //Header------------------------------------------------------------------------------
            do
            {
                line = client->readStringUntil(0x0A);

                if (line.startsWith("Content-Length:")) //Find length
                {
                    //Length---------------------------------------------------
                    len = atoll(line.substring(15).c_str());

                    if (len > SPIFFS.totalBytes() - SPIFFS.usedBytes())
                    {
                        SIMQTT.error("File too big for SPIFFS space", SIMQTT_ERROR_DOWNLOAD_FILE_TOO_BIG);
                        client->stop();
                        delete client;
                        return false;
                    }
                }
                else if (line.indexOf(SI_SERVER_MD5_HEADER) >= 0) //Check for md5
                {
                    //MD5---------------------------------------------------
                    ti = line.indexOf("md5") + 3;
                    String base64md5 = line.substring(ti, line.indexOf(" ", ti));
                    base64_decodestate md5State;
                    base64_init_decodestate(&md5State);
                    base64_decode_block(base64md5.c_str(), base64md5.length(), serverMd5, &md5State);
                    md5Present = true;
                }

            } while (client->available() && line.length() != 1);
            uint32_t downloadedBytes = 0;
            //Read data and fill buffer
            while (downloadedBytes < len)
            {
                //Evaluate bytes to download in next chunk
                uint16_t bytesToDownload = (len - downloadedBytes) > SI_DOWNLOADER_CHUNK_LEN ? SI_DOWNLOADER_CHUNK_LEN : (len - downloadedBytes);

                uint16_t chunkLen = client->readBytes(buffer, bytesToDownload);
                downloadedBytes += chunkLen;

                if (file.write(buffer, chunkLen) != chunkLen)
                {
                    SIMQTT.error("Write failed, is space over?", SIMQTT_ERROR_DOWNLOAD_FILE_IO_ERROR);
                    file.close();
                    client->stop();
                    delete client;
                    return false;
                }

#ifdef SI_DEBUG_BUILD
                //Evaluate percentage and send to debug
                uint8_t newPerc = ((uint32_t)100 * downloadedBytes / len);
                if (newPerc > percentage)
                {
                    percentage = newPerc;
                    SIMQTT.debug(TAG, String("Downloading: ") + newPerc + "%");
                }
#endif
            }

            SIMQTT.debug(TAG, "File ended");
            file.close();
        }
        else if (code.equals("404"))
        {
            SIMQTT.error("Error 404", SIMQTT_ERROR_DOWNLOAD_404);
            client->stop();
            delete client;
            return false;
        }
        else
        {
            SIMQTT.error("Client replied with Unknown code: " + code, SIMQTT_ERROR_DOWNLOAD_UNKNOWN);
            client->stop();
            delete client;
            return false;
        }
    }
    client->stop();
    delete client;

    //Check md5----------------------------------------------
    if (md5Present)
    {
        char calculatedMd5[SI_MD5_LEN] = {};

        File file = SPIFFS.open(SI_TEMPORARY_GCODE_PATH, FILE_READ);
        if (!file)
        {
            SIMQTT.error("Unable to open temporary gcode file in read mode", SIMQTT_ERROR_DOWNLOAD_FILE_IO_ERROR);
            return false;
        }
        //Check size
        if (file.size() != len)
        {
            SIMQTT.error("File dimension mismatch ", SIMQTT_ERROR_DOWNLOAD_FILE_IO_ERROR);
        }
        //Evaluate md5
        localMd5.addStream(file, len);
        localMd5.calculate(); //Evaluate md5
        localMd5.getBytes((uint8_t *)calculatedMd5);
        file.close();

        SIMQTT.debug(TAG, "Md5 Check");
        for (int i = 0; i < SI_MD5_LEN; i++)
        {

            if (calculatedMd5[i] != serverMd5[i])
            {
                SIMQTT.error("Md5 mismatch", SIMQTT_ERROR_DOWNLOAD_FILE_IO_ERROR);
                return false;
            }
        }
    }
    else if (forcemd5Check)
    {
        SIMQTT.error("Missing md5 but control forced. Cannot download", SIMQTT_ERROR_DOWNLOAD_FILE_IO_ERROR);
        return false;
    }

    return true;
}

WiFiClient *SIFileDownloader::parseTarget(String target)
{
    WiFiClient *client;

    //Choose correct client-----------------------------
    if (target.startsWith("http://"))
    {
        client = new WiFiClient();
        m_httpPort = 80;
    }
    else if (target.startsWith("https://"))
    {
        client = new WiFiClientSecure();
        m_httpPort = 443;
    }
    else
    {
        //ESP_LOGE("SIFileStreamer", "Unknown protocol");
        SIMQTT.error(String("Unknown protocol ") + target, SIMQTT_ERROR_DOWNLOAD_UNKNOWN_PROTOCOL);
        return nullptr;
    }

    //Parse target--------------------------------------
    int hostStart, hostEnd;
    //get index of first byte after http(s)://
    hostStart = target.indexOf("/") + 2;
    hostEnd = target.indexOf("/", hostStart);

    m_host = target.substring(hostStart, hostEnd);
    m_url = target.substring(hostEnd);

    SIMQTT.debug(TAG, String("Connecting to host: ") + m_host + " - url: " + m_url);

    return client;
}

bool SIFileDownloader::getStartingPosition(String &startPositionGcode, int16_t measures[SI_CALIBRATION_POINT_NUMBER], uint8_t wallID, uint8_t deviceID[6])
{
    String line;
    //Parse target
    WiFiClient *client = parseTarget(SI_CALIBRATION_URL);
    //Connect to host
    if (!client->connect(m_host.c_str(), m_httpPort))
    {
        SIMQTT.error(String("Connection failed to: ") + m_host, SIMQTT_ERROR_DOWNLOAD_CONNECTION_FAILED);
        client->stop();
        delete client;
        return false;
    }

    //Create data string------------------------------------
    String data = "{\"sn\": \"";
    //Add ID
    for (int i = 0; i < 6; i++)
        data += String(deviceID[i], HEX);
    //Add wallID
    data += "\", \"wallId\":" + String(wallID, DEC) + ",";
    //Add calibration data
    data += "\"scans\": [";
    for (int i = 0; i < SI_CALIBRATION_POINT_NUMBER; i++)
    {
        data += String((float) measures[i]/10, DEC);
        if (i < SI_CALIBRATION_POINT_NUMBER - 1)
            data += ",";
    }
    data += "]}";

    //SIMQTT.debug(TAG, "Calibration payload:" + data);
    String request=String("POST ") + m_url + " HTTP/1.1\r\n" +
                 "Host: " + m_host + "\r\n" +
                 "Accept: */*\r\n" +
                 "Content-Type: application/json\r\n" +
                 "accesstoken: cmJqv3ah7nPj3OVGoNyevDXs7LwNJbIW\r\n" +
                 "Content-Length: " +String(data.length(), DEC) + "\r\n" +
                 "\n" +  data;
    //Send request-----------------------------------------
    //SIMQTT.debug(TAG,request);
    client->print(request);
    

    //Wait for reply
    unsigned long timeout = millis();
    while (client->available() == 0)
    {
        if (millis() - timeout > SI_CALIBRATION_API_TIMEOUT)
        {
            SIMQTT.error("Timeout waiting for calibration results", SIMQTT_ERROR_CANNOT_CALIBRATE);
            client->stop();
            delete client;
            return false;
        }
    }

    //Read response code--------------------------------
    line = client->readStringUntil(0x0A);
    //SIMQTT.debug(TAG,line);
    uint16_t ti = line.indexOf(" ") + 1;
    String code = line.substring(ti, line.indexOf(" ", ti));

    //Evaluate response code----------------------------
    if (code.equals("200")) //OK
    {
        // Skip Header------------------------------------------------------------------------------
        do
        {
            line = client->readStringUntil(0x0A);
            //SIMQTT.debug(TAG,line);
        } while (client->available() && line.length() != 1);
        
        line=client->readStringUntil(0x0A);
        uint8_t cmdStart=line.indexOf("G92");
        uint8_t cmdEnd=line.indexOf("\"",cmdStart);
        startPositionGcode=line.substring(cmdStart,cmdEnd);

        SIMQTT.debug(TAG,"Calib received: "+startPositionGcode);
    }
    else if (code.equals("404"))
    {
        SIMQTT.error("CDN replied 404", SIMQTT_ERROR_CANNOT_CALIBRATE);
        client->stop();
        delete client;
        return false;
    }
    else
    {
        SIMQTT.error("CDN replied with Unknown code: " + code, SIMQTT_ERROR_CANNOT_CALIBRATE);
        client->stop();
        delete client;
        return false;
    }

    client->stop();
    delete client;

    //Return true if line found
    return startPositionGcode.length() > 0;
}