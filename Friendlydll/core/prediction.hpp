#pragma once
#include "../includes.hpp"

namespace prediction {

    inline bool enabled = false;
    inline float predict_time = 0.5f;      // how far ahead to predict (seconds)
    inline float ghost_alpha = 80.f;        // base alpha for ghost skeleton [0-255]

    struct PlayerState {
        Vector lastPos{};
        Vector velocity{};
        Vector acceleration{};
        Vector prevVelocity{};
        float lastUpdate = 0.f;
        bool initialized = false;
        int sampleCount = 0;
    };
    inline PlayerState g_states[128]{};

    // Bone connections matching the existing skeleton ESP in d3d9_hook.cpp
    inline constexpr std::pair<int, int> kBoneConnections[] = {
        { Bones::bone_head,      Bones::bone_neck      },
        { Bones::bone_neck,      Bones::bone_spine_1   },
        { Bones::bone_spine_1,   Bones::bone_spine_3   },
        { Bones::bone_spine_3,   Bones::bone_spine_2   },
        { Bones::bone_spine_2,   Bones::bone_pelvis    },
        { Bones::bone_spine_1,   Bones::bone_arm_top_l },
        { Bones::bone_arm_top_l, Bones::bone_arm_bot_l },
        { Bones::bone_arm_bot_l, Bones::bone_hand_l    },
        { Bones::bone_spine_1,   Bones::bone_arm_top_r },
        { Bones::bone_arm_top_r, Bones::bone_arm_bot_r },
        { Bones::bone_arm_bot_r, Bones::bone_hand_r    },
        { Bones::bone_pelvis,    Bones::bone_leg_top_l },
        { Bones::bone_leg_top_l, Bones::bone_leg_bot_l },
        { Bones::bone_leg_bot_l, Bones::bone_ANKLE_l   },
        { Bones::bone_pelvis,    Bones::bone_leg_top_r },
        { Bones::bone_leg_top_r, Bones::bone_leg_bot_r },
        { Bones::bone_leg_bot_r, Bones::bone_ANKLE_r   },
    };

    inline void Update(float curtime) {
        if (!enabled) return;
        auto& bones = config::BoneRead();
        for (int i = 1; i < 128; i++) {
            auto& b = bones[i];
            auto& s = g_states[i];
            if (!b.valid || b.dormant || b.noBones) { s = {}; continue; }
            if (s.initialized && curtime > s.lastUpdate) {
                float dt = curtime - s.lastUpdate;
                if (dt > 0.001f && dt < 1.0f) {
                    Vector newVel;
                    newVel.x = (b.absOrigin.x - s.lastPos.x) / dt;
                    newVel.y = (b.absOrigin.y - s.lastPos.y) / dt;
                    newVel.z = (b.absOrigin.z - s.lastPos.z) / dt;

                    s.velocity.x = s.velocity.x * 0.7f + newVel.x * 0.3f;
                    s.velocity.y = s.velocity.y * 0.7f + newVel.y * 0.3f;
                    s.velocity.z = s.velocity.z * 0.7f + newVel.z * 0.3f;

                    if (s.sampleCount > 2) {
                        Vector newAccel;
                        newAccel.x = (s.velocity.x - s.prevVelocity.x) / dt;
                        newAccel.y = (s.velocity.y - s.prevVelocity.y) / dt;
                        newAccel.z = (s.velocity.z - s.prevVelocity.z) / dt;
                        s.acceleration.x = s.acceleration.x * 0.8f + newAccel.x * 0.2f;
                        s.acceleration.y = s.acceleration.y * 0.8f + newAccel.y * 0.2f;
                        s.acceleration.z = s.acceleration.z * 0.8f + newAccel.z * 0.2f;
                    }
                    s.prevVelocity = s.velocity;
                    s.sampleCount++;
                }
            }
            s.lastPos = b.absOrigin;
            s.lastUpdate = curtime;
            s.initialized = true;
        }
    }

