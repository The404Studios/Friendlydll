#pragma once
#include "../includes.hpp"
#include <array>
#include <cmath>

namespace threat_radar {

    inline bool enabled = false;
    inline bool show_level = true;
    inline bool color_esp = true;

    struct ThreatInfo {
        float level = 0.f;
        bool aimingAtUs = false;
        float aimDot = 0.f;
        float lastUpdate = 0.f;
    };

    inline std::array<ThreatInfo, 128> g_threats{};
    inline Vector g_localEyePos{};

    inline float WeaponThreat(const char* weapon) {
        if (!weapon || !weapon[0]) return 0.1f;
        std::string w(weapon);
        if (w.find("awp") != std::string::npos || w.find("sniper") != std::string::npos) return 1.0f;
        if (w.find("rpg") != std::string::npos || w.find("launcher") != std::string::npos) return 0.95f;
        if (w.find("shotgun") != std::string::npos) return 0.8f;
        if (w.find("rifle") != std::string::npos || w.find("ak") != std::string::npos || w.find("m4") != std::string::npos) return 0.7f;
        if (w.find("smg") != std::string::npos || w.find("mp") != std::string::npos) return 0.5f;
        if (w.find("pistol") != std::string::npos || w.find("glock") != std::string::npos || w.find("deagle") != std::string::npos) return 0.4f;
        if (w.find("knife") != std::string::npos || w.find("melee") != std::string::npos) return 0.15f;
        if (w.find("phys") != std::string::npos || w.find("tool") != std::string::npos) return 0.05f;
        return 0.3f;
    }

    inline void Update(const Vector& localEyePos) {
        if (!enabled) return;
        g_localEyePos = localEyePos;

        auto& cache = config::BoneRead();
        for (int i = 1; i < 128; ++i) {
            if (!cache[i].valid || cache[i].noBones) { g_threats[i] = {}; continue; }

            float weaponScore = WeaponThreat(cache[i].weapon);

            float distFactor = 1.f - std::clamp(cache[i].distance / 3000.f, 0.f, 1.f);

            // Derive body facing direction from spine alignment (pelvis→upper spine, XY only)
            Vector spineBot = cache[i].bones[Bones::bone_pelvis].GetOrigin();
            Vector spineTop = cache[i].bones[Bones::bone_spine_1].GetOrigin();
            Vector bodyDir;
            bodyDir.x = spineTop.x - spineBot.x;
            bodyDir.y = spineTop.y - spineBot.y;
            bodyDir.z = 0.f;
            float bodyLen = sqrtf(bodyDir.x * bodyDir.x + bodyDir.y * bodyDir.y);

            // Direction from their head to our position (XY only for facing check)
            Vector headPos = cache[i].bones[Bones::bone_head].GetOrigin();
            Vector toUs;
            toUs.x = localEyePos.x - headPos.x;
            toUs.y = localEyePos.y - headPos.y;
            toUs.z = 0.f;
            float toUsLen = sqrtf(toUs.x * toUs.x + toUs.y * toUs.y);

            float aimScore = 0.f;
            g_threats[i].aimingAtUs = false;
            g_threats[i].aimDot = 0.f;

            if (bodyLen > 1.f && toUsLen > 1.f) {
                bodyDir.x /= bodyLen; bodyDir.y /= bodyLen;
                toUs.x /= toUsLen; toUs.y /= toUsLen;

                float dot = bodyDir.x * toUs.x + bodyDir.y * toUs.y;
                g_threats[i].aimDot = dot;

                // dot > 0.85 = within ~30 degrees, 0.7 = ~45 degrees
                if (dot > 0.85f) {
                    aimScore = 1.0f;
                    g_threats[i].aimingAtUs = true;
                } else if (dot > 0.5f) {
                    aimScore = (dot - 0.5f) / 0.35f;
                } else {
                    aimScore = 0.f;
                }
            }

            float adminMod = cache[i].isAdmin ? 0.1f : 0.f;
            float wantedMod = cache[i].isWanted ? 0.15f : 0.f;

            float score = weaponScore * 0.3f + distFactor * 0.25f + aimScore * 0.35f + wantedMod - adminMod;
            g_threats[i].level = std::clamp(score, 0.f, 1.f);
        }
    }

    inline ImU32 ThreatColor(float level, int alpha = 255) {
        if (level > 0.7f) return IM_COL32(255, 40, 40, alpha);      // high = red
        if (level > 0.4f) return IM_COL32(255, 180, 40, alpha);     // medium = orange
        if (level > 0.2f) return IM_COL32(255, 255, 60, alpha);     // low = yellow
        return IM_COL32(80, 255, 80, alpha);                         // minimal = green
    }

    inline const char* ThreatLabel(float level) {
        if (level > 0.7f) return "HIGH";
        if (level > 0.4f) return "MED";
        if (level > 0.2f) return "LOW";
        return "SAFE";
    }

    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH) {
        if (!enabled || !show_level) return;

        auto& cache = config::BoneRead();
        for (int i = 1; i < 128; ++i) {
            if (!cache[i].valid || cache[i].noBones || g_threats[i].level < 0.05f) continue;
            if (cache[i].distance < config::esp_min_dist || cache[i].distance > config::esp_max_dist) continue;

            Vector headPos = cache[i].bones[Bones::bone_head].GetOrigin();
            float sx, sy;
            if (!config::WorldToScreen(headPos, sx, sy)) continue;

            const char* label = ThreatLabel(g_threats[i].level);
            ImU32 col = ThreatColor(g_threats[i].level);

            float sz = fontSize * 0.7f;
            ImVec2 ts = font->CalcTextSizeA(sz, FLT_MAX, 0.f, label);
            float tx = sx + 15.f;
            float ty = sy - ts.y * 0.5f;

            dl->AddRectFilled(ImVec2(tx - 2.f, ty - 1.f), ImVec2(tx + ts.x + 2.f, ty + ts.y + 1.f),
                              IM_COL32(0, 0, 0, 140), 2.f);
            dl->AddText(font, sz, ImVec2(tx, ty), col, label);

            // "AIMING" warning when facing within ~30 degrees
            if (g_threats[i].aimingAtUs) {
                float pulse = sinf(static_cast<float>(ImGui::GetTime()) * 5.f) * 0.3f + 0.7f;
                int a = static_cast<int>(pulse * 255.f);
                const char* aim = "AIMING";
                ImVec2 as = font->CalcTextSizeA(sz, FLT_MAX, 0.f, aim);
                float ax = sx + 15.f;
                float ay = ty + ts.y + 2.f;
                dl->AddRectFilled(ImVec2(ax - 2.f, ay - 1.f), ImVec2(ax + as.x + 2.f, ay + as.y + 1.f),
                                  IM_COL32(140, 0, 0, 160), 2.f);
                dl->AddText(font, sz, ImVec2(ax, ay), IM_COL32(255, 60, 60, a), aim);
            }

            float ringR = 8.f + g_threats[i].level * 8.f;
            dl->AddCircle(ImVec2(sx, sy), ringR, ThreatColor(g_threats[i].level, 120), 16, 1.5f);
        }
    }

} // namespace threat_radar
