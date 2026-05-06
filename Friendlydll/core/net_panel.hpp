#pragma once
#include "../includes.hpp"
#include "lua_scripts.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <fstream>
#include <mutex>
#include <algorithm>

namespace net_panel {

    // -----------------------------------------------------------------------
    // Data structures
    // -----------------------------------------------------------------------
    struct NetEntry {
        float time;
        char name[64];
        int length;
        char payload[256];
    };

    struct ChatEntry {
        float time;
        char player[32];
        char message[128];
        bool teamChat;
        bool dead;
    };

    struct TrafficStats {
        int totalMessages = 0;
        int totalBytesIn = 0;
        int totalBytesOut = 0;
        float msgPerSecond = 0.f;
        float peakMsgPerSecond = 0.f;
        float lastStatTime = 0.f;
        int lastStatCount = 0;
        std::unordered_map<std::string, int> msgFrequency;
    };

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    inline std::mutex g_netMutex;
    inline std::vector<NetEntry> g_netLog;
    inline std::vector<ChatEntry> g_chatLog;
    inline bool g_snifferInstalled = false;
    inline bool g_chatSpyInstalled = false;
    inline bool g_deepHookInstalled = false;
    inline bool chat_spy_enabled = false;

    // Filtering
    inline char g_filterBuf[64] = "";
    inline bool g_filterWhitelist = false;

    // Statistics
    inline TrafficStats g_stats;
    inline float g_lastSeenTime = 0.f;
    inline int g_lastSeenCount = 0;

    // Payload inspection
    inline int g_selectedMsg = -1;
    inline bool g_payloadSnifferInstalled = false;

    // File logging
    inline bool g_fileLogging = false;
    inline std::ofstream g_logFile;

    // -----------------------------------------------------------------------
    // File logging helpers
    // -----------------------------------------------------------------------
    inline void LogToFile(const NetEntry& e) {
        if (!g_fileLogging) return;
        if (!g_logFile.is_open()) {
            g_logFile.open("friendlydll_netlog.txt", std::ios::app);
            if (!g_logFile.is_open()) return;
        }
        g_logFile << "[" << e.time << "] " << e.name << " (" << e.length << " B)";
        if (e.payload[0]) g_logFile << " | " << e.payload;
        g_logFile << "\n";
        g_logFile.flush();
    }

    inline void ExportLog() {
        std::ofstream out("friendlydll_net_export.txt", std::ios::trunc);
        if (!out.is_open()) return;
        out << "=== FriendlyDLL Net Log Export ===\n\n";

        out << "--- Traffic Statistics ---\n";
        out << "Total messages: " << g_stats.totalMessages << "\n";
        out << "Total bytes IN: " << g_stats.totalBytesIn << "\n";
        out << "Total bytes OUT: " << g_stats.totalBytesOut << "\n";
        out << "Msg/sec: " << g_stats.msgPerSecond << "\n";
        out << "Peak msg/sec: " << g_stats.peakMsgPerSecond << "\n\n";

        out << "--- Message Frequency ---\n";
        std::vector<std::pair<std::string, int>> sorted(g_stats.msgFrequency.begin(), g_stats.msgFrequency.end());
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        for (const auto& kv : sorted)
            out << kv.first << ": " << kv.second << "\n";
        out << "\n--- Raw Log ---\n";

        std::lock_guard<std::mutex> lk(g_netMutex);
        for (const auto& e : g_netLog) {
            out << "[" << e.time << "] " << e.name << " (" << e.length << " B)";
            if (e.payload[0]) out << " | " << e.payload;
            out << "\n";
        }
        out.close();
        spdlog::info("[net_panel] Exported log to friendlydll_net_export.txt");
    }

    // -----------------------------------------------------------------------
    // Forged packet sender
    // -----------------------------------------------------------------------
    inline std::string EscapeLuaString(const char* input) {
        std::string out;
        out.reserve(strlen(input) + 16);
        for (const char* p = input; *p; ++p) {
            if (*p == '\\') out += "\\\\";
            else if (*p == '"') out += "\\\"";
            else if (*p == '\n') out += "\\n";
            else if (*p == '\r') out += "\\r";
            else if (*p == '\0') break;
            else out += *p;
        }
        return out;
    }

