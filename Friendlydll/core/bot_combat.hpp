#pragma once
#include "../includes.hpp"

namespace bot_combat {

    // ---- config toggles ----
    inline bool  enabled       = false;
    inline bool  auto_shoot    = false;
    inline bool  auto_retreat  = true;

    // ---- engagement configuration ----
    struct EngagementConfig {
        float engage_distance  = 800.f;
        float disengage_health = 25.f;
        bool  fight_back_only  = true;
        bool  protect_target   = true;
        bool  headshot_only    = false;
    };
    inline EngagementConfig g_engageCfg{};

    // ---- threat system ----
    static constexpr int MAX_THREATS = 32;

    struct Threat {
        int    idx       = -1;
        float  danger    = 0.f;
        float  distance  = 0.f;
        bool   visible   = false;
        bool   aiming_at_us = false;
        Vector pos{};
        int    health    = 0;
    };

    inline Threat g_threats[MAX_THREATS]{};
    inline int    g_threatCount = 0;

    // ---- health monitoring ----
    struct HealthState {
        int   current         = 100;
        int   previous        = 100;
        float lastDamageTime  = 0.f;
        int   totalDamageTaken = 0;
        bool  underFire       = false;
    };
    inline HealthState g_health{};

    // ---- cover system ----
    struct CoverSpot {
        Vector pos{};
        float  score = 0.f;
        bool   valid = false;
    };

    // ---- engagement state ----
    inline int   g_engagedTarget = -1;
    inline float g_engageStartTime = 0.f;
    inline bool  g_retreating = false;
    inline bool  g_wantFlee = false;
    inline Vector g_fleeFromPos{};

    // ---- trace helper (matching follow_bot convention) ----
    inline bool TraceVisible(Vector from, Vector to, C_BasePlayer* skip) {
        CTrace trace;
        TraceFilterSimple filter(skip);
        Ray_t ray;
        ray.Init(from, to);
        interfaces::trace->TraceRay(ray, MASK_SHOT, &filter, &trace);
        return trace.fraction > 0.97f;
    }

    // =========================================================================
    // 1. Threat Scanner
    // =========================================================================

