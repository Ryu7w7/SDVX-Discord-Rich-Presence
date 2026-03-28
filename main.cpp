#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <fstream>
#include <regex>
#include <io.h>
#include <fcntl.h>
#include <mutex>
#pragma comment(lib, "urlmon.lib")
#include <urlmon.h>
#include "discord_ipc.h"

// --- CONFIGURATION ---
const std::string CLIENT_ID = "YOUR_CLIENT_ID_HERE";
const std::string IMG_MENU = "sdvx_nabla";
const std::string PLAYER_NAME = "SDVX ∇";

// --- REMOTE SERVER CONFIGURATION (OPTIONAL) ---
// Uncomment to use remote server jackets and music_db
// const std::string IMG_DEFAULT = "https://your-domain.com/sdvx/jk_dummy.png";
// const std::string IMG_PLAYING = "https://your-domain.com/sdvx/jk_dummy.png";
// const std::string JACKET_BASE_URL = "https://your-domain.com/sdvx";
// const std::string MUSIC_DB_URL = "https://your-domain.com/music_db.xml";
// const std::string MUSIC_DB_MERGED_URL = "https://your-domain.com/music_db.merged.xml";

// Default URLs
const std::string IMG_DEFAULT = "";
const std::string IMG_PLAYING = "";
const std::string JACKET_BASE_URL = "";
const std::string MUSIC_DB_URL = "";
const std::string MUSIC_DB_MERGED_URL = "";

// --- STATE ---
struct SongData {
    std::string title;
};

std::string current_song = "...";
std::string current_song_id = "";
std::string current_jacket_path = "";
std::string current_state = "Menu";
std::string play_mode = "";
std::string active_event = "";
std::string sub_menu = "";
long long start_time = 0;
long long session_start_time = 0;
std::map<int, SongData> song_map;
std::mutex song_map_mutex;

DiscordIPC rpc(CLIENT_ID);

std::string sjis_to_utf8(const std::string& sjis) {
    if (sjis.empty()) return "";
    int wlen = MultiByteToWideChar(932, 0, sjis.c_str(), -1, NULL, 0);
    if (wlen == 0) return sjis;
    std::vector<wchar_t> wbuf(wlen);
    MultiByteToWideChar(932, 0, sjis.c_str(), -1, &wbuf[0], wlen);

    int ulen = WideCharToMultiByte(CP_UTF8, 0, &wbuf[0], -1, NULL, 0, NULL, NULL);
    if (ulen == 0) return sjis;
    std::vector<char> ubuf(ulen);
    WideCharToMultiByte(CP_UTF8, 0, &wbuf[0], -1, &ubuf[0], ulen, NULL, NULL);

    return std::string(&ubuf[0]);
}

std::string get_image_key(const std::string& state) {
    if (state == "Menu" || state == "Selecting") return IMG_MENU;
    if (state == "Playing") return IMG_PLAYING.empty() ? IMG_MENU : IMG_PLAYING;
    return IMG_DEFAULT.empty() ? IMG_MENU : IMG_DEFAULT;
}

long long get_time_sec() {
    return std::time(nullptr);
}

