#pragma once
#include "../includes.hpp"

namespace fakelag {

    inline bool enabled = false;
    inline int mode = 0;          // 0=static, 1=adaptive, 2=on-peek, 3=random
    inline int choke_ticks = 4;   // ticks to choke before sending
    inline int max_choke = 14;
    inline bool visualize = false;

    inline int g_chokedCount = 0;
    inline bool g_shouldChoke = false;
    inline bool g_wasOnGround = false;

    inline void Reset() {
        g_chokedCount = 0;
        g_shouldChoke = false;
    }

    inline int CalculateChokeTicks(C_BasePlayer* local, CUserCmd* cmd) {
        if (!enabled || !local) return 0;

        switch (mode) {
        case 0: // static
            return choke_ticks;

        case 1: { // adaptive - choke more when moving fast
            float speed = local->GetVelocity().Length2D();
            if (speed > 200.f) return max_choke;
            if (speed > 100.f) return max_choke / 2;
            return 2;
        }

        case 2: { // on-peek - choke while strafing into a peek
            float speed = local->GetVelocity().Length2D();
            bool moving = speed > 50.f;
            bool attacking = (cmd->buttons & CUserCmd::IN_ATTACK) != 0;
            if (moving && !attacking) return max_choke;
            return 0;
        }

        case 3: // random
            return 2 + (rand() % (std::max)(1, max_choke - 1));

        default:
            return choke_ticks;
        }
    }

    inline bool ShouldChoke(C_BasePlayer* local, CUserCmd* cmd) {
        if (!enabled) { g_chokedCount = 0; return false; }

        int target = CalculateChokeTicks(local, cmd);
        if (target <= 0) { g_chokedCount = 0; return false; }

        if (g_chokedCount < target && g_chokedCount < max_choke) {
            ++g_chokedCount;
            g_shouldChoke = true;
            return true;
        }

        g_chokedCount = 0;
        g_shouldChoke = false;
        return false;
    }

    inline void DrawIndicator(ImDrawList* dl, ImFont* font, float fontSize,
                              int screenW, int screenH) {
        if (!enabled || !visualize) return;

        float barW = 120.f;
        float barH = 8.f;
        float x = static_cast<float>(screenW) * 0.5f - barW * 0.5f;
        float y = static_cast<float>(screenH) - 120.f;

        float fraction = static_cast<float>(g_chokedCount) / static_cast<float>(max_choke);
        fraction = std::clamp(fraction, 0.f, 1.f);

        dl->AddRectFilled(ImVec2(x - 1.f, y - 1.f), ImVec2(x + barW + 1.f, y + barH + 1.f),
                          IM_COL32(0, 0, 0, 160), 3.f);

        ImU32 fillCol = g_shouldChoke ? IM_COL32(255, 60, 60, 220) : IM_COL32(60, 255, 60, 220);
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + barW * fraction, y + barH), fillCol, 2.f);

        char buf[32];
        snprintf(buf, sizeof(buf), "FL: %d/%d", g_chokedCount, max_choke);
        ImVec2 ts = font->CalcTextSizeA(fontSize * 0.7f, FLT_MAX, 0.f, buf);
        dl->AddText(font, fontSize * 0.7f,
                    ImVec2(x + barW * 0.5f - ts.x * 0.5f, y - ts.y - 2.f),
                    IM_COL32(220, 220, 220, 200), buf);
    }

} // namespace fakelag
