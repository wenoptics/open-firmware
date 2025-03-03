#include "WiFiClientSecure.h"
#include <Update.h>

#include "ScribIt.hpp"
#include "SIMQTT.hpp"

#define TAG "Scribit_update"

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName)
{
    return header.substring(strlen(headerName.c_str()));
}

bool ScribIt::updateFW(int device, String target)
{
    int port;
    WiFiClient *client;
    int contentLength = 0;
    bool isValidContentType = false;
    char deviceStr[4];

    //Define device name
    if (device == U_COMPANION)
        strcpy(deviceStr, "SAM");
    else if (device == U_FLASH)
        strcpy(deviceStr, "ESP");
    else if (device == U_SPIFFS)
        strcpy(deviceStr, "SPI");
    else
    {
        SIMQTT.debug(TAG, "Uknow device");
    }

    SIMQTT.publish("updating", String("{ \"Device\":\"") + deviceStr + "\",\"Status\":\"Updating\"");

    //Choose correct client-----------------------------
    if (target.startsWith("http://"))
    {
        client = new WiFiClient();
        port = 80;
    }
    else if (target.startsWith("https://"))
    {
        client = new WiFiClientSecure();
        port = 443;
    }
    else
    {
        SIMQTT.error(String("Unknown protocol ") + target, SIMQTT_ERROR_DOWNLOAD_UNKNOWN_PROTOCOL);
        SIMQTT.publish("updating", String("{ \"Device\":\"") + deviceStr + "\",\"Status\":\"Error\"");
        return false;
    }

    //Parse target--------------------------------------
    int hostStart, hostEnd;
    //get index of first byte after http(s)://
    hostStart = target.indexOf("/") + 2;
    hostEnd = target.indexOf("/", hostStart);

    String host = target.substring(hostStart, hostEnd);
    String bin = target.substring(hostEnd);

    SIMQTT.debug(TAG, "Connecting to: " + String(host));
    // Connect to S3
    if (client->connect(host.c_str(), port))
    {
        // Connection Succeed.
        // Fecthing the bin
        SIMQTT.debug(TAG, "Fetching Bin: " + String(bin));

        // Get the contents of the bin file
        client->print(String("GET ") + bin + " HTTP/1.1\r\n" +
                      "Host: " + host + "\r\n" +
                      "Cache-Control: no-cache\r\n" +
                      "Connection: close\r\n\r\n");

        unsigned long timeout = millis();
        while (client->available() == 0)
        {
            if (millis() - timeout > 5000)
            {
                SIMQTT.debug(TAG, "Client Timeout !");
                SIMQTT.publish("updating", String("{ \"Device\":\"") + deviceStr + "\",\"Status\":\"Error\"");
                client->stop();
                delete client;
                return false;
            }
        }

        while (client->available())
        {
            // read line till /n
            String line = client->readStringUntil('\n');
            // remove space, to check if the line is end of headers
            line.trim();

            // if the the line is empty,
            // this is end of headers
            // break the while and feed the
            // remaining `client` to the
            // Update.writeStream();
            if (!line.length())
            {
                //headers ended
                break; // and get the OTA started
            }

            // Check if the HTTP Response is 200
            // else break and Exit Update
            if (line.startsWith("HTTP/1.1"))
            {
                if (line.indexOf("200") < 0)
                {
                    SIMQTT.debug(TAG, "Got a non 200 status code from server. Exiting OTA Update.");
                    SIMQTT.publish("updating", String("{ \"Device\":\"") + deviceStr + "\",\"Status\":\"Error\"");
                    break;
                }
            }

            // extract headers here
            // Start with content length
            if (line.startsWith("Content-Length: "))
            {
                contentLength = atoi((getHeaderValue(line, "Content-Length: ")).c_str());
                SIMQTT.debug(TAG, "Got " + String(contentLength) + " bytes from server");
            }

            // Next, the content type
            if (line.startsWith("Content-Type: ") || line.startsWith("Content-type: "))
            {
                String contentType = getHeaderValue(line, "Content-Type: ");
                SIMQTT.debug(TAG, "Got " + contentType + " payload.");
                if (contentType == "application/octet-stream")
                {
                    isValidContentType = true;
                }
            }
        }
    }
    else
    {
        // Connect to S3 failed
        SIMQTT.error("Connection to " + String(host) + " failed.", SIMQTT_ERROR_OTA_FAILED);
        SIMQTT.publish("updating", String("{ \"Device\":\"") + deviceStr + "\",\"Status\":\"Error\"");
    }

    // Check what is the contentLength and if content type is `application/octet-stream`
    SIMQTT.debug(TAG, "contentLength : " + String(contentLength) + ", isValidContentType : " + String(isValidContentType));

    // check contentLength and content type
    if (contentLength && isValidContentType)
    {
        // Check if there is enough to OTA Update
        bool canBegin = Update.begin(contentLength, device);

        // If yes, begin
        if (canBegin)
        {
            SIMQTT.debug(TAG, "Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
            // No activity would appear on the Serial monitor
            // So be patient. This may take 2 - 5mins to complete
            size_t written = Update.writeStream(*client);

            if (written == contentLength)
            {
                SIMQTT.debug(TAG, "Written : " + String(written) + " successfully");
            }
            else
            {
                SIMQTT.debug(TAG, "Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
                // retry??
                // execOTA();
            }

            if (Update.end())
            {
                SIMQTT.debug(TAG, "OTA done!");
                if (Update.isFinished())
                {
                    SIMQTT.debug(TAG, "Update successfully completed. Rebooting.");
                    SIMQTT.publish("updating", String("{ \"Device\":\"") + deviceStr + "\",\"Status\":\"Done\"");
                    if (device == U_COMPANION)
                        Update.resetCompanion();
                    delay(500);
                    ESP.restart();
                }
                else
                {
                    SIMQTT.error("Update not finished, something went wrong!", SIMQTT_ERROR_OTA_FAILED);
                    SIMQTT.publish("updating", String("{ \"Device\":\"") + deviceStr + "\",\"Status\":\"Error\"");
                    if (device == U_COMPANION)
                        resetSamd();
                }
            }
            else
            {
                SIMQTT.debug(TAG, "Error Occurred. Error #: " + String(Update.getError()));
                SIMQTT.publish("updating", String("{ \"Device\":\"") + deviceStr + "\",\"Status\":\"Error\"");
                if (device == U_COMPANION)
                    resetSamd();
            }
        }
        else
        {
            // not enough space to begin OTA
            // Understand the partitions and
            // space availability
            SIMQTT.debug(TAG, "Not enough space to begin OTA");
            SIMQTT.publish("updating", String("{ \"Device\":\"") + deviceStr + "\",\"Status\":\"Error\"");
            client->flush();
        }
    }
    else
    {
        SIMQTT.error("There was no content in the response", SIMQTT_ERROR_OTA_FAILED);
        SIMQTT.publish("updating", String("{ \"Device\":\"") + deviceStr + "\",\"Status\":\"Error\"");
        client->flush();
    }

    client->stop();
    delete client;
    return true;
}