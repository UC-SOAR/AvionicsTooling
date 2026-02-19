#ifndef NEW_TEST_RUNCAM_REGISTRY_H
#define NEW_TEST_RUNCAM_REGISTRY_H

#include <cstdint>
#include <vector>

// --- PROTOCOL CONSTANTS ---
const uint8_t RC_HEADER = 0xCC;

// --- 1. COMMAND IDs (The "Category") ---
enum CommandID : uint8_t {
    CMD_GET_INFO        = 0x00,
    CMD_CAM_CONTROL     = 0x01,
    CMD_5KEY_PRESS      = 0x02,
    CMD_5KEY_RELEASE    = 0x03,
    CMD_OSD_HANDSHAKE   = 0x04,
    CMD_GET_SETTINGS    = 0x10,
    CMD_READ_DETAIL     = 0x11,
    CMD_WRITE_SETTING   = 0x13,
    CMD_WRITE_STRING    = 0x22
};

// --- 2. CAMERA CONTROL ACTIONS (Payloads for CMD 0x01) ---
enum ControlAction : uint8_t {
    ACT_WIFI_BTN        = 0x00,
    ACT_POWER_BTN       = 0x01,
    ACT_CHANGE_MODE     = 0x02,
    ACT_START_REC       = 0x03,
    ACT_STOP_REC        = 0x04
};

// --- 3. 5-KEY OSD ACTIONS (Payloads for CMD 0x02) ---
enum OSDAction : uint8_t {
    OSD_ENTER           = 0x01,
    OSD_LEFT            = 0x02,
    OSD_RIGHT           = 0x03,
    OSD_UP              = 0x04,
    OSD_DOWN            = 0x05
};

// --- 4. PRE-CALCULATED PACKETS ---
namespace RunCamHex {
    const std::vector<uint8_t> GET_INFO   = {0xCC, 0x00, 0x00};
    const std::vector<uint8_t> START_REC  = {0xCC, 0x01, 0x03};
    const std::vector<uint8_t> STOP_REC   = {0xCC, 0x01, 0x04};
    const std::vector<uint8_t> OSD_OPEN   = {0xCC, 0x04, 0x01};
    const std::vector<uint8_t> OSD_CLOSE  = {0xCC, 0x04, 0x02};
    const std::vector<uint8_t> CHANGE_MODE = {RC_HEADER, CMD_CAM_CONTROL, ACT_CHANGE_MODE};
    const std::vector<uint8_t> WIFI_BTN = {RC_HEADER, CMD_CAM_CONTROL, ACT_WIFI_BTN};
    const std::vector<uint8_t> OSD_ENTER_PRESS = {RC_HEADER, CMD_5KEY_PRESS, OSD_ENTER};
    const std::vector<uint8_t> OSD_ENTER_RELEASE = {RC_HEADER, CMD_5KEY_RELEASE, OSD_ENTER};
    const std::vector<uint8_t> GET_SETTINGS = {RC_HEADER, CMD_GET_SETTINGS, 0x00};
    const std::vector<uint8_t> READ_DETAIL = {RC_HEADER, CMD_READ_DETAIL, 0x00};
    const std::vector<uint8_t> WRITE_SETTING = {RC_HEADER, CMD_WRITE_SETTING, 0x00};
}

#endif // NEW_TEST_RUNCAM_REGISTRY_H