    inline void ScanThreats(C_BasePlayer* lp) {
        g_threatCount = 0;
        if (!lp || !interfaces::trace) return;

        Vector eyePos = lp->GetEyePosition();
        Vector ownPos = lp->GetAbsOrigin();
        int localIdx  = interfaces::engine->GetLocalPlayer();
        const auto& bones = config::BoneRead();

        for (int i = 1; i <= interfaces::globalVars->maxClients && g_threatCount < MAX_THREATS; i++) {
            if (i == localIdx) continue;
            if (!bones[i].valid || bones[i].dormant) continue;
            if (bones[i].health <= 0) continue;
            if (!config::IsTargetAllowed(i)) continue;

            Vector theirPos = bones[i].absOrigin;
            float dx = theirPos.x - ownPos.x;
            float dy = theirPos.y - ownPos.y;
            float dz = theirPos.z - ownPos.z;
            float dist = sqrtf(dx * dx + dy * dy + dz * dz);
            if (dist < 1.f) continue;

            // --- visibility check ---
            Vector theirEye = { theirPos.x, theirPos.y, theirPos.z + 64.f };
            bool vis = TraceVisible(eyePos, theirEye, lp);

            // --- check if they are aiming at us (within 15 degrees) ---
            bool aimingAtUs = false;
            {
                C_BasePlayer* them = (C_BasePlayer*)interfaces::entityList->GetClientEntity(i);
                if (them && them->IsPlayer()) {
                    Vector theirEyePos = them->GetEyePosition();
                    Vector toUs = eyePos - theirEyePos;
                    float toUsDist = toUs.Length();
                    if (toUsDist > 1.f) {
                        toUs.x /= toUsDist;
                        toUs.y /= toUsDist;
                        toUs.z /= toUsDist;

                        // Reconstruct their aim direction from their bone data
                        // Use the vector from their position to their head bone as a rough facing estimate
                        // Better: try to get their actual view angles via entity
                        // Approximate: direction from pelvis to head gives a rough facing
                        if (!bones[i].noBones) {
                            Vector headPos = bones[i].bones[Bones::bone_head].GetOrigin();
                            Vector pelvisPos = bones[i].bones[Bones::bone_pelvis].GetOrigin();
                            Vector facing = headPos - pelvisPos;
                            // Head-pelvis is mostly vertical; use their eye-to-forward instead
                            // We approximate: their forward is the direction from their origin
                            // toward where their head is facing horizontally
                            // This is imperfect but works for threat detection
                            Vector horizForward = { headPos.x - pelvisPos.x, headPos.y - pelvisPos.y, 0.f };
                            float hLen = horizForward.Length2D();
                            if (hLen > 0.1f) {
                                horizForward.x /= hLen;
                                horizForward.y /= hLen;
                                horizForward.z = 0.f;

                                // Flatten toUs for horizontal comparison
                                Vector toUsFlat = { toUs.x, toUs.y, 0.f };
                                float flatLen = toUsFlat.Length2D();
                                if (flatLen > 0.01f) {
                                    toUsFlat.x /= flatLen;
                                    toUsFlat.y /= flatLen;
                                    float dot = horizForward.x * toUsFlat.x + horizForward.y * toUsFlat.y;
                                    // cos(15 deg) ~ 0.966
                                    if (dot > 0.966f) aimingAtUs = true;
                                }
                            }
                        }
                    }
                }
            }

            // --- danger score ---
            float danger = 1.f / (dist + 1.f);

            if (aimingAtUs)
                danger *= 2.f;

            // weapon check: if their weapon field is not empty, they are armed
            if (bones[i].weapon[0] != '\0')
                danger *= 1.5f;

            // if they are facing away from us entirely (dot < 0), reduce threat
            // (aiming_at_us already checked above, this handles the "away" case)
            if (!aimingAtUs) {
                // Check if facing completely away (> 90 degrees off)
                if (!bones[i].noBones) {
                    Vector headPos = bones[i].bones[Bones::bone_head].GetOrigin();
                    Vector pelvisPos = bones[i].bones[Bones::bone_pelvis].GetOrigin();
                    Vector horizForward = { headPos.x - pelvisPos.x, headPos.y - pelvisPos.y, 0.f };
                    float hLen = horizForward.Length2D();
                    if (hLen > 0.1f) {
                        horizForward.x /= hLen;
                        horizForward.y /= hLen;
                        Vector toUs2D = { ownPos.x - theirPos.x, ownPos.y - theirPos.y, 0.f };
                        float d2 = toUs2D.Length2D();
                        if (d2 > 0.01f) {
                            toUs2D.x /= d2;
                            toUs2D.y /= d2;
                            float dot2 = horizForward.x * toUs2D.x + horizForward.y * toUs2D.y;
                            if (dot2 < 0.f)
                                danger *= 0.5f;
                        }
                    }
                }
            }

            Threat& t = g_threats[g_threatCount];
            t.idx         = i;
            t.danger      = danger;
            t.distance    = dist;
            t.visible     = vis;
            t.aiming_at_us = aimingAtUs;
            t.pos         = theirPos;
            t.health      = bones[i].health;
            g_threatCount++;
        }

        // Sort by danger descending (simple insertion sort for small fixed array)
        for (int i = 1; i < g_threatCount; i++) {
            Threat key = g_threats[i];
            int j = i - 1;
            while (j >= 0 && g_threats[j].danger < key.danger) {
                g_threats[j + 1] = g_threats[j];
                j--;
            }
            g_threats[j + 1] = key;
        }
    }

    inline const Threat* GetTopThreat() {
        if (g_threatCount <= 0) return nullptr;
        return &g_threats[0];
    }

    inline int GetThreatsInRadius(float radius) {
        int count = 0;
        for (int i = 0; i < g_threatCount; i++) {
            if (g_threats[i].distance <= radius)
                count++;
        }
        return count;
    }

    // =========================================================================
    // 2. Engagement Rules
    // =========================================================================

    inline bool ShouldEngage(const Threat& t, C_BasePlayer* lp) {
        if (!lp) return false;

        // Out of engage range
        if (t.distance > g_engageCfg.engage_distance)
            return false;

        // Must be visible to fight
        if (!t.visible)
            return false;

        // Fight-back-only: only engage if they are aiming at us or we are under fire
        if (g_engageCfg.fight_back_only) {
            if (!t.aiming_at_us && !g_health.underFire)
                return false;
        }

        // Health too low to fight — retreat instead
        if (lp->GetHealth() <= (int)g_engageCfg.disengage_health)
            return false;

        return true;
    }

    inline bool ShouldProtectTarget(const Threat& t, int followTargetIdx) {
        if (!g_engageCfg.protect_target || followTargetIdx < 0)
            return false;

        const auto& bones = config::BoneRead();
        if (!bones[followTargetIdx].valid || bones[followTargetIdx].dormant)
            return false;

        // Check if threat is near our follow target (within 600 units)
        Vector targetPos = bones[followTargetIdx].absOrigin;
        float dx = t.pos.x - targetPos.x;
        float dy = t.pos.y - targetPos.y;
        float dz = t.pos.z - targetPos.z;
        float distToTarget = sqrtf(dx * dx + dy * dy + dz * dz);
        return distToTarget < 600.f;
    }

