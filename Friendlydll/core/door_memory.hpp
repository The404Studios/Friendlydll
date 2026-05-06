#pragma once
#include "../includes.hpp"
#include <unordered_map>
#include <string>
#include <fstream>

namespace door_memory {

    inline bool enabled = false;
    inline bool show_on_esp = true;

    struct DoorEntry {
        std::string code;
        std::string owner;
        float lastSeen = 0.f;
    };

    inline std::unordered_map<std::string, DoorEntry> g_posCodes; // "x,y,z" -> DoorEntry (persistent)

    inline std::string PosKey(float x, float y, float z) {
        char buf[64];
        int gx = static_cast<int>(std::floor(x / 10.f));
        int gy = static_cast<int>(std::floor(y / 10.f));
        int gz = static_cast<int>(std::floor(z / 10.f));
        snprintf(buf, sizeof(buf), "%d,%d,%d", gx, gy, gz);
        return buf;
    }

    inline void RecordCode(float x, float y, float z, const std::string& code, const std::string& owner = "") {
        if (code.empty()) return;
        auto key = PosKey(x, y, z);
        auto& entry = g_posCodes[key];
        entry.code = code;
        entry.owner = owner;
        entry.lastSeen = 0.f;
    }

    inline std::string LookupCode(float x, float y, float z) {
        auto key = PosKey(x, y, z);
        auto it = g_posCodes.find(key);
        if (it != g_posCodes.end()) return it->second.code;
        return "";
    }

    inline void ParseKeypadLog(const std::string& log) {
        if (log.empty()) return;
        std::istringstream ss(log);
        std::string line;
        while (std::getline(ss, line)) {
            // format: "code\tx\ty\tz\towner"
            float x = 0, y = 0, z = 0;
            char code[32] = {}, owner[64] = {};
            if (sscanf_s(line.c_str(), "%31[^\t]\t%f\t%f\t%f\t%63[^\n]", code, (unsigned)sizeof(code), &x, &y, &z, owner, (unsigned)sizeof(owner)) >= 4) {
                RecordCode(x, y, z, code, owner);
            }
        }
    }

    inline std::string GetSavePath() {
        char path[MAX_PATH];
        HMODULE hm = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&GetSavePath, &hm);
        GetModuleFileNameA(hm, path, MAX_PATH);
        std::string dir(path);
        size_t pos = dir.find_last_of("\\/");
        if (pos != std::string::npos) dir = dir.substr(0, pos + 1);
        return dir + "door_codes.dat";
    }

    inline void SaveCodes() {
        std::ofstream f(GetSavePath());
        if (!f.is_open()) return;
        for (const auto& [key, entry] : g_posCodes) {
            f << key << "=" << entry.code << "\n";
        }
        f.close();
    }

    inline void LoadCodes() {
        std::ifstream f(GetSavePath());
        if (!f.is_open()) return;
        std::string line;
        while (std::getline(f, line)) {
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            auto& entry = g_posCodes[line.substr(0, eq)];
            entry.code = line.substr(eq + 1);
        }
        f.close();
    }

    inline void DrawOnESP(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH) {
        if (!enabled || !show_on_esp) return;

        auto& cache = config::EntRead();
        int cnt = config::EntReadCount();

        for (int i = 0; i < cnt; ++i) {
            const auto& ent = cache[i];
            if (!ent.valid || ent.type != 3) continue; // type 3 = door

            std::string code = LookupCode(ent.pos.x, ent.pos.y, ent.pos.z);
            if (code.empty()) continue;

            float sx, sy;
            if (!config::WorldToScreen(ent.pos, sx, sy)) continue;

            char buf[48];
            snprintf(buf, sizeof(buf), "[CODE: %s]", code.c_str());

            ImVec2 ts = font->CalcTextSizeA(fontSize * 0.85f, FLT_MAX, 0.f, buf);
            float x = sx - ts.x * 0.5f;
            float y = sy + 20.f;

            dl->AddRectFilled(ImVec2(x - 3.f, y - 2.f), ImVec2(x + ts.x + 3.f, y + ts.y + 2.f),
                              IM_COL32(0, 0, 0, 180), 3.f);
            dl->AddText(font, fontSize * 0.85f, ImVec2(x, y), IM_COL32(255, 220, 0, 255), buf);
        }
    }

} // namespace door_memory