void update_presence() {
    std::string state_txt, details_txt, large_text;
    std::string img_key = get_image_key(current_state);

    if (current_state == "Playing" || current_state == "Results") {
        if (!current_jacket_path.empty() && !JACKET_BASE_URL.empty()) {
            img_key = JACKET_BASE_URL + "/" + current_jacket_path;
        } else if (JACKET_BASE_URL.empty()) {
            img_key = IMG_MENU;
        }
    }

    // Prevents visual spam during Megamix Battle
    if (play_mode == "Megamix Battle") {
        img_key = IMG_MENU;
    }

    std::string small_img = IMG_MENU;
    std::string small_txt = play_mode.empty() ? "SDVX" : play_mode;

    if (current_state == "Playing") {
        details_txt = play_mode.empty() ? "Playing" : play_mode;
        if (!active_event.empty()) details_txt = active_event;
        
        state_txt = "Playing: " + current_song;
        if (current_song == "Browsing..." || current_song == "...") state_txt = "Loading...";
        large_text = current_song;
        
        if (play_mode == "Megamix Battle") {
            details_txt = "Megamix Battle";
            state_txt = "Playing";
            large_text = "Megamix Battle";
            small_img = "";
            small_txt = "";
        }

        rpc.SetActivity(state_txt, details_txt, img_key, large_text, small_img, small_txt, session_start_time);
    } 
    else if (current_state == "Selecting") {
        details_txt = play_mode.empty() ? "Selecting Song" : play_mode;
        if (!active_event.empty()) details_txt = active_event;

        state_txt = "Selecting: " + current_song;
        if (current_song == "Browsing..." || current_song == "...") state_txt = "Choosing Song...";
        large_text = "SDVX ∇";

        if (play_mode == "Megamix Battle") {
            details_txt = "Megamix Battle";
            state_txt = "Selecting Tracks";
        }

        rpc.SetActivity(state_txt, details_txt, img_key, large_text, "", "", session_start_time);
    }
    else if (current_state == "Results") {
        details_txt = play_mode.empty() ? "Results" : play_mode;
        if (!active_event.empty()) details_txt = active_event;

        state_txt = "Result: " + current_song;
        large_text = current_song;

        if (play_mode == "Megamix Battle") {
            details_txt = "Megamix Battle";
            state_txt = "Result";
            large_text = "Megamix Battle";
        }

        rpc.SetActivity(state_txt, details_txt, img_key, large_text, "", "", session_start_time);
    }
    else if (current_state == "TotalResults") {
        rpc.SetActivity("Session Results", "SDVX", IMG_MENU, "SDVX ∇", "", "", session_start_time);
    }
    else /* Menu */ {
        std::string menu_txt = sub_menu.empty() ? "In Menu" : sub_menu;
        rpc.SetActivity(menu_txt, "SDVX", IMG_MENU, "SDVX ∇", "", "", session_start_time);
    }
}