    inline bool ShouldRetreat(C_BasePlayer* lp) {
        if (!lp) return true;
        return lp->GetHealth() <= (int)g_engageCfg.disengage_health;
    }

    // =========================================================================
    // 3. Auto-Aim for Bot
    // =========================================================================

    inline Angle CalcAimAngles(Vector eyePos, Vector targetPos) {
        Vector delta = targetPos - eyePos;
        return Angle::FromVector(delta);
    }

    inline Angle SmoothAim(Angle current, Angle target, float smoothing) {
        if (smoothing <= 1.f) return target;

        Angle diff;
        diff.p = normalize_yaw(target.p - current.p);
        diff.y = normalize_yaw(target.y - current.y);
        diff.r = 0.f;

        Angle result;
        result.p = current.p + diff.p / smoothing;
        result.y = current.y + diff.y / smoothing;
        result.r = 0.f;
        result.Normalize();
        return result;
    }

    inline void ApplyBotAim(CUserCmd* cmd, C_BasePlayer* lp) {
        if (g_engagedTarget < 0 || !lp || !auto_shoot) return;

        const auto& bones = config::BoneRead();
        if (!bones[g_engagedTarget].valid || bones[g_engagedTarget].dormant) {
            g_engagedTarget = -1;
            return;
        }

        // Pick aim bone
        int aimBone = g_engageCfg.headshot_only ? Bones::bone_head : Bones::bone_pelvis;
        if (bones[g_engagedTarget].noBones) {
            // Fallback: aim at center mass (origin + 48 units up)
            Vector fallbackPos = bones[g_engagedTarget].absOrigin;
            fallbackPos.z += 48.f;

            Vector eyePos = lp->GetEyePosition();
            Angle desired = CalcAimAngles(eyePos, fallbackPos);
            Angle smoothed = SmoothAim(cmd->viewangles, desired, 3.f);
            cmd->viewangles = smoothed;

            // Check if on target (within 3 degrees)
            float dp = normalize_yaw(desired.p - smoothed.p);
            float dy = normalize_yaw(desired.y - smoothed.y);
            float fovDist = sqrtf(dp * dp + dy * dy);
            if (fovDist < 3.f) {
                cmd->buttons |= CUserCmd::IN_ATTACK;
            }
            return;
        }

        Vector targetBonePos = bones[g_engagedTarget].bones[aimBone].GetOrigin();
        Vector eyePos = lp->GetEyePosition();

        // Verify the bone position is sane (not at world origin)
        if (targetBonePos.x == 0.f && targetBonePos.y == 0.f && targetBonePos.z == 0.f) {
            targetBonePos = bones[g_engagedTarget].absOrigin;
            targetBonePos.z += 64.f;
        }

        Angle desired = CalcAimAngles(eyePos, targetBonePos);
        Angle smoothed = SmoothAim(cmd->viewangles, desired, 3.f);
        cmd->viewangles = smoothed;

        // Check if on target (within 3 degrees FOV)
        float dp = normalize_yaw(desired.p - smoothed.p);
        float dy = normalize_yaw(desired.y - smoothed.y);
        float fovDist = sqrtf(dp * dp + dy * dy);
        if (fovDist < 3.f) {
            cmd->buttons |= CUserCmd::IN_ATTACK;
        }
    }

    // =========================================================================
    // 4. Cover System
    // =========================================================================

    inline Vector RetreatDirection(Vector ownPos, Vector threatPos) {
        Vector away = ownPos - threatPos;
        float len = away.Length();
        if (len < 1.f) return Vector(0.f, 1.f, 0.f);
        return Vector(away.x / len, away.y / len, 0.f);
    }

