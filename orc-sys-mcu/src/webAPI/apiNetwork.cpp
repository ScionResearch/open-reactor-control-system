/**
 * @file apiNetwork.cpp
 * @brief Network configuration API implementation
 */

#include "apiNetwork.h"
#include "../network/networkManager.h"
#include "../utils/logger.h"
#include <ArduinoJson.h>

void setupNetworkAPI()
{
    server.on("/api/network", HTTP_GET, []()
    {
        Serial.println("[WEB] /api/network GET request received");
        StaticJsonDocument<512> doc;
        doc["mode"] = networkConfig.useDHCP ? "dhcp" : "static";
        
        // Get current IP configuration
        IPAddress ip = eth.localIP();
        IPAddress subnet = eth.subnetMask();
        IPAddress gateway = eth.gatewayIP();
        IPAddress dns = eth.dnsIP();
        
        Serial.printf("[WEB] IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
        
        char ipStr[16];
        snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        doc["ip"] = ipStr;
        
        char subnetStr[16];
        snprintf(subnetStr, sizeof(subnetStr), "%d.%d.%d.%d", subnet[0], subnet[1], subnet[2], subnet[3]);
        doc["subnet"] = subnetStr;
        
        char gatewayStr[16];
        snprintf(gatewayStr, sizeof(gatewayStr), "%d.%d.%d.%d", gateway[0], gateway[1], gateway[2], gateway[3]);
        doc["gateway"] = gatewayStr;
        
        char dnsStr[16];
        snprintf(dnsStr, sizeof(dnsStr), "%d.%d.%d.%d", dns[0], dns[1], dns[2], dns[3]);
        doc["dns"] = dnsStr;

        doc["mac"] = deviceMacAddress;
        doc["hostname"] = networkConfig.hostname;
        doc["ntp"] = networkConfig.ntpServer;
        doc["dst"] = networkConfig.dstEnabled;
        
        String response;
        serializeJson(doc, response);
        Serial.printf("[WEB] Sending /api/network response (%d bytes)\n", response.length());
        server.send(200, "application/json", response);
        Serial.println("[WEB] /api/network response sent successfully");
    });

    server.on("/api/network", HTTP_POST, []()
    {
        if (!server.hasArg("plain"))
        {
            server.send(400, "application/json", "{\"error\":\"No data received\"}");
            return;
        }

        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, server.arg("plain"));

        if (error)
        {
            server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }

        // Update network configuration
        networkConfig.useDHCP = doc["mode"] == "dhcp";

        if (!networkConfig.useDHCP)
        {
            // Validate and parse IP addresses
            if (!networkConfig.ip.fromString(doc["ip"] | ""))
            {
                server.send(400, "application/json", "{\"error\":\"Invalid IP address\"}");
                return;
            }
            if (!networkConfig.subnet.fromString(doc["subnet"] | ""))
            {
                server.send(400, "application/json", "{\"error\":\"Invalid subnet mask\"}");
                return;
            }
            if (!networkConfig.gateway.fromString(doc["gateway"] | ""))
            {
                server.send(400, "application/json", "{\"error\":\"Invalid gateway\"}");
                return;
            }
            if (!networkConfig.dns.fromString(doc["dns"] | ""))
            {
                server.send(400, "application/json", "{\"error\":\"Invalid DNS server\"}");
                return;
            }
        }

        // Update hostname
        strlcpy(networkConfig.hostname, doc["hostname"] | "open-reactor", sizeof(networkConfig.hostname));

        // Update NTP server
        strlcpy(networkConfig.ntpServer, doc["ntp"] | "pool.ntp.org", sizeof(networkConfig.ntpServer));

        // Update DST setting if provided
        if (doc.containsKey("dst")) {
            networkConfig.dstEnabled = doc["dst"];
        }

        // Save configuration to storage
        saveNetworkConfig();

        // Send success response before applying changes
        server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configuration saved\"}");

        // Apply new configuration after a short delay
        delay(1000);
        rp2040.reboot();
    });
}
