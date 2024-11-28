/**
 * @file wardriving.cpp
 * @author IncursioHack - https://github.com/IncursioHack
 * @brief WiFi Wardriving Implementation
 * @version 0.3
 * @note Updated: 2024-11-28
 */

#include "wardriving.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/wifi_common.h"
#include "core/sd_functions.h"
#include <ctype.h>

#define MAX_WAIT 5000
#define CURRENT_YEAR 2024
#define FS_CHECK_INTERVAL 300000 // Check file system every 5 minutes

Wardriving::Wardriving() {
    setup();
}

Wardriving::~Wardriving() {
    if (gpsConnected) end();
}

void Wardriving::setup() {
    display_banner();
    padprintln("Initializing...");

    begin_wifi();
    if (!begin_gps()) return;

    // Initialize MAC address tracking system
    if (!initializeIndex()) {
        padprintln("Failed to initialize index file");
        end();
        return;
    }

    delay(500);
    return loop();
}

void Wardriving::begin_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
}

int Wardriving::getGPSBaudRate() {
    return bruceConfig.gpsModule == GPS_M5STACK_V1_1 ? 115200 : 9600;
}

bool Wardriving::begin_gps() {
    int baudRate = getGPSBaudRate();
    GPSserial.begin(baudRate, SERIAL_8N1, SERIAL_RX, SERIAL_TX);

    int count = 0;
    padprintln("Waiting for GPS data");
    padprintf("GPS Module: %s\n", bruceConfig.gpsModule == GPS_M5STACK_V1_1 ? "M5Stack GPS 1.1" : "Generic GPS");
    padprintf("Baud Rate: %d\n", baudRate);

    while(GPSserial.available() <= 0) {
        if(checkEscPress()) {
            end();
            return false;
        }
        displayRedStripe("Waiting GPS: " + String(count)+ "s", TFT_WHITE, bruceConfig.priColor);
        count++;
        delay(1000);
    }

    gpsConnected = true;
    return true;
}

bool Wardriving::checkFileSystem() {
    FS *fs;
    if(!getFsStorage(fs)) return false;
    
    if (!(*fs).exists("/BruceWardriving")) {
        if (!(*fs).mkdir("/BruceWardriving")) return false;
    }
    
    return true;
}

void Wardriving::end() {
    // Close any open file handles
    FS *fs;
    if(getFsStorage(fs)) {
        // Close any open index file
        if ((*fs).exists(indexFilePath)) {
            File indexFile = (*fs).open(indexFilePath, FILE_READ);
            if (indexFile) indexFile.close();
        }
        
        // Close current data file if exists
        if (filename != "" && (*fs).exists("/BruceWardriving/"+filename)) {
            File dataFile = (*fs).open("/BruceWardriving/"+filename, FILE_READ);
            if (dataFile) dataFile.close();
        }
    }

    wifiDisconnect();
    GPSserial.end();
    
    // Reset file system state
    indexFileInitialized = false;
    
    returnToMenu = true;
    gpsConnected = false;
    delay(500);
}

void Wardriving::loop() {
    int count = 0;
    unsigned long lastFsCheck = 0;
    returnToMenu = false;
    
    while(1) {
        display_banner();

        if (checkEscPress() || returnToMenu) return end();

        // Periodic file system check
        unsigned long currentTime = millis();
        if (currentTime - lastFsCheck >= FS_CHECK_INTERVAL) {
            if (checkFileSystem()) {
                initializeIndex(); // Re-initialize index file if needed
            }
            lastFsCheck = currentTime;
        }

        if (GPSserial.available() > 0) {
            count = 0;
            while (GPSserial.available() > 0) gps.encode(GPSserial.read());

            if (gps.location.isUpdated()) {
                padprintln("GPS location updated");
                set_position();
                scan_networks();
            } else {
                padprintln("GPS location not updated");
                dump_gps_data();

                if (filename == "" && gps.date.year() >= CURRENT_YEAR && gps.date.year() < CURRENT_YEAR+5)
                    create_filename();
            }
        } else {
            if (count > 5) {
                displayError("GPS not Found!");
                return end();
            }
            padprintln("No GPS data available");
            count++;
        }

        int tmp = millis();
        while(millis()-tmp < MAX_WAIT && !gps.location.isUpdated()){
            if (checkEscPress() || returnToMenu) return end();
        }
    }
}