    // Render thread: draw ghost skeletons at predicted positions
    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH) {
        if (!enabled) return;

        auto& bones = config::BoneRead();
        const ImU32 ghostCol  = IM_COL32(0, 200, 255, static_cast<int>(ghost_alpha));
        const ImU32 ghostGlow = IM_COL32(0, 0, 0, static_cast<int>(ghost_alpha * 0.5f));
        const ImU32 headCol   = IM_COL32(0, 200, 255, static_cast<int>(ghost_alpha * 1.2f > 255.f ? 255.f : ghost_alpha * 1.2f));
        const ImU32 dashCol   = IM_COL32(0, 200, 255, static_cast<int>(ghost_alpha * 0.5f));

        for (int i = 1; i < 128; i++) {
            const auto& rec = bones[i];
            const auto& st  = g_states[i];
            if (!rec.valid || rec.dormant || rec.noBones) continue;
            if (!st.initialized) continue;
            if (!config::IsTargetAllowed(i)) continue;

            // Skip nearly-stationary players (velocity below threshold)
            float speedSq = st.velocity.x * st.velocity.x
                          + st.velocity.y * st.velocity.y
                          + st.velocity.z * st.velocity.z;
            if (speedSq < 25.f) continue; // ~5 units/sec minimum

            float t = predict_time;
            float halfTSq = 0.5f * t * t;
            Vector offset;
            offset.x = st.velocity.x * t + st.acceleration.x * halfTSq;
            offset.y = st.velocity.y * t + st.acceleration.y * halfTSq;
            offset.z = st.velocity.z * t + st.acceleration.z * halfTSq;

            // ── Draw ghost skeleton lines ────────────────────────────────────
            for (const auto& [a, b] : kBoneConnections) {
                // Predicted bone world positions = current bone pos + offset
                Vector boneA;
                boneA.x = rec.bones[a][0][3] + offset.x;
                boneA.y = rec.bones[a][1][3] + offset.y;
                boneA.z = rec.bones[a][2][3] + offset.z;

                Vector boneB;
                boneB.x = rec.bones[b][0][3] + offset.x;
                boneB.y = rec.bones[b][1][3] + offset.y;
                boneB.z = rec.bones[b][2][3] + offset.z;

                float scrAx, scrAy, scrBx, scrBy;
                if (!config::WorldToScreen(boneA, scrAx, scrAy)) continue;
                if (!config::WorldToScreen(boneB, scrBx, scrBy)) continue;

                // Cull off-screen segments
                if (scrAx < -100.f || scrAx > screenW + 100.f) continue;
                if (scrAy < -100.f || scrAy > screenH + 100.f) continue;

                // Dark glow behind, then colored line on top
                dl->AddLine(ImVec2(scrAx, scrAy), ImVec2(scrBx, scrBy), ghostGlow, 3.f);
                dl->AddLine(ImVec2(scrAx, scrAy), ImVec2(scrBx, scrBy), ghostCol, 1.5f);
            }

            // ── Draw filled circle at predicted head ─────────────────────────
            Vector predHead;
            predHead.x = rec.bones[Bones::bone_head][0][3] + offset.x;
            predHead.y = rec.bones[Bones::bone_head][1][3] + offset.y;
            predHead.z = rec.bones[Bones::bone_head][2][3] + offset.z;

            float headSx, headSy;
            if (config::WorldToScreen(predHead, headSx, headSy)) {
                dl->AddCircleFilled(ImVec2(headSx, headSy), 4.f, headCol, 12);
            }

            // ── Draw dashed line from current origin to predicted origin ─────
            // Current pelvis screen pos (use absOrigin as the "current" anchor)
            float curSx, curSy;
            if (!config::WorldToScreen(rec.absOrigin, curSx, curSy)) continue;

            // Predicted origin
            Vector predOrigin;
            predOrigin.x = rec.absOrigin.x + offset.x;
            predOrigin.y = rec.absOrigin.y + offset.y;
            predOrigin.z = rec.absOrigin.z + offset.z;

            float predSx, predSy;
            if (!config::WorldToScreen(predOrigin, predSx, predSy)) continue;

            // Simulate dashed line: draw short segments with gaps
            constexpr float dashLen = 6.f;
            constexpr float gapLen  = 4.f;

            float dx = predSx - curSx;
            float dy = predSy - curSy;
            float totalLen = std::sqrtf(dx * dx + dy * dy);
            if (totalLen < 1.f) continue;

            float dirX = dx / totalLen;
            float dirY = dy / totalLen;

            float drawn = 0.f;
            bool drawing = true;
            while (drawn < totalLen) {
                float segLen = drawing ? dashLen : gapLen;
                if (drawn + segLen > totalLen) segLen = totalLen - drawn;

                if (drawing) {
                    float x0 = curSx + dirX * drawn;
                    float y0 = curSy + dirY * drawn;
                    float x1 = curSx + dirX * (drawn + segLen);
                    float y1 = curSy + dirY * (drawn + segLen);
                    dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), dashCol, 1.f);
                }

                drawn += segLen;
                drawing = !drawing;
            }
        }
    }

} // namespace prediction
