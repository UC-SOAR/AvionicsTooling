#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <windows.h>
#include <thread>
#include <chrono>
#include <functional>
#include <conio.h> 

#include "New_RunCam_registry.h" 

// --- CRC8 DVB-S2 Calculator ---
uint8_t crc8_dvb_s2(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x80) crc = (crc << 1) ^ 0xD5;
            else crc <<= 1;
        }
    }
    return crc;
}

// --- FSM States ---
enum class RunCamState { Idle, WaitingResponse, Success, Error, Timeout, Disconnected };
enum class RxState { WaitHeader, Buffering };

// --- Request Context ---
struct RunCamRequest {
    std::vector<uint8_t> packet;
    int retries;
    DWORD sendTime;
    bool awaitingResponse;
    std::function<void(const std::vector<uint8_t>&, bool)> callback;
};

// --- RunCam Serial Class ---
class RunCamSerialFSM {
public:
    bool blindMode = true; // DEFAULT ON - Send commands without waiting for response, assume success.

private:
    HANDLE hSerial = INVALID_HANDLE_VALUE;
    RunCamState state = RunCamState::Idle;
    std::queue<RunCamRequest> requestQueue;
    RunCamRequest currentRequest;
    int maxRetries = 3;
    int timeoutMs = 500;
    RxState rxState = RxState::WaitHeader;
    std::vector<uint8_t> rxBuffer;
    size_t currentExpectedLen = 5;

public:
    bool connect(const std::string& portName) {
        hSerial = CreateFileA(portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hSerial == INVALID_HANDLE_VALUE) return false;
        
        DCB dcb = {0};
        dcb.DCBlength = sizeof(dcb);
        GetCommState(hSerial, &dcb);
        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity = NOPARITY;
        SetCommState(hSerial, &dcb);
        
        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout = 20;
        timeouts.ReadTotalTimeoutConstant = 50;
        timeouts.WriteTotalTimeoutConstant = 50;
        SetCommTimeouts(hSerial, &timeouts);
        
        state = RunCamState::Idle;
        return true;
    }

    void disconnect() {
        if (hSerial != INVALID_HANDLE_VALUE) CloseHandle(hSerial);
        hSerial = INVALID_HANDLE_VALUE;
        state = RunCamState::Disconnected;
    }

    void queueRequest(const std::vector<uint8_t>& packet, std::function<void(const std::vector<uint8_t>&, bool)> cb = nullptr) {
        RunCamRequest req;
        req.packet = packet;
        req.retries = 0;
        req.sendTime = 0;
        req.awaitingResponse = false;
        req.callback = cb;
        requestQueue.push(req);
    }

    void processFSM() {
        switch (state) {
            case RunCamState::Idle:
                if (!requestQueue.empty()) {
                    currentRequest = requestQueue.front();
                    requestQueue.pop();

                    // Flush and Send
                    rxState = RxState::WaitHeader; 
                    rxBuffer.clear();
                    PurgeComm(hSerial, PURGE_RXCLEAR | PURGE_RXABORT);
                    sendCommand(currentRequest.packet, true); 
                    currentRequest.sendTime = GetTickCount();
                    
                    if (blindMode) {
                        Sleep(50); // Tiny physical delay
                        state = RunCamState::Success; 
                        if (currentRequest.callback) currentRequest.callback({}, true);
                    } else {
                        state = RunCamState::WaitingResponse;
                    }
                }
                break;

            case RunCamState::WaitingResponse:
                if (GetTickCount() - currentRequest.sendTime > timeoutMs) {
                     state = RunCamState::Timeout; // Simple timeout for now
                     if (currentRequest.callback) currentRequest.callback({}, false);
                } else if (processIncomingBytes()) {
                    state = RunCamState::Success;
                    if (currentRequest.callback) currentRequest.callback(rxBuffer, true);
                }
                break;

            case RunCamState::Success:
            case RunCamState::Timeout:
            case RunCamState::Error:
                state = RunCamState::Idle;
                break;
            default: break;
        }
    }

private:
    void sendCommand(const std::vector<uint8_t>& basePacket, bool appendCRC) {
        std::vector<uint8_t> packet = basePacket;
        if (appendCRC) {
            uint8_t crc = crc8_dvb_s2(packet.data(), packet.size());
            packet.push_back(crc);
        }
        DWORD bytesWritten;
        WriteFile(hSerial, packet.data(), packet.size(), &bytesWritten, NULL);
    }

    bool processIncomingBytes() {
        uint8_t byte;
        DWORD read;
        while (ReadFile(hSerial, &byte, 1, &read, NULL) && read > 0) {
            switch (rxState) {
                case RxState::WaitHeader:
                    if (byte == 0xCC) {
                        rxBuffer.clear();
                        rxBuffer.push_back(byte);
                        rxState = RxState::Buffering;
                    }
                    break;
                case RxState::Buffering:
                    rxBuffer.push_back(byte);
                    if (rxBuffer.size() >= currentExpectedLen) {
                        rxState = RxState::WaitHeader;
                        return validateCRC(rxBuffer);
                    }
                    break;
            }
        }
        return false;
    }

    bool validateCRC(const std::vector<uint8_t>& data) {
        if (data.size() < 2) return false;
        uint8_t crc = data.back();
        return crc8_dvb_s2(data.data(), data.size() - 1) == crc;
    }
};