    inline CoverSpot FindCover(Vector ownPos, Vector threatPos, C_BasePlayer* lp) {
        CoverSpot best{};
        best.valid = false;
        best.score = -1.f;

        if (!lp || !interfaces::trace) return best;

        // Sample 8 directions at 200 unit radius
        for (int i = 0; i < 8; i++) {
            float angle = (float)i * (360.f / 8.f);
            float rad = deg2rad(angle);
            Vector samplePos = {
                ownPos.x + cosf(rad) * 200.f,
                ownPos.y + sinf(rad) * 200.f,
                ownPos.z
            };

            // Check if we can walk there (trace from us to sample)
            CTrace walkTrace;
            TraceFilterSimple walkFilter(lp);
            Ray_t walkRay;
            walkRay.Init(ownPos, samplePos);
            interfaces::trace->TraceRay(walkRay, MASK_PLAYERSOLID, &walkFilter, &walkTrace);
            if (walkTrace.fraction < 0.8f) continue; // can't reach it

            Vector reachablePos = walkTrace.endPos;

            // Trace from sample position to threat — if blocked, it is cover
            Vector sampleEye = { reachablePos.x, reachablePos.y, reachablePos.z + 64.f };
            Vector threatEye = { threatPos.x, threatPos.y, threatPos.z + 64.f };

            CTrace coverTrace;
            TraceFilterSimple coverFilter(lp);
            Ray_t coverRay;
            coverRay.Init(sampleEye, threatEye);
            interfaces::trace->TraceRay(coverRay, MASK_SHOT, &coverFilter, &coverTrace);

            // Score: lower fraction means more cover
            float score = 1.f - coverTrace.fraction;

            // Bonus: prefer positions farther from threat
            float dxT = reachablePos.x - threatPos.x;
            float dyT = reachablePos.y - threatPos.y;
            float distToThreat = sqrtf(dxT * dxT + dyT * dyT);
            score += distToThreat * 0.0005f;

            if (score > best.score) {
                best.pos   = reachablePos;
                best.score = score;
                best.valid = true;
            }
        }

        return best;
    }

    // =========================================================================
    // 5. Health Monitoring
    // =========================================================================

    inline void UpdateHealth(C_BasePlayer* lp) {
        if (!lp) return;

        float curtime = interfaces::globalVars->curtime;
        int hp = lp->GetHealth();

        g_health.previous = g_health.current;
        g_health.current  = hp;

        if (hp < g_health.previous) {
            int dmg = g_health.previous - hp;
            g_health.totalDamageTaken += dmg;
            g_health.lastDamageTime = curtime;
            g_health.underFire = true;
        }

        // Clear underFire after 2 seconds of no damage
        if (g_health.underFire && (curtime - g_health.lastDamageTime) > 2.f) {
            g_health.underFire = false;
        }
    }

    inline bool IsUnderFire() {
        return g_health.underFire;
    }

    inline float GetDamageRate() {
        float curtime = interfaces::globalVars->curtime;
        float elapsed = curtime - g_health.lastDamageTime;
        // DPS over the last 2 second window
        if (elapsed > 2.f || elapsed < 0.001f) return 0.f;
        return (float)g_health.totalDamageTaken / (elapsed + 0.001f);
    }

    // =========================================================================
    // 6. Integration — main per-tick update
    // =========================================================================

    inline void Update(CUserCmd* cmd, C_BasePlayer* lp, int followTargetIdx) {
        if (!enabled || !lp || !cmd) return;
        if (!interfaces::trace || !interfaces::engine) return;

        // 1. Scan threats
        ScanThreats(lp);

        // 2. Update health tracking
        UpdateHealth(lp);

        // 3. Decide engagement
        g_engagedTarget = -1;
        g_retreating = false;

        // Check if we should retreat
        if (auto_retreat && ShouldRetreat(lp)) {
            g_retreating = true;
            g_engagedTarget = -1;
        }

        if (!g_retreating) {
            // Try to find a target to engage
            for (int i = 0; i < g_threatCount; i++) {
                const Threat& t = g_threats[i];

                bool shouldFight = ShouldEngage(t, lp);

                // Also consider protecting our follow target
                if (!shouldFight && t.visible && t.distance < g_engageCfg.engage_distance) {
                    shouldFight = ShouldProtectTarget(t, followTargetIdx);
                }

                if (shouldFight) {
                    g_engagedTarget = t.idx;
                    break;
                }
            }
        }

        // 4. Apply aim if fighting
        if (g_engagedTarget >= 0 && auto_shoot) {
            ApplyBotAim(cmd, lp);
        }

        // 5. Retreat: set flag for hooks.cpp to trigger flee (no movement here)
        if (g_retreating && auto_retreat) {
            const Threat* top = GetTopThreat();
            if (top) {
                g_wantFlee = true;
                g_fleeFromPos = top->pos;
            }
        }
    }

    // =========================================================================
    // Reset all state
    // =========================================================================

    inline void Reset() {
        g_threatCount   = 0;
        g_engagedTarget = -1;
        g_engageStartTime = 0.f;
        g_retreating    = false;
        g_wantFlee      = false;
        g_fleeFromPos   = Vector(0.f, 0.f, 0.f);

        g_health.current         = 100;
        g_health.previous        = 100;
        g_health.lastDamageTime  = 0.f;
        g_health.totalDamageTaken = 0;
        g_health.underFire       = false;

        for (int i = 0; i < MAX_THREATS; i++)
            g_threats[i] = Threat{};
    }

} // namespace bot_combat
