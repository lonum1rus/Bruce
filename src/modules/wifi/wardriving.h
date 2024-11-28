/**
 * @file wardriving.h
 * @author IncursioHack - https://github.com/IncursioHack
 * @brief WiFi Wardriving
 * @version 0.3
 * @note Updated: 2024-11-28
 */

#ifndef WAR_DRIVING_H
#define WAR_DRIVING_H

#include "core/globals.h"
#include "core/config.h"  // Include config.h for GPSModules enum
#include <TinyGPS++.h>
#include <set>
#include <esp_wifi_types.h>

class Wardriving {
public:
    /////////////////////////////////////////////////////////////////////////////////////
    // Constructor
    /////////////////////////////////////////////////////////////////////////////////////
    Wardriving();
    ~Wardriving();

    /////////////////////////////////////////////////////////////////////////////////////
    // Life Cycle
    /////////////////////////////////////////////////////////////////////////////////////
    void setup();
    void loop();

private:
    bool date_time_updated = false;
    bool initial_position_set = false;
    double cur_lat;
    double cur_lng;
    double distance = 0;
    String filename = "";
    TinyGPSPlus gps;
    HardwareSerial GPSserial = HardwareSerial(2);     // Uses UART2 for GPS
    
    /////////////////////////////////////////////////////////////////////////////////////
    // MAC Address Tracking System
    /////////////////////////////////////////////////////////////////////////////////////
    static const size_t CACHE_SIZE = 1000;          // Maximum number of MAC addresses to keep in memory
    static const size_t BLOCK_SIZE = 6;             // Size of MAC address in binary format (bytes)
    static const size_t CACHE_CLEAN_THRESHOLD = 800;// When to start cleaning cache
    std::set<String> macAddressCache;              // In-memory cache for recent MAC addresses
    String indexFilePath = "/BruceWardriving/mac_index.bin"; // Path to binary MAC address index file
    int wifiNetworkCount = 0;                      // Counter for wifi networks in current session
    bool indexFileInitialized = false;             // Track if index file is initialized

    /////////////////////////////////////////////////////////////////////////////////////
    // Setup & System Management
    /////////////////////////////////////////////////////////////////////////////////////
    void begin_wifi(void);
    bool begin_gps(void);
    void end(void);
    int getGPSBaudRate(void);        // Helper to get correct baud rate based on GPS module
    bool checkFileSystem(void);       // Check and ensure file system is mounted properly

    /////////////////////////////////////////////////////////////////////////////////////
    // Display functions
    /////////////////////////////////////////////////////////////////////////////////////
    void display_banner(void);
    void dump_gps_data(void);

    /////////////////////////////////////////////////////////////////////////////////////
    // Operations
    /////////////////////////////////////////////////////////////////////////////////////
    void set_position(void);
    void scan_networks(void);
    String auth_mode_to_string(wifi_auth_mode_t authMode);
    void append_to_file(int network_amount);
    void create_filename(void);

    /////////////////////////////////////////////////////////////////////////////////////
    // MAC Address Management
    /////////////////////////////////////////////////////////////////////////////////////
    bool isValidMacString(const String& mac);      // Validate MAC address format
    bool macStringToBytes(const String& mac, uint8_t* bytes); // Convert MAC string to bytes
    String bytesToMacString(const uint8_t* bytes); // Convert bytes to MAC string
    bool isMacInCache(const String& mac);          // Check if MAC exists in memory cache
    bool isMacInIndex(const String& mac);          // Check if MAC exists in persistent storage
    void addMacToCache(const String& mac);         // Add MAC to memory cache
    bool addMacToIndex(const String& mac);         // Add MAC to persistent storage
    bool initializeIndex();                        // Initialize the MAC address index file
    void maintainCache();                          // Manage cache size and cleanup
};

#endif // WAR_DRIVING_H