// --- MAIN MENU INTERFACE ---
int main() {
    RunCamSerialFSM cam;
    cam.blindMode = true; 

    std::cout << "--- RUNCAM CONTROLLER (BLIND FSM) ---" << std::endl;
    std::cout << "Enter COM Port: ";
    std::string comPort;
    std::cin >> comPort;
    std::cin.ignore(1000, '\n'); // Clear buffer
    
    if (!cam.connect("\\\\.\\COM" + comPort)) {
        std::cerr << "ERROR: Check COM port!" << std::endl;
        return 1;
    }

    uint8_t textCmdID = 0x22; // Default to Horizontal (Can be toggled)
    bool running = true;

    while (running) {
        system("cls");
        std::cout << "--------------------------------" << std::endl;
        std::cout << "--- SOAR RUNCAM CONTROLLER ---" << std::endl;
        std::cout << "--------------------------------" << std::endl;
        std::cout << "" << std::endl;
        std::cout << " [1] Power Toggle   (0x01)" << std::endl;
        std::cout << " [2] Start Record   (0x03) - Non Functional?" << std::endl;
        std::cout << " [3] Stop Record    (0x04)- Non Functional?" << std::endl;
        std::cout << " [4] WiFi Button    (0x00)" << std::endl;
        std::cout << " [5] Change Mode    (0x05)" << std::endl;
        std::cout << " [9] Get Info       (0x00)" << std::endl;
        std::cout << "[10] Get Settings   (0x10)" << std::endl;
        std::cout << "[11] Read Detail    (0x11)" << std::endl;
        std::cout << "[12] Write Setting  (0x13)" << std::endl;
        std::cout << "--------------------------------" << std::endl;
        std::cout << " [6] OSD Menu OPEN  (Locks Camera!)" << std::endl;
        std::cout << " [7] OSD Menu CLOSE (Unlocks Camera)" << std::endl;
        std::cout << " [8] Write Text     (Requires Open [6])" << std::endl;
        std::cout << "[13] OSD ENTER PRESS" << std::endl;
        std::cout << "[14] OSD ENTER RELEASE" << std::endl;
        std::cout << "-------- OSD NAVIGATION --------" << std::endl;
        std::cout << " [w] OSD UP" << std::endl;
        std::cout << " [a] OSD LEFT" << std::endl;
        std::cout << " [s] OSD DOWN" << std::endl;
        std::cout << " [d] OSD RIGHT" << std::endl;
        std::cout << "--------------------------------" << std::endl;
        std::cout << " [p] Protocol Toggle (Current: " << (textCmdID == 0x22 ? "0x22 Horizontal" : "0x23 Vertical") << ")" << std::endl;
        std::cout << " [x] Exit" << std::endl;

        std::cout << "\n Command >> ";
        std::string input;
        std::getline(std::cin, input);
        char key = input.empty() ? 0 : input[0];

        if (input == "10") key = 'g';
        if (input == "11") key = 'h';
        if (input == "12") key = 'i';
        if (input == "13") key = 'j';
        if (input == "14") key = 'k';

        switch (key) {
            case '1': cam.queueRequest({0xCC, CMD_CAM_CONTROL, ACT_POWER_BTN}); break;
            case '2': cam.queueRequest(RunCamHex::START_REC); break;
            case '3': cam.queueRequest(RunCamHex::STOP_REC); break;
            case '4': cam.queueRequest(RunCamHex::WIFI_BTN); break;
            case '5': cam.queueRequest(RunCamHex::CHANGE_MODE); break;
            case '6':
                std::cout << "Opening OSD..." << std::endl;
                cam.queueRequest(RunCamHex::OSD_OPEN);
                break;
            case '7':
                std::cout << "Closing OSD..." << std::endl;
                cam.queueRequest(RunCamHex::OSD_CLOSE);
                break;
            case '8': {
                int x, y;
                std::string text;
                std::cout << "\n--- WRITE TEXT (" << std::hex << (int)textCmdID << ") ---" << std::endl;
                std::cout << "X (0-29): "; std::cin >> x;
                std::cout << "Y (0-15): "; std::cin >> y;
                std::cin.ignore(1000, '\n');
                std::cout << "Text: "; std::getline(std::cin, text);
                std::vector<uint8_t> packet = {0xCC, textCmdID, (uint8_t)text.length(), (uint8_t)x, (uint8_t)y};
                for(char c : text) packet.push_back((uint8_t)c);
                cam.queueRequest(packet);
                break;
            }
            case '9': cam.queueRequest(RunCamHex::GET_INFO); break;
            case 'g': cam.queueRequest(RunCamHex::GET_SETTINGS); break;
            case 'h': cam.queueRequest(RunCamHex::READ_DETAIL); break;
            case 'i': cam.queueRequest(RunCamHex::WRITE_SETTING); break;
            case 'j': cam.queueRequest(RunCamHex::OSD_ENTER_PRESS); break;
            case 'k': cam.queueRequest(RunCamHex::OSD_ENTER_RELEASE); break;
            case 'w': {
                std::vector<uint8_t> packet = {0xCC, CMD_5KEY_PRESS, OSD_UP};
                cam.queueRequest(packet);
                break;
            }
            case 'a': {
                std::vector<uint8_t> packet = {0xCC, CMD_5KEY_PRESS, OSD_LEFT};
                cam.queueRequest(packet);
                break;
            }
            case 's': {
                std::vector<uint8_t> packet = {0xCC, CMD_5KEY_PRESS, OSD_DOWN};
                cam.queueRequest(packet);
                break;
            }
            case 'd': {
                std::vector<uint8_t> packet = {0xCC, CMD_5KEY_PRESS, OSD_RIGHT};
                cam.queueRequest(packet);
                break;
            }
            case 'p':
                textCmdID = (textCmdID == 0x22) ? 0x23 : 0x22;
                break;
            case 'x': running = false; break;
        }

        std::cout << " [Sending...]";
        for(int i=0; i<5; i++) {
            cam.processFSM();
            Sleep(20);
        }
    }

    cam.disconnect();
    return 0;
}