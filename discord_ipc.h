#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdio>
#include <ctime>

class DiscordIPC {
private:
    HANDLE hPipe;
    std::string client_id;
    bool connected;

    bool SendFrame(int opcode, const std::string& payload) {
        if (!connected || hPipe == INVALID_HANDLE_VALUE) return false;
        
        struct {
            int32_t opcode;
            int32_t length;
        } header;
        
        header.opcode = opcode;
        header.length = (int32_t)payload.length();
        
        DWORD written = 0;
        if (!WriteFile(hPipe, &header, sizeof(header), &written, nullptr)) {
            Disconnect();
            return false;
        }
        if (!WriteFile(hPipe, payload.c_str(), header.length, &written, nullptr)) {
            Disconnect();
            return false;
        }
        return true;
    }

public:
    DiscordIPC(const std::string& cid) : hPipe(INVALID_HANDLE_VALUE), client_id(cid), connected(false) {}

    ~DiscordIPC() { Disconnect(); }

    bool Connect() {
        if (connected) return true;
        
        hPipe = CreateFileA("\\\\.\\pipe\\discord-ipc-0", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hPipe == INVALID_HANDLE_VALUE) return false;
        
        connected = true;
        
        // Send handshake
        char payload[256];
        snprintf(payload, sizeof(payload), "{\"v\": 1, \"client_id\": \"%s\"}", client_id.c_str());
        
        if (!SendFrame(0, payload)) {
            return false;
        }
        
        // Read handshake response (we don't strictly need to parse it)
        struct {
            int32_t opcode;
            int32_t length;
        } header;
        DWORD read = 0;
        if (ReadFile(hPipe, &header, sizeof(header), &read, nullptr)) {
            if (header.length > 0) {
                std::vector<char> buf(header.length);
                ReadFile(hPipe, buf.data(), header.length, &read, nullptr);
            }
        }
        
        return true;
    }

    void Disconnect() {
        if (hPipe != INVALID_HANDLE_VALUE) {
            CloseHandle(hPipe);
            hPipe = INVALID_HANDLE_VALUE;
        }
        connected = false;
    }

    void SetActivity(const std::string& state, const std::string& details, const std::string& large_image, const std::string& large_text, const std::string& small_image, const std::string& small_text, long long start_time) {
        if (!Connect()) return;
        
        DWORD pid = GetCurrentProcessId();
        
        // Escape strings
        auto escape = [](std::string s) {
            std::string res;
            for (char c : s) {
                if (c == '"' || c == '\\') res += '\\';
                res += c;
            }
            return res;
        };

        std::string e_state = escape(state);
        std::string e_details = escape(details);
        std::string e_large_text = escape(large_text);
        std::string e_small_text = escape(small_text);

        char payload[2048];
        int len = snprintf(payload, sizeof(payload), 
            "{\"cmd\": \"SET_ACTIVITY\", \"args\": {\"pid\": %lu, \"activity\": {"
            "\"details\": \"%s\", \"state\": \"%s\", ",
            pid, e_details.c_str(), e_state.c_str());

        if (start_time > 0) {
            len += snprintf(payload + len, sizeof(payload) - len, "\"timestamps\": {\"start\": %lld}, ", start_time);
        }

        len += snprintf(payload + len, sizeof(payload) - len, 
            "\"assets\": {\"large_image\": \"%s\", \"large_text\": \"%s\"",
            large_image.c_str(), e_large_text.c_str());

        if (!small_image.empty()) {
            len += snprintf(payload + len, sizeof(payload) - len, 
                ", \"small_image\": \"%s\", \"small_text\": \"%s\"",
                small_image.c_str(), e_small_text.c_str());
        }

        snprintf(payload + len, sizeof(payload) - len, 
            "}}}, \"nonce\": \"1\"}");

        SendFrame(1, payload);
    }
};