    inline void SendForgedPacket(const char* msgName, int dataType, const char* payload) {
        if (!config::luastate || !msgName[0]) return;
        std::string safePayload = EscapeLuaString(payload);
        std::string safeMsgName = EscapeLuaString(msgName);
        std::string writeCall;
        switch (dataType) {
            case 0: writeCall = std::string("net.WriteString(\"") + safePayload + "\")"; break;
            case 1: writeCall = std::string("net.WriteInt(") + payload + ", 32)"; break;
            case 2: writeCall = std::string("net.WriteFloat(") + payload + ")"; break;
            case 3: writeCall = std::string("net.WriteBool(") + payload + ")"; break;
            case 4: writeCall = std::string("net.WriteEntity(Entity(") + payload + "))"; break;
            default: writeCall = std::string("net.WriteString(\"") + safePayload + "\")"; break;
        }
        std::string script = std::string("pcall(function() net.Start(\"") + safeMsgName + "\") " + writeCall + " net.SendToServer() end)";
        lualoader::Execute(script);
        spdlog::info("[net_panel] Sent forged packet: {} -> {}", msgName, payload);
    }

    // -----------------------------------------------------------------------
    // Statistics updater
    // -----------------------------------------------------------------------
    inline void UpdateStats(float curTime) {
        float dt = curTime - g_stats.lastStatTime;
        if (dt >= 1.f) {
            int newMsgs = g_stats.totalMessages - g_stats.lastStatCount;
            g_stats.msgPerSecond = static_cast<float>(newMsgs) / dt;
            if (g_stats.msgPerSecond > g_stats.peakMsgPerSecond)
                g_stats.peakMsgPerSecond = g_stats.msgPerSecond;
            g_stats.lastStatTime = curTime;
            g_stats.lastStatCount = g_stats.totalMessages;
        }
    }

    // -----------------------------------------------------------------------
    // Net log update — with payload capture support
    // -----------------------------------------------------------------------
    inline void UpdateNetLog() {
        if (!config::net_sniffer) return;
        if (!config::luastate) return;

        // Install payload sniffer if deep hook is active
        if (g_deepHookInstalled && !g_payloadSnifferInstalled) {
            if (luascripts::RunLuaScript(luascripts::LUA_NET_PAYLOAD_SETUP))
                g_payloadSnifferInstalled = true;
        }

        // Query Lua OUTSIDE the mutex to avoid blocking the render thread
        std::string result;
        if (g_deepHookInstalled) {
            if (g_payloadSnifferInstalled)
                result = luascripts::QueryLuaScript(luascripts::LUA_NET_PAYLOAD_READ);
            else
                result = luascripts::QueryLuaScript(luascripts::LUA_NET_DEEP_READ);
        } else {
            if (!g_snifferInstalled) {
                if (luascripts::RunLuaScript(luascripts::LUA_NET_SNIFFER_SETUP))
                    g_snifferInstalled = true;
            }
            result = luascripts::QueryLuaScript(luascripts::LUA_NET_SNIFFER_READ);
        }

        if (result.empty()) return;

        std::lock_guard<std::mutex> lk(g_netMutex);

        if (g_deepHookInstalled) {
            std::vector<NetEntry> newLog;
            std::istringstream ss(result);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.empty()) continue;

                size_t t1 = line.find('\t');
                if (t1 == std::string::npos) continue;
                size_t t2 = line.find('\t', t1 + 1);
                if (t2 == std::string::npos) continue;
                size_t t3 = line.find('\t', t2 + 1);
                if (t3 == std::string::npos) continue;

                NetEntry e{};
                std::string dir = line.substr(0, t1);
                e.time = static_cast<float>(std::atof(line.substr(t1 + 1, t2 - t1 - 1).c_str()));
                std::string fullName = (dir == "OUT" ? ">> " : "<< ") + line.substr(t2 + 1, t3 - t2 - 1);
                strncpy_s(e.name, fullName.c_str(), 63);

                std::string rest = line.substr(t3 + 1);
                size_t t4 = rest.find('\t');
                if (t4 != std::string::npos) {
                    e.length = std::atoi(rest.substr(0, t4).c_str());
                    strncpy_s(e.payload, rest.substr(t4 + 1).c_str(), 255);
                } else {
                    e.length = std::atoi(rest.c_str());
                    e.payload[0] = '\0';
                }

                newLog.push_back(e);
            }

            // Only count entries newer than what we last saw
            int newCount = (int)newLog.size();
            int startNew = newCount - g_lastSeenCount;
            if (startNew < 0 || g_lastSeenCount == 0) startNew = 0;
            if (newCount > 0 && newLog.back().time < g_lastSeenTime) startNew = 0;

