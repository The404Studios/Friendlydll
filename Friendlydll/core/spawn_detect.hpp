#pragma once
#include "../includes.hpp"
#include <array>

namespace spawn_detect {

    inline bool enabled = false;

    struct PlayerState {
        int lastHealth = 0;
        bool wasHit = false;
        int surviveCount = 0;
        bool godmode = false;
        float detectTime = 0.f;
        int lastDamageTick = -999; // tick when health last decreased
        int tickCounter = 0;      // total ticks since tracking started
    };

    inline std::array<PlayerState, 128> g_states{};

    inline void Update(float curtime) {
        if (!enabled) return;

        auto& cache = config::BoneRead();
        for (int i = 1; i < 128; ++i) {
            if (!cache[i].valid) {
                g_states[i] = {};
                continue;
            }

            auto& st = g_states[i];
            int hp = cache[i].health;
            st.tickCounter++;

            // Detect damage taken (health decreased but player still alive)
            if (hp < st.lastHealth && hp > 0) {
                st.lastDamageTick = st.tickCounter;
                st.surviveCount = 0;
                st.godmode = false;
            }

            // Only count godmode ticks if the player was damaged recently (within 200 ticks)
            // AND their health hasn't dropped since (stayed at >= 100)
            bool recentlyDamaged = (st.tickCounter - st.lastDamageTick) < 200;
            if (recentlyDamaged && hp == st.lastHealth && hp >= 100) {
                st.surviveCount++;
                if (st.surviveCount > 200) {
                    st.godmode = true;
                    st.detectTime = curtime;
                }
            } else if (!recentlyDamaged) {
                // No recent damage, reset the counter -- not suspicious
                st.surviveCount = 0;
            }

            if (st.godmode && curtime - st.detectTime > 30.f) {
                st.godmode = false;
                st.surviveCount = 0;
            }

            st.lastHealth = hp;
        }
    }

    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH) {
        if (!enabled) return;

        auto& cache = config::BoneRead();
        for (int i = 1; i < 128; ++i) {
            if (!cache[i].valid || !g_states[i].godmode || cache[i].noBones) continue;

            Vector headPos = cache[i].bones[Bones::bone_head].GetOrigin();
            float sx, sy;
            if (!config::WorldToScreen(headPos, sx, sy)) continue;

            const char* text = "GODMODE";
            ImVec2 ts = font->CalcTextSizeA(fontSize * 0.9f, FLT_MAX, 0.f, text);
            float x = sx - ts.x * 0.5f;
            float y = sy - 30.f;

            dl->AddRectFilled(ImVec2(x - 4.f, y - 2.f), ImVec2(x + ts.x + 4.f, y + ts.y + 2.f),
                              IM_COL32(180, 0, 0, 180), 3.f);
            dl->AddText(font, fontSize * 0.9f, ImVec2(x + 1.f, y + 1.f), IM_COL32(0, 0, 0, 200), text);
            dl->AddText(font, fontSize * 0.9f, ImVec2(x, y), IM_COL32(255, 255, 0, 255), text);
        }
    }

} // namespace spawn_detect
