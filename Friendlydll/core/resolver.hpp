#pragma once
#include "../includes.hpp"

namespace resolver {

    struct PlayerState {
        float prevModelYaw     = 0.f;
        float prevPrevModelYaw = 0.f;
        float resolvedOffset   = 0.f;
        int   bruteIndex       = 0;
        int   ticksOnOffset    = 0;
        bool  jitter           = false;
        bool  antiAim          = false;
        // adaptive hit tracking
        int   hitCount[8]      = {};
        int   missCount[8]     = {};
        int   lastShotOffset   = -1;
        float confidence       = 0.f;
    };

    // Ordered by how commonly they fix anti-aim
    inline constexpr float bruteOffsets[] = { 0.f, 180.f, 90.f, -90.f, 45.f, -45.f, 135.f, -135.f };

    inline std::array<PlayerState, 128> g_states{};

    // Rotate `point` around `pivot` by `yawDeg` degrees on the XY plane
    inline Vector RotateAround(const Vector& point, const Vector& pivot, float yawDeg) noexcept {
        const float rad = deg2rad(yawDeg);
        const float s = sinf(rad), c = cosf(rad);
        const float lx = point.x - pivot.x;
        const float ly = point.y - pivot.y;
        return { lx * c - ly * s + pivot.x,
                 lx * s + ly * c + pivot.y,
                 point.z };
    }

    // Call once per frame per visible enemy BEFORE reading their bone position
    inline void Update(int idx, C_BasePlayer* player, const Matrix3x4* bones) noexcept {
        auto& st = g_states[idx];

        // Derive model yaw from pelvis→head vector (resilient to crouch/jump)
        const Vector pelvisW = bones[Bones::bone_pelvis].GetOrigin();
        const Vector headW   = bones[Bones::bone_head].GetOrigin();
        Vector dir = headW - pelvisW;
        dir.z = 0.f;
        const float modelYaw = (dir.Length2D() > 1.f)
            ? rad2deg(atan2f(dir.y, dir.x))
            : st.prevModelYaw;

        // Jitter: yaw oscillates ~180° between consecutive ticks
        const float d1 = normalize_yaw(modelYaw       - st.prevModelYaw);
        const float d2 = normalize_yaw(st.prevModelYaw - st.prevPrevModelYaw);
        st.jitter = (fabsf(fabsf(d1) - 180.f) < 25.f) ||
                    (fabsf(fabsf(d2) - 180.f) < 25.f);

        // Movement-based resolver: when a player moves, their real spine yaw
        // must follow their velocity. A large divergence means anti-aim.
        const Vector vel   = player->GetVelocity();
        const float  speed = vel.Length2D();
        st.antiAim = false;

        if (speed > 15.f) {
            const float velYaw    = rad2deg(atan2f(vel.y, vel.x));
            const float antidelta = normalize_yaw(modelYaw - velYaw);
            if (fabsf(antidelta) > 35.f) {
                st.antiAim        = true;
                // Negate the anti-aim offset to realign bones with velocity
                st.resolvedOffset = -antidelta;
            }
        }

        if (!st.antiAim) {
            if (st.jitter) {
                st.resolvedOffset = (fabsf(d1) > 90.f) ? 180.f : 0.f;
            } else {
                // Adaptive brute-force: prioritize offsets that have landed hits
                int bestBrute = st.bruteIndex;
                float bestScore = -1.f;
                for (int bi = 0; bi < static_cast<int>(std::size(bruteOffsets)); bi++) {
                    int total = st.hitCount[bi] + st.missCount[bi];
                    float score = (total > 0) ? (float)st.hitCount[bi] / (float)total : 0.5f;
                    if (score > bestScore) { bestScore = score; bestBrute = bi; }
                }
                if (bestScore > 0.6f && (st.hitCount[bestBrute] + st.missCount[bestBrute]) >= 3) {
                    st.bruteIndex = bestBrute;
                    st.confidence = bestScore;
                } else {
                    if (++st.ticksOnOffset >= 3) {
                        st.bruteIndex = (st.bruteIndex + 1) % static_cast<int>(std::size(bruteOffsets));
                        st.ticksOnOffset = 0;
                    }
                    st.confidence = 0.f;
                }
                st.resolvedOffset = bruteOffsets[st.bruteIndex];
            }
        }

        st.prevPrevModelYaw = st.prevModelYaw;
        st.prevModelYaw     = modelYaw;
    }

    // Apply the resolver's predicted offset to a bone world position
    inline Vector ResolvePos(int idx, const Vector& boneWorld, const Vector& entityOrigin) noexcept {
        const float off = g_states[idx].resolvedOffset;
        if (fabsf(off) < 0.5f) return boneWorld;
        return RotateAround(boneWorld, entityOrigin, off);
    }

    inline void RecordHit(int idx) {
        auto& st = g_states[idx];
        if (st.bruteIndex >= 0 && st.bruteIndex < static_cast<int>(std::size(bruteOffsets)))
            st.hitCount[st.bruteIndex]++;
    }

    inline void RecordMiss(int idx) {
        auto& st = g_states[idx];
        if (st.bruteIndex >= 0 && st.bruteIndex < static_cast<int>(std::size(bruteOffsets)))
            st.missCount[st.bruteIndex]++;
    }

} // namespace resolver