void Wardriving::set_position() {
    double lat = gps.location.lat();
    double lng = gps.location.lng();

    if (initial_position_set) distance += gps.distanceBetween(cur_lat, cur_lng, lat, lng);
    else initial_position_set = true;

    cur_lat = lat;
    cur_lng = lng;
}

void Wardriving::display_banner() {
    drawMainBorderWithTitle("Wardriving");
    padprintln("");

    if (wifiNetworkCount > 0){
        padprintln("File: " + filename.substring(0, filename.length()-4), 2);
        padprintln("Unique Networks Found: " + String(wifiNetworkCount), 2);
        padprintf(2, "Distance: %.2fkm\n", distance / 1000);
    }

    padprintln("");
}

void Wardriving::dump_gps_data() {
    if (!date_time_updated && (!gps.date.isUpdated() || !gps.time.isUpdated())) {
        padprintln("Waiting for valid GPS data");
        return;
    }
    date_time_updated = true;
    padprintf(2, "Date: %02d-%02d-%02d\n", gps.date.year(), gps.date.month(), gps.date.day());
    padprintf(2, "Time: %02d:%02d:%02d\n", gps.time.hour(), gps.time.minute(), gps.time.second());
    padprintf(2, "Sat:  %d\n", gps.satellites.value());
    padprintf(2, "HDOP: %.2f\n", gps.hdop.hdop());
}

String Wardriving::auth_mode_to_string(wifi_auth_mode_t authMode) {
    switch (authMode) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK: return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA_WPA2_PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK: return "WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2_WPA3_PSK";
        case WIFI_AUTH_WAPI_PSK: return "WAPI_PSK";
        default: return "UNKNOWN";
    }
}

void Wardriving::scan_networks() {
    wifiConnected = true;

    int network_amount = WiFi.scanNetworks();
    if (network_amount == 0) {
        padprintln("No Wi-Fi networks found", 2);
        return;
    }

    padprintf(2, "Coord: %.6f, %.6f\n", gps.location.lat(), gps.location.lng());
    padprintln("Networks Found: " + String(network_amount), 2);

    return append_to_file(network_amount);
}

void Wardriving::create_filename() {
    char timestamp[20];
    sprintf(
        timestamp,
        "%02d%02d%02d_%02d%02d%02d",
        gps.date.year(),
        gps.date.month(),
        gps.date.day(),
        gps.time.hour(),
        gps.time.minute(),
        gps.time.second()
    );
    filename = String(timestamp) + "_wardriving.csv";
}

bool Wardriving::isValidMacString(const String& mac) {
    if (mac.length() != 17) return false;
    
    for (size_t i = 0; i < 17; i++) {
        if (i % 3 == 2) {
            if (mac[i] != ':') return false;
        } else {
            if (!isxdigit(mac[i])) return false;
        }
    }
    return true;
}

bool Wardriving::macStringToBytes(const String& mac, uint8_t* bytes) {
    if (!isValidMacString(mac)) return false;
    
    for (size_t i = 0, byteIndex = 0; i < mac.length(); i += 3, byteIndex++) {
        char hexPair[3] = {mac[i], mac[i+1], '\0'};
        bytes[byteIndex] = strtol(hexPair, NULL, 16);
    }
    return true;
}

String Wardriving::bytesToMacString(const uint8_t* bytes) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
    return String(macStr);
}

bool Wardriving::initializeIndex() {
    if (indexFileInitialized) return true;
    
    if (!checkFileSystem()) return false;
    
    FS *fs;
    if (!getFsStorage(fs)) return false;

    if (!(*fs).exists(indexFilePath)) {
        File indexFile = (*fs).open(indexFilePath, FILE_WRITE);
        if (!indexFile) return false;
        indexFile.close();
        }
    
    indexFileInitialized = true;
    return true;
}

bool Wardriving::isMacInCache(const String& mac) {
    return macAddressCache.find(mac) != macAddressCache.end();
}

bool Wardriving::isMacInIndex(const String& mac) {
    if (!indexFileInitialized && !initializeIndex()) return false;
    
    FS *fs;
    if(!getFsStorage(fs)) return false;
    
    File indexFile = (*fs).open(indexFilePath, FILE_READ);
    if (!indexFile) return false;

    uint8_t searchBytes[BLOCK_SIZE];
    if (!macStringToBytes(mac, searchBytes)) {
        indexFile.close();
        return false;
    }

    uint8_t fileBytes[BLOCK_SIZE];
    bool found = false;
    
    while (indexFile.available() >= BLOCK_SIZE) {
        if (indexFile.read(fileBytes, BLOCK_SIZE) != BLOCK_SIZE) break;
        
        if (memcmp(searchBytes, fileBytes, BLOCK_SIZE) == 0) {
            found = true;
            break;
        }
    }
    
    indexFile.close();
    return found;
}

