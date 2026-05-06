#pragma once
#include "../includes.hpp"
#include "lua_scripts.hpp"
#include "killstreak.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

namespace killfeed {

    // -----------------------------------------------------------------------
    // Configuration toggles
    // -----------------------------------------------------------------------
    inline bool analyzer_enabled = false;
    inline bool voice_indicators = false;
    inline bool g_killfeedInstalled = false;
    inline bool g_voiceSpyInstalled = false;

    // -----------------------------------------------------------------------
    // Kill Feed data structures
    // -----------------------------------------------------------------------
    struct KillRecord {
        float time;
        char attacker[32];
        char victim[32];
        char weapon[32];
    };

    struct PlayerStats {
        char name[32];
        int kills;
        int deaths;
    };

    inline std::vector<KillRecord> g_kills;
    inline std::vector<PlayerStats> g_stats;

    // -----------------------------------------------------------------------
    // Voice proximity data structures
    // -----------------------------------------------------------------------
    struct VoiceState {
        int entIndex;
        char name[32];
    };

    inline std::vector<VoiceState> g_voiceActive;


    // -----------------------------------------------------------------------
    // Parsing -- Kill Feed
    // -----------------------------------------------------------------------
    inline void ParseKillFeed(const std::string& data) {
        g_kills.clear();
        g_stats.clear();

        std::istringstream ss(data);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;

            // Determine line type from prefix
            size_t t0 = line.find('\t');
            if (t0 == std::string::npos) continue;
            std::string prefix = line.substr(0, t0);

            if (prefix == "KILL") {
                // KILL\tTIME\tATTACKER\tVICTIM\tWEAPON
                size_t t1 = line.find('\t', t0 + 1);
                if (t1 == std::string::npos) continue;
                size_t t2 = line.find('\t', t1 + 1);
                if (t2 == std::string::npos) continue;
                size_t t3 = line.find('\t', t2 + 1);
                if (t3 == std::string::npos) continue;

                KillRecord k{};
                k.time = static_cast<float>(std::atof(line.substr(t0 + 1, t1 - t0 - 1).c_str()));
                strncpy_s(k.attacker, line.substr(t1 + 1, t2 - t1 - 1).c_str(), 31);
                strncpy_s(k.victim, line.substr(t2 + 1, t3 - t2 - 1).c_str(), 31);
                strncpy_s(k.weapon, line.substr(t3 + 1).c_str(), 31);
                g_kills.push_back(k);
            }
            else if (prefix == "STAT") {
                // STAT\tNAME\tKILLS\tDEATHS
                size_t t1 = line.find('\t', t0 + 1);
                if (t1 == std::string::npos) continue;
                size_t t2 = line.find('\t', t1 + 1);
                if (t2 == std::string::npos) continue;
                size_t t3 = line.find('\t', t2 + 1);
                if (t3 == std::string::npos) continue;

                PlayerStats s{};
                strncpy_s(s.name, line.substr(t1 + 1, t2 - t1 - 1).c_str(), 31);
                s.kills = std::atoi(line.substr(t2 + 1, t3 - t2 - 1).c_str());
                s.deaths = std::atoi(line.substr(t3 + 1).c_str());
                g_stats.push_back(s);
            }
        }

