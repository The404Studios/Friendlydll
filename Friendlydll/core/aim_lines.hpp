#pragma once
#include "../includes.hpp"

namespace aim_lines {

    inline bool enabled = false;
    inline float line_length = 200.f;
    inline float aim_color[3] = { 1.f, 0.3f, 0.3f };

    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH) {
        if (!enabled) return;

        auto& cache = config::BoneRead();
        for (int i = 0; i < 128; ++i) {
            if (!cache[i].valid) continue;
            if (cache[i].noBones) continue;
            if (cache[i].distance < config::esp_min_dist || cache[i].distance > config::esp_max_dist) continue;
            if (!config::IsTargetAllowed(i)) continue;

            const Vector headPos = cache[i].bones[Bones::bone_head].GetOrigin();
            float hx, hy;
            if (!config::WorldToScreen(headPos, hx, hy)) continue;

            // Use the head bone's local X axis (forward direction) as the aim direction.
            // In Source engine, the bone matrix X axis represents the bone's forward vector.
            const auto& headMat = cache[i].bones[Bones::bone_head];
            Vector aimDir = { headMat[0][0], headMat[1][0], headMat[2][0] };
            float len = sqrtf(aimDir.x * aimDir.x + aimDir.y * aimDir.y + aimDir.z * aimDir.z);
            if (len < 0.01f) continue;
            aimDir.x /= len; aimDir.y /= len; aimDir.z /= len;

            Vector endPos = {
                headPos.x + aimDir.x * line_length,
                headPos.y + aimDir.y * line_length,
                headPos.z + aimDir.z * line_length
            };

            float ex, ey;
            if (!config::WorldToScreen(endPos, ex, ey)) continue;

            ImU32 col = ImColor(aim_color[0], aim_color[1], aim_color[2], 0.7f);
            ImU32 glow = IM_COL32(0, 0, 0, 100);
            dl->AddLine(ImVec2(hx, hy), ImVec2(ex, ey), glow, 2.5f);
            dl->AddLine(ImVec2(hx, hy), ImVec2(ex, ey), col, 1.2f);

            dl->AddCircleFilled(ImVec2(ex, ey), 3.f, col);
        }
    }

} // namespace aim_lines