void parse_xml_file(const std::string& xml_path) {
    std::ifstream file(xml_path, std::ios::binary);
    if (!file.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    bool is_utf8 = (content.find("encoding=\"UTF-8\"") != std::string::npos || content.find("encoding=\"utf-8\"") != std::string::npos);

    size_t pos = 0;
    while ((pos = content.find("<music id=\"", pos)) != std::string::npos) {
        size_t end_pos = content.find("</music>", pos);
        if (end_pos == std::string::npos) break;
        
        std::string block = content.substr(pos, end_pos - pos);
        
        // Extract ID
        size_t id_end = block.find("\"", 11);
        int id = 0;
        try { id = std::stoi(block.substr(11, id_end - 11)); } catch(...) { pos = end_pos; continue; }

        // Extract Title
        std::regex rx_title("<title_name.*?>(.*?)</title_name>");
        std::smatch m_title;
        std::string title;
        if (std::regex_search(block, m_title, rx_title)) {
            title = m_title[1];
            title.erase(0, title.find_first_not_of(" \t\r\n"));
            title.erase(title.find_last_not_of(" \t\r\n") + 1);
            if (!is_utf8) title = sjis_to_utf8(title);
        } else { title = std::to_string(id); }

        SongData sd;
        sd.title = title;

        {
            std::lock_guard<std::mutex> lock(song_map_mutex);
            song_map[id] = sd;
        }
        pos = end_pos;
    }
}

void fetch_and_parse_xml(const std::string& url) {
    char temp_path[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_path);
    std::string temp_file = std::string(temp_path) + "sdvx_temp_db_" + std::to_string(GetTickCount()) + ".xml";

    HRESULT hr = URLDownloadToFileA(NULL, url.c_str(), temp_file.c_str(), 0, NULL);
    if (hr == S_OK) {
        parse_xml_file(temp_file);
        DeleteFileA(temp_file.c_str());
    }
}

void load_song_map_thread() {
    if (!MUSIC_DB_URL.empty()) fetch_and_parse_xml(MUSIC_DB_URL);
    if (!MUSIC_DB_MERGED_URL.empty()) fetch_and_parse_xml(MUSIC_DB_MERGED_URL);

    bool has_data = false;
    {
        std::lock_guard<std::mutex> lock(song_map_mutex);
        has_data = !song_map.empty();
    }

    if (!has_data) {
        std::vector<std::string> possible_paths = {
            "data/others/music_db.xml",
            "../data/others/music_db.xml",
            "others/music_db.xml",
            "music_db.xml"
        };

        std::vector<std::string> merged_paths = {
            "data_mods/omnimix/others/music_db.merged.xml",
            "../data_mods/omnimix/others/music_db.merged.xml",
            "omnimix/others/music_db.merged.xml",
            "music_db.merged.xml"
        };

        for (const auto& p : possible_paths) {
            if (std::ifstream(p).good()) {
                parse_xml_file(p);
                break;
            }
        }

        for (const auto& p : merged_paths) {
            if (std::ifstream(p).good()) {
                parse_xml_file(p);
                break;
            }
        }
    }
}

void parse_line(const std::string& line) {
    // 1. Detect Play Mode
    if (line.find("ea3_report_posev") != std::string::npos && line.find("/coin/kfc_game_s_") != std::string::npos) {
        std::string new_mode;
        if (line.find("light") != std::string::npos) new_mode = "Light Start";
        else if (line.find("standard_plus") != std::string::npos) new_mode = "Normal Start";
        else if (line.find("standard") != std::string::npos) new_mode = "Normal Start";
        else if (line.find("premium") != std::string::npos) new_mode = "Premium Time";
        else if (line.find("blaster") != std::string::npos) new_mode = "Blaster Start";
        else if (line.find("paradise") != std::string::npos) new_mode = "Paradise Start";
        else if (line.find("arena") != std::string::npos) new_mode = "Arena Battle";
        else if (line.find("megamix") != std::string::npos) new_mode = "Megamix Battle";

        if (!new_mode.empty()) {
            play_mode = new_mode;
        }
    }

    // 2. Detect Hexa Diver
    if (line.find("LoadingIFS") != std::string::npos && line.find("hexa_diver") != std::string::npos && line.find("blue") != std::string::npos) {
        if (active_event != "Hexa Diver") {
            active_event = "Hexa Diver";
            current_song = "Browsing...";
            current_state = "Selecting";
            update_presence();
        }
    }
    if (line.find("LoadingIFS") != std::string::npos && line.find("ver06/ms_sel") != std::string::npos) {
        if (active_event == "Hexa Diver") active_event = "";
    }

    // 3. Detect Song & Difficulty from Images securely
    if (line.find("Loading /data/music/") != std::string::npos && line.find(".png") != std::string::npos) {
        
        std::regex rx_song("music/(\\d+)_");
        std::smatch match_song;
        if (std::regex_search(line, match_song, rx_song)) {
            std::string sid_str = match_song[1];
            size_t png_pos = line.find(".png");
            
            // Resolution of Active Song (Only sync ID on definitive background loads or explicit playtime states)
            bool is_bg_load = line.find("_b.png") != std::string::npos;
            if (is_bg_load || current_state == "Playing" || active_event == "Hexa Diver") {
                current_song_id = sid_str;
                if (png_pos > 0 && line.find("Loading /data/music/") != std::string::npos) {
                    current_jacket_path = line.substr(line.find("Loading /data/music/") + 20, (png_pos + 4) - (line.find("Loading /data/music/") + 20));
                }
            }
            
            // D. Push Notification Array safely
            if (is_bg_load || current_state == "Playing") {
                try {
                    int sid = std::stoi(sid_str);
                    std::string song_name;
                    {
                         std::lock_guard<std::mutex> lock(song_map_mutex);
                         song_name = song_map.count(sid) ? song_map[sid].title : std::to_string(sid);
                    }
                    if (song_name != current_song || current_state == "Playing") {
                        current_song = song_name;
                        update_presence(); 
                    }
                } catch(...) {}
            }
        }
    }

    if (line.find("in MUSICSELECT") != std::string::npos) {
        if (active_event == "Hexa Diver" && line.find("ms_sel") != std::string::npos) active_event = "";
        if (current_state != "Selecting") {
            current_state = "Selecting";
            if (current_song == "..." && active_event != "Hexa Diver") current_song = "Browsing...";
            
            update_presence();
        }
    }

    if (line.find("in ALTERNATIVE_GAME_SCENE") != std::string::npos ||
        line.find("in MEGAMIX_GAME_SCENE") != std::string::npos ||
        line.find("in MEGAMIX_BATTLE") != std::string::npos ||
        line.find("in BATTLE_GAME_SCENE") != std::string::npos ||
        line.find("in AUTOMATION_GAME_SCENE") != std::string::npos ||
        line.find("in ARENA_GAME_SCENE") != std::string::npos ||
        line.find("game_bg/") != std::string::npos) {
        if (current_state != "Playing") {
            current_state = "Playing";
            start_time = get_time_sec();
            update_presence();
        }
    }

    if (line.find("in RESULT_SCENE") != std::string::npos) {
        if (current_state != "Results") {
            current_state = "Results";
            update_presence();
        }
    }

    if (line.find("in T_RESULT_SCENE") != std::string::npos) {
        if (current_state != "TotalResults") {
            current_state = "TotalResults";
            sub_menu = "";
            update_presence();
        }
    }

    if (line.find("in GAMEOVER") != std::string::npos || line.find("in CARD_OUT_SCENE") != std::string::npos || line.find("in TITLEDEMO") != std::string::npos) {
        if (current_state != "Menu") {
            current_state = "Menu";
            play_mode = "";
            active_event = "";
            sub_menu = "";
            current_song_id = "";
            current_jacket_path = "";
            start_time = get_time_sec();
            update_presence();
        }
    }

    if (current_state == "Menu" && line.find("in ") != std::string::npos) {
        if (line.find("GENERATOR") != std::string::npos) {
            sub_menu = "Card Generator";
            update_presence();
        } else if (line.find("SKILL_ANALYZER") != std::string::npos) {
            sub_menu = "Skill Analyzer";
            update_presence();
        } else if (line.find("CREW_SELECT") != std::string::npos || line.find("CREW") != std::string::npos) {
            sub_menu = "Crew Selection";
            update_presence();
        }
    }
}

// Redirect stdout to our pipe
HANDLE hOriginalStdout = NULL;
HANDLE hReadPipe = NULL;
HANDLE hWritePipe = NULL;

void log_reader_thread() {
    char buffer[4096];
    DWORD bytesRead;
    
    // Connect to Discord
    rpc.Connect();
    std::thread(load_song_map_thread).detach();
    start_time = get_time_sec();
    session_start_time = start_time;
    update_presence();

    std::string line_buffer;

    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
        if (bytesRead == 0) continue;
        buffer[bytesRead] = '\0';
        
        // Pass through to original stdout if there was one
        if (hOriginalStdout && hOriginalStdout != INVALID_HANDLE_VALUE && hOriginalStdout != hWritePipe) {
            DWORD written;
            WriteFile(hOriginalStdout, buffer, bytesRead, &written, NULL);
        }

        line_buffer += buffer;
        size_t pos_nl = 0;
        while ((pos_nl = line_buffer.find('\n')) != std::string::npos) {
            std::string line = line_buffer.substr(0, pos_nl);
            line_buffer.erase(0, pos_nl + 1);
            
            // Remove \r if present
            if (!line.empty() && line.back() == '\r') line.pop_back();

            parse_line(line);
        }
    }
}

void setup_stdout_hook() {
    // Save original stdout
    hOriginalStdout = GetStdHandle(STD_OUTPUT_HANDLE);

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return;
    }

    SetStdHandle(STD_OUTPUT_HANDLE, hWritePipe);
    SetStdHandle(STD_ERROR_HANDLE, hWritePipe);

    // Some programs use C runtime stdout instead of standard handles directly.
    int fd = _open_osfhandle((intptr_t)hWritePipe, _O_TEXT);
    if (fd != -1) {
        FILE* fp = _fdopen(fd, "w");
        if (fp) {
            *stdout = *fp;
            *stderr = *fp;
            setvbuf(stdout, NULL, _IONBF, 0);       
            setvbuf(stderr, NULL, _IONBF, 0);
        }
    }

    std::thread(log_reader_thread).detach();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        setup_stdout_hook();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