        // Sort stats by K/D ratio descending
        std::sort(g_stats.begin(), g_stats.end(), [](const PlayerStats& a, const PlayerStats& b) {
            float kdA = (a.deaths > 0) ? static_cast<float>(a.kills) / a.deaths : static_cast<float>(a.kills);
            float kdB = (b.deaths > 0) ? static_cast<float>(b.kills) / b.deaths : static_cast<float>(b.kills);
            return kdA > kdB;
        });
    }

    // -----------------------------------------------------------------------
    // Parsing -- Voice State
    // -----------------------------------------------------------------------
    inline void ParseVoiceState(const std::string& data) {
        g_voiceActive.clear();

        std::istringstream ss(data);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;

            // ENTINDEX\tNAME
            size_t t1 = line.find('\t');
            if (t1 == std::string::npos) continue;

            VoiceState v{};
            v.entIndex = std::atoi(line.substr(0, t1).c_str());
            strncpy_s(v.name, line.substr(t1 + 1).c_str(), 31);
            g_voiceActive.push_back(v);
        }
    }

    // -----------------------------------------------------------------------
    // Lookup -- is a specific player currently speaking?
    // -----------------------------------------------------------------------
    inline bool IsPlayerSpeaking(int entIndex) {
        for (const auto& v : g_voiceActive) {
            if (v.entIndex == entIndex) return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // Update functions -- call from frame loop
    // -----------------------------------------------------------------------
    inline void UpdateKillFeed() {
        if (!analyzer_enabled) return;
        if (!config::luastate) return;

        if (!g_killfeedInstalled) {
            if (luascripts::RunLuaScript(luascripts::LUA_KILLFEED_SETUP))
                g_killfeedInstalled = true;
        }

        auto result = luascripts::QueryLuaScript(luascripts::LUA_KILLFEED_READ);
        if (result.empty()) return;
        ParseKillFeed(result);

        if (config::killstreak_enabled && !g_kills.empty()) {
            static float s_lastProcessedTime = 0.f;
            float newestTime = g_kills.back().time;
            if (newestTime > s_lastProcessedTime) {
                player_info_s localInfo{};
                if (interfaces::engine->GetPlayerInfo(interfaces::engine->GetLocalPlayer(), &localInfo)) {
                    for (const auto& k : g_kills) {
                        if (k.time <= s_lastProcessedTime) continue;
                        if (_stricmp(k.attacker, localInfo.name) == 0)
                            killstreak::OnKill(k.victim, k.time);
                        else if (_stricmp(k.victim, localInfo.name) == 0)
                            killstreak::OnDeath();
                    }
                }
                s_lastProcessedTime = newestTime;
            }
        }
    }

    inline void UpdateVoiceState() {
        if (!voice_indicators) return;
        if (!config::luastate) return;

        if (!g_voiceSpyInstalled) {
            if (luascripts::RunLuaScript(luascripts::LUA_VOICE_SPY_SETUP))
                g_voiceSpyInstalled = true;
        }

        auto result = luascripts::QueryLuaScript(luascripts::LUA_VOICE_SPY_READ);
        if (result.empty()) {
            g_voiceActive.clear();
            return;
        }
        ParseVoiceState(result);
    }

    // -----------------------------------------------------------------------
    // Render -- Kill Feed Analyzer panel (top-right)
    // -----------------------------------------------------------------------
    inline void DrawAnalyzer(ImDrawList* dl, ImFont* font, float fontSize,
                             int screenW, int screenH)
    {
        if (!analyzer_enabled) return;
        if (g_kills.empty() && g_stats.empty()) return;

        const float panelW = 340.f;
        const float lineH = fontSize + 2.f;
        const float padding = 8.f;
        const float titleH = lineH + 6.f;

        // Calculate panel height
        int maxKills = 10;
        int killCount = static_cast<int>(g_kills.size());
        int showKills = (killCount > maxKills) ? maxKills : killCount;

        int maxStats = 5;
        int statCount = static_cast<int>(g_stats.size());
        int showStats = (statCount > maxStats) ? maxStats : statCount;

        float killSectionH = showKills * lineH;
        float statSectionH = showStats * lineH;
        float separatorH = (showStats > 0 && showKills > 0) ? lineH : 0.f;
        float statTitleH = (showStats > 0) ? lineH : 0.f;
        float panelH = titleH + killSectionH + separatorH + statTitleH + statSectionH + padding * 2.f;

        // Top-right positioning
        float panelX = screenW - panelW - 20.f;
        float panelY = 20.f;

        // Background
        dl->AddRectFilled(ImVec2(panelX, panelY),
                          ImVec2(panelX + panelW, panelY + panelH),
                          IM_COL32(10, 10, 14, 200), 6.f);
        // Accent border (cyan like net_panel)
        dl->AddRect(ImVec2(panelX, panelY),
                    ImVec2(panelX + panelW, panelY + panelH),
                    IM_COL32(0, 180, 216, 120), 6.f);

        float textY = panelY + 4.f;

        // Title
        dl->AddText(font, fontSize, ImVec2(panelX + padding, textY),
                    IM_COL32(0, 180, 216, 255), "KILL FEED");

        // Kill count badge
        if (killCount > 0) {
            char badge[16];
            snprintf(badge, sizeof(badge), "[%d]", killCount);
            dl->AddText(font, fontSize * 0.85f,
                        ImVec2(panelX + padding + 90.f, textY + 1.f),
                        IM_COL32(180, 180, 200, 160), badge);
        }

        textY += titleH;

        // Recent kills -- newest at top
        int startIdx = (killCount > maxKills) ? killCount - maxKills : 0;
        for (int i = killCount - 1; i >= startIdx; --i) {
            const auto& k = g_kills[i];

            float curX = panelX + padding;

            // Time stamp
            char timeBuf[16];
            snprintf(timeBuf, sizeof(timeBuf), "[%.0f]", k.time);
            dl->AddText(font, fontSize * 0.85f, ImVec2(curX, textY),
                        IM_COL32(120, 120, 140, 180), timeBuf);
            curX += 50.f;

            // Attacker name (green)
            dl->AddText(font, fontSize * 0.9f, ImVec2(curX, textY),
                        IM_COL32(80, 220, 80, 230), k.attacker);

            // Measure attacker text width for arrow placement
            ImVec2 attackerSize = font->CalcTextSizeA(fontSize * 0.9f, FLT_MAX, 0.f, k.attacker);
            curX += attackerSize.x + 4.f;

            // Arrow separator
            dl->AddText(font, fontSize * 0.85f, ImVec2(curX, textY),
                        IM_COL32(180, 180, 200, 160), ">");
            curX += 12.f;

            // Victim name (red)
            dl->AddText(font, fontSize * 0.9f, ImVec2(curX, textY),
                        IM_COL32(220, 70, 70, 230), k.victim);

            ImVec2 victimSize = font->CalcTextSizeA(fontSize * 0.9f, FLT_MAX, 0.f, k.victim);
            curX += victimSize.x + 6.f;

            // Weapon (gray, parenthesized)
            char weapBuf[40];
            snprintf(weapBuf, sizeof(weapBuf), "(%s)", k.weapon);
            dl->AddText(font, fontSize * 0.8f, ImVec2(curX, textY + 1.f),
                        IM_COL32(140, 140, 160, 180), weapBuf);

            textY += lineH;
        }

        // Stats section
        if (showStats > 0 && showKills > 0) {
            // Separator line
            float sepY = textY + lineH * 0.4f;
            dl->AddLine(ImVec2(panelX + padding, sepY),
                        ImVec2(panelX + panelW - padding, sepY),
                        IM_COL32(80, 80, 100, 100));
            textY += lineH;

            // Stats header (purple accent)
            dl->AddText(font, fontSize * 0.9f, ImVec2(panelX + padding, textY),
                        IM_COL32(180, 120, 255, 255), "TOP K/D");
            textY += lineH;

            for (int i = 0; i < showStats; ++i) {
                const auto& s = g_stats[i];

                float kd = (s.deaths > 0)
                    ? static_cast<float>(s.kills) / s.deaths
                    : static_cast<float>(s.kills);

                char statBuf[96];
                snprintf(statBuf, sizeof(statBuf), "%s  %dK/%dD  (%.1f)",
                         s.name, s.kills, s.deaths, kd);

                // Color grade: high K/D = gold, medium = white, low = dim
                ImU32 col;
                if (kd >= 3.0f)       col = IM_COL32(255, 200, 50, 230);   // gold
                else if (kd >= 1.5f)  col = IM_COL32(220, 220, 220, 220);  // white
                else                  col = IM_COL32(150, 150, 160, 180);   // dim

                dl->AddText(font, fontSize * 0.85f,
                            ImVec2(panelX + padding, textY), col, statBuf);
                textY += lineH;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Render -- Voice indicators at player screen positions
    // -----------------------------------------------------------------------
    inline void DrawVoiceIndicators(ImDrawList* dl, ImFont* font, float fontSize) {
        if (!voice_indicators) return;
        if (g_voiceActive.empty()) return;

        auto& bones = config::BoneRead();

        for (const auto& v : g_voiceActive) {
            if (v.entIndex <= 0 || v.entIndex >= 128) continue;
            const auto& rec = bones[v.entIndex];
            if (!rec.valid || rec.dormant) continue;

            Vector headPos;
            if (rec.noBones) {
                headPos = Vector(rec.absOrigin.x, rec.absOrigin.y, rec.absOrigin.z + 84.f);
            } else {
                headPos.x = rec.bones[Bones::bone_head][0][3];
                headPos.y = rec.bones[Bones::bone_head][1][3];
                headPos.z = rec.bones[Bones::bone_head][2][3] + 12.f;
            }

            float sx, sy;
            if (!config::WorldToScreen(headPos, sx, sy)) continue;

            // Filled circle indicator (orange/yellow)
            float radius = 6.f;
            dl->AddCircleFilled(ImVec2(sx, sy - 14.f), radius,
                                IM_COL32(255, 180, 30, 220), 12);

            // Outline for visibility
            dl->AddCircle(ImVec2(sx, sy - 14.f), radius,
                          IM_COL32(255, 220, 80, 255), 12, 1.5f);

            // "V" letter centered in the circle
            ImVec2 textSize = font->CalcTextSizeA(fontSize * 0.7f, FLT_MAX, 0.f, "V");
            dl->AddText(font, fontSize * 0.7f,
                        ImVec2(sx - textSize.x * 0.5f, sy - 14.f - textSize.y * 0.5f),
                        IM_COL32(10, 10, 14, 255), "V");

            // Player name below the indicator (small, yellow tint)
            ImVec2 nameSize = font->CalcTextSizeA(fontSize * 0.7f, FLT_MAX, 0.f, v.name);
            dl->AddText(font, fontSize * 0.7f,
                        ImVec2(sx - nameSize.x * 0.5f, sy - 2.f),
                        IM_COL32(255, 200, 60, 200), v.name);
        }
    }

} // namespace killfeed