            for (int ni = startNew; ni < newCount; ++ni) {
                const auto& e = newLog[ni];
                std::string cleanName(e.name);
                if (cleanName.size() > 3 && (cleanName[0] == '>' || cleanName[0] == '<'))
                    cleanName = cleanName.substr(3);
                g_stats.msgFrequency[cleanName]++;
                g_stats.totalMessages++;
                bool isOut = (e.name[0] == '>' && e.name[1] == '>');
                if (isOut) g_stats.totalBytesOut += e.length;
                else g_stats.totalBytesIn += e.length;
                LogToFile(e);
            }

            if (!newLog.empty()) {
                g_lastSeenTime = newLog.back().time;
                g_lastSeenCount = newCount;
            }

            g_netLog = std::move(newLog);
            UpdateStats(g_netLog.empty() ? 0.f : g_netLog.back().time);
        } else {
            std::vector<NetEntry> newLog;
            std::istringstream ss2(result);
            std::string line2;
            while (std::getline(ss2, line2)) {
                if (line2.empty()) continue;
                size_t t1 = line2.find('\t');
                if (t1 == std::string::npos) continue;
                size_t t2 = line2.find('\t', t1 + 1);
                if (t2 == std::string::npos) continue;

                NetEntry e{};
                e.time = static_cast<float>(std::atof(line2.substr(0, t1).c_str()));
                strncpy_s(e.name, line2.substr(t1 + 1, t2 - t1 - 1).c_str(), 63);
                e.length = std::atoi(line2.substr(t2 + 1).c_str());
                e.payload[0] = '\0';
                newLog.push_back(e);
            }

            int newCount = (int)newLog.size();
            int startNew = newCount - g_lastSeenCount;
            if (startNew < 0 || g_lastSeenCount == 0) startNew = 0;
            if (newCount > 0 && newLog.back().time < g_lastSeenTime) startNew = 0;

            for (int ni = startNew; ni < newCount; ++ni) {
                const auto& e = newLog[ni];
                g_stats.msgFrequency[std::string(e.name)]++;
                g_stats.totalMessages++;
                g_stats.totalBytesIn += e.length;
                LogToFile(e);
            }

            if (!newLog.empty()) {
                g_lastSeenTime = newLog.back().time;
                g_lastSeenCount = newCount;
            }

            g_netLog = std::move(newLog);
        }
    }

    // -----------------------------------------------------------------------
    // Chat log update
    // -----------------------------------------------------------------------
    inline void UpdateChatLog() {
        if (!chat_spy_enabled) return;
        if (!config::luastate) return;

        if (!g_chatSpyInstalled) {
            if (luascripts::RunLuaScript(luascripts::LUA_CHAT_SPY_SETUP))
                g_chatSpyInstalled = true;
        }

        auto result = luascripts::QueryLuaScript(luascripts::LUA_CHAT_SPY_READ);
        if (result.empty()) return;

        g_chatLog.clear();
        std::istringstream ss(result);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            size_t t1 = line.find('\t');
            if (t1 == std::string::npos) continue;
            size_t t2 = line.find('\t', t1 + 1);
            if (t2 == std::string::npos) continue;
            size_t t3 = line.find('\t', t2 + 1);
            if (t3 == std::string::npos) continue;
            size_t t4 = line.find('\t', t3 + 1);
            if (t4 == std::string::npos) continue;

            ChatEntry e{};
            e.time = static_cast<float>(std::atof(line.substr(0, t1).c_str()));
            strncpy_s(e.player, line.substr(t1 + 1, t2 - t1 - 1).c_str(), 31);
            strncpy_s(e.message, line.substr(t2 + 1, t3 - t2 - 1).c_str(), 127);
            e.teamChat = line.substr(t3 + 1, t4 - t3 - 1) != "0";
            e.dead = line.substr(t4 + 1) != "0";
            g_chatLog.push_back(e);
        }
    }

    // -----------------------------------------------------------------------
    // HUD Net Panel draw
    // -----------------------------------------------------------------------
    inline void DrawNetPanel(ImDrawList* dl, ImFont* font, float fontSize,
                             int screenW, int screenH)
    {
        if (!config::net_sniffer) return;
        std::lock_guard<std::mutex> lk(g_netMutex);
        if (g_netLog.empty()) return;

        const float panelW = 360.f;
        const float lineH = fontSize + 2.f;
        int maxLines = 15;

        // Apply filter
        std::vector<const NetEntry*> filtered;
        for (const auto& e : g_netLog) {
            if (g_filterBuf[0] != '\0') {
                bool matches = strstr(e.name, g_filterBuf) != nullptr;
                if (g_filterWhitelist && !matches) continue;
                if (!g_filterWhitelist && matches) continue;
            }
            filtered.push_back(&e);
        }

        int startIdx = filtered.size() > (size_t)maxLines ? (int)filtered.size() - maxLines : 0;
        int count = (int)filtered.size() - startIdx;
        float panelH = lineH * (count + 2) + 34.f;

        float panelX = 20.f;
        float panelY = static_cast<float>(screenH) - panelH - 260.f;

        dl->AddRectFilled(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + panelH),
                          IM_COL32(10, 10, 14, 210), 6.f);
        dl->AddRect(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + panelH),
                    IM_COL32(0, 180, 216, 120), 6.f);

        float textY = panelY + 4.f;
        const char* title = g_deepHookInstalled ?
            (g_payloadSnifferInstalled ? "NET SNIFFER (DEEP+PAYLOAD)" : "NET SNIFFER (DEEP)") : "NET SNIFFER";
        dl->AddText(font, fontSize, ImVec2(panelX + 8.f, textY),
                    IM_COL32(0, 180, 216, 255), title);
        textY += lineH;

        // Stats line
        char statBuf[96];
        snprintf(statBuf, sizeof(statBuf), "%.1f msg/s | %d total | %d KB",
            g_stats.msgPerSecond, g_stats.totalMessages,
            (g_stats.totalBytesIn + g_stats.totalBytesOut) / 1024);
        dl->AddText(font, fontSize * 0.8f, ImVec2(panelX + 8.f, textY),
                    IM_COL32(120, 180, 200, 180), statBuf);
        textY += lineH + 2.f;

        for (int i = startIdx; i < (int)filtered.size(); ++i) {
            const auto& e = *filtered[i];
            char buf[192];
            if (e.payload[0])
                snprintf(buf, sizeof(buf), "[%.1f] %s (%d) %s", e.time, e.name, e.length, e.payload);
            else
                snprintf(buf, sizeof(buf), "[%.1f] %s (%d)", e.time, e.name, e.length);

            ImU32 col = IM_COL32(180, 200, 220, 220);
            if (e.name[0] == '>' && e.name[1] == '>') col = IM_COL32(255, 160, 80, 220);
            else if (e.name[0] == '<' && e.name[1] == '<') col = IM_COL32(80, 200, 255, 220);
            dl->AddText(font, fontSize * 0.85f, ImVec2(panelX + 8.f, textY), col, buf);
            textY += lineH;
        }
    }

    // -----------------------------------------------------------------------
    // HUD Chat Panel draw
    // -----------------------------------------------------------------------
    inline void DrawChatPanel(ImDrawList* dl, ImFont* font, float fontSize,
                              int screenW, int screenH)
    {
        if (!chat_spy_enabled) return;
        if (g_chatLog.empty()) return;

        const float panelW = 350.f;
        const float lineH = fontSize + 2.f;
        int maxLines = 10;
        int startIdx = g_chatLog.size() > (size_t)maxLines ? (int)g_chatLog.size() - maxLines : 0;
        int count = (int)g_chatLog.size() - startIdx;
        float panelH = lineH * count + 30.f;

        float panelX = static_cast<float>(screenW) - panelW - 20.f;
        float panelY = static_cast<float>(screenH) - panelH - 20.f;

        dl->AddRectFilled(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + panelH),
                          IM_COL32(10, 10, 14, 200), 6.f);
        dl->AddRect(ImVec2(panelX, panelY), ImVec2(panelX + panelW, panelY + panelH),
                    IM_COL32(180, 120, 255, 120), 6.f);

        float textY = panelY + 4.f;
        dl->AddText(font, fontSize, ImVec2(panelX + 8.f, textY),
                    IM_COL32(180, 120, 255, 255), "CHAT SPY");
        textY += lineH + 4.f;

        for (int i = startIdx; i < (int)g_chatLog.size(); ++i) {
            const auto& e = g_chatLog[i];
            char buf[200];
            const char* prefix = e.dead ? "*DEAD* " : (e.teamChat ? "(TEAM) " : "");
            snprintf(buf, sizeof(buf), "%s%s: %s", prefix, e.player, e.message);

            ImU32 col = e.teamChat ? IM_COL32(100, 255, 100, 220) : IM_COL32(220, 220, 220, 220);
            if (e.dead) col = IM_COL32(200, 100, 100, 200);

            dl->AddText(font, fontSize * 0.9f, ImVec2(panelX + 8.f, textY), col, buf);
            textY += lineH;
        }
    }

} // namespace net_panel