void Wardriving::addMacToCache(const String& mac) {
    macAddressCache.insert(mac);
    maintainCache();
}

bool Wardriving::addMacToIndex(const String& mac) {
    if (!indexFileInitialized && !initializeIndex()) return false;
    
    FS *fs;
    if(!getFsStorage(fs)) return false;
    
    File indexFile = (*fs).open(indexFilePath, FILE_APPEND);
    if (!indexFile) return false;
    
    uint8_t bytes[BLOCK_SIZE];
    if (!macStringToBytes(mac, bytes)) {
        indexFile.close();
        return false;
    }
    
    bool success = (indexFile.write(bytes, BLOCK_SIZE) == BLOCK_SIZE);
    indexFile.close();
    return success;
}

void Wardriving::maintainCache() {
    if (macAddressCache.size() > CACHE_CLEAN_THRESHOLD) {
        size_t targetSize = CACHE_SIZE / 2;
        while (macAddressCache.size() > targetSize) {
            macAddressCache.erase(macAddressCache.begin());
        }
    }
}

void Wardriving::append_to_file(int network_amount) {
    if (!checkFileSystem()) {
        padprintln("Storage setup error");
        returnToMenu = true;
        return;
    }

    FS *fs;
    if(!getFsStorage(fs)) {
        padprintln("Storage access error");
        returnToMenu = true;
        return;
    }

    if (filename == "") create_filename();

    bool is_new_file = !(*fs).exists("/BruceWardriving/"+filename);
    File file = (*fs).open("/BruceWardriving/"+filename, is_new_file ? FILE_WRITE : FILE_APPEND);

    if (!file) {
        padprintln("Failed to open file for writing");
        returnToMenu = true;
        return;
    }

    if (is_new_file) {
        file.println("WigleWifi-1.6,appRelease=v"+String(BRUCE_VERSION)+",model=M5Stack GPS Unit,release=v"+String(BRUCE_VERSION)+",device=ESP32 M5Stack,display=SPI TFT,board=ESP32 M5Stack,brand=Bruce,star=Sol,body=4,subBody=1");
        file.println("MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type");
    }

    for (int i = 0; i < network_amount; i++) {
        String macAddress = WiFi.BSSIDstr(i);
        String ssid = WiFi.SSID(i);
        
        // Validate MAC address format
        if (!isValidMacString(macAddress)) {
            padprintln("Invalid MAC format: " + macAddress);
            continue;
        }

        // Skip empty or invalid SSIDs
        if (ssid.length() == 0) {
            continue;
        }

        // Check both cache and index for the MAC address
        if (!isMacInCache(macAddress) && !isMacInIndex(macAddress)) {
            // Add to cache first
            addMacToCache(macAddress);
            
            // Try to add to index file
            if (!addMacToIndex(macAddress)) {
                padprintln("Failed to add MAC to index: " + macAddress);
                continue;
            }
            
            // Get channel information
            int32_t channel = WiFi.channel(i);
            int32_t frequency = channel != 14 ? 2407 + (channel * 5) : 2484;

            // Format date and time first to avoid multiple function calls
            char datetime[20];
            snprintf(datetime, sizeof(datetime), "%04d-%02d-%02d %02d:%02d:%02d",
                gps.date.year(), gps.date.month(), gps.date.day(),
                gps.time.hour(), gps.time.minute(), gps.time.second());

            // Create main buffer with formatted data
            char buffer[512];
            snprintf(buffer, sizeof(buffer), 
                "%s,%s,[%s],%s,%d,%d,%d,%.6f,%.6f,%.2f,%.2f,,,WIFI\n",
                macAddress.c_str(),
                ssid.c_str(),
                auth_mode_to_string(WiFi.encryptionType(i)).c_str(),
                datetime,
                channel,
                frequency,
                WiFi.RSSI(i),
                gps.location.lat(),
                gps.location.lng(),
                gps.altitude.meters(),
                gps.hdop.hdop() * 1.0
            );

            // Write to file
            if (file.print(buffer)) {
                wifiNetworkCount++;
            } else {
                padprintln("Failed to write to file");
            }
        }
    }

    file.close();
}