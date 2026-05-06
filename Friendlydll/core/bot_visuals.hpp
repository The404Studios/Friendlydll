#pragma once
#include "../includes.hpp"

// Forward-declared sibling bot namespaces (headers included before this one)
// bot_combat, bot_tasks, bot_nav

namespace bot_visuals {

    // ---- config ----
    inline bool drawHUD     = true;
    inline bool drawPath    = true;
    inline bool drawThreats = true;
    inline bool drawMinimap = false;
    inline float hudScale   = 1.0f;
    inline ImVec2 hudPos    = {10.f, 200.f};

    // ---- color constants ----
    static constexpr ImU32 COL_PANEL_BG      = IM_COL32(12, 12, 18, 210);
    static constexpr ImU32 COL_PANEL_BORDER  = IM_COL32(0, 180, 216, 120);
    static constexpr ImU32 COL_TEXT_DIM      = IM_COL32(160, 160, 170, 200);
    static constexpr ImU32 COL_TEXT_BRIGHT   = IM_COL32(230, 235, 240, 255);
    static constexpr ImU32 COL_TEXT_SHADOW   = IM_COL32(0, 0, 0, 200);
    static constexpr ImU32 COL_ACCENT        = IM_COL32(0, 200, 255, 220);
    static constexpr ImU32 COL_HEALTH_HIGH   = IM_COL32(50, 220, 80, 255);
    static constexpr ImU32 COL_HEALTH_MED    = IM_COL32(220, 200, 30, 255);
    static constexpr ImU32 COL_HEALTH_LOW    = IM_COL32(220, 40, 30, 255);
    static constexpr ImU32 COL_THREAT_HIGH   = IM_COL32(255, 40, 40, 230);
    static constexpr ImU32 COL_THREAT_MED    = IM_COL32(255, 200, 40, 210);
    static constexpr ImU32 COL_THREAT_LOW    = IM_COL32(140, 140, 150, 160);
    static constexpr ImU32 COL_PATH_NEAR     = IM_COL32(0, 230, 255, 200);
    static constexpr ImU32 COL_PATH_FAR      = IM_COL32(30, 60, 200, 160);
    static constexpr ImU32 COL_PATH_PULSE    = IM_COL32(255, 255, 255, 240);
    static constexpr ImU32 COL_GUARD_OK      = IM_COL32(50, 220, 80, 180);
    static constexpr ImU32 COL_GUARD_RETURN  = IM_COL32(220, 200, 30, 180);
    static constexpr ImU32 COL_PATROL_LINE   = IM_COL32(60, 140, 255, 140);
    static constexpr ImU32 COL_PATROL_ACTIVE = IM_COL32(80, 180, 255, 240);
    static constexpr ImU32 COL_PATROL_DONE   = IM_COL32(80, 180, 255, 180);
    static constexpr ImU32 COL_PATROL_PEND   = IM_COL32(60, 100, 180, 120);
    static constexpr ImU32 COL_FARM_MONEY    = IM_COL32(255, 220, 40, 220);
    static constexpr ImU32 COL_FARM_PRINTER  = IM_COL32(0, 220, 180, 220);
    static constexpr ImU32 COL_FARM_WEAPON   = IM_COL32(180, 140, 255, 220);
    static constexpr ImU32 COL_MINIMAP_BG    = IM_COL32(10, 10, 16, 200);
    static constexpr ImU32 COL_MINIMAP_RING  = IM_COL32(0, 180, 216, 140);
    static constexpr ImU32 COL_FLEE_BG       = IM_COL32(120, 20, 20, 200);
    static constexpr ImU32 COL_UNDERFIRE     = IM_COL32(255, 30, 30, 255);

    // ---- mode colors ----
    inline ImU32 GetModeColor(int mode) {
        // 0=Idle 1=Follow 2=Guard 3=Patrol 4=Farm 5=Flee
        switch (mode) {
            case 0: return IM_COL32(160, 160, 170, 220); // idle  - gray
            case 1: return IM_COL32(0, 200, 255, 240);   // follow - cyan
            case 2: return IM_COL32(50, 220, 80, 240);   // guard  - green
            case 3: return IM_COL32(80, 140, 255, 240);  // patrol - blue
            case 4: return IM_COL32(255, 200, 40, 240);  // farm   - gold
            case 5: return IM_COL32(255, 50, 50, 240);   // flee   - red
            default: return COL_TEXT_DIM;
        }
    }

    // ---- helpers ----
    inline ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
        if (t <= 0.f) return a;
        if (t >= 1.f) return b;
        int ra = (a >> 0) & 0xFF, ga = (a >> 8) & 0xFF, ba_ = (a >> 16) & 0xFF, aa = (a >> 24) & 0xFF;
        int rb = (b >> 0) & 0xFF, gb = (b >> 8) & 0xFF, bb  = (b >> 16) & 0xFF, ab = (b >> 24) & 0xFF;
        return IM_COL32(
            ra + (int)((rb - ra) * t),
            ga + (int)((gb - ga) * t),
            ba_ + (int)((bb - ba_) * t),
            aa + (int)((ab - aa) * t));
    }

    inline ImU32 WithAlpha(ImU32 col, int a) {
        return (col & 0x00FFFFFF) | ((ImU32)a << 24);
    }

    inline ImU32 HealthColor(int hp, int maxHp = 100) {
        float frac = (float)hp / (float)maxHp;
        if (frac > 0.6f) return LerpColor(COL_HEALTH_MED, COL_HEALTH_HIGH, (frac - 0.6f) / 0.4f);
        if (frac > 0.25f) return LerpColor(COL_HEALTH_LOW, COL_HEALTH_MED, (frac - 0.25f) / 0.35f);
        return COL_HEALTH_LOW;
    }

    // ---- shadowed text helper ----
    inline void DrawTextShadow(ImDrawList* dl, ImFont* font, float size,
                               ImVec2 pos, ImU32 col, const char* text) {
        dl->AddText(font, size, ImVec2(pos.x + 1.f, pos.y + 1.f), COL_TEXT_SHADOW, text);
        dl->AddText(font, size, pos, col, text);
    }

    // ========================================================================
    // 1. STATUS HUD PANEL
    // ========================================================================

    // Measures HUD content height without drawing, so background can be sized
    // accurately before content is rendered on top.
    inline float MeasureHUDHeight(float lineH, float pad, int mode,
                                  bool underFire, bool hasTarget) {
        float h = pad;           // top padding
        h += lineH;             // mode line
        h += lineH;             // health bar
        if (underFire) h += lineH;
        h += lineH * 2.f;      // target info (2 lines always)
        h += lineH;             // threat count
        h += lineH * 2.f;      // mode-specific (always 2 lines)
        h += pad;               // bottom padding
        return h;
    }

    inline void DrawStatusHUD(ImDrawList* dl, ImFont* font, float fontSize,
                              int screenW, int screenH) {
        if (!drawHUD) return;
        if (!follow_bot::enabled) return;

        const float sc      = hudScale;
        const float fs      = fontSize * sc;
        const float fsSmall = fs * 0.85f;
        const float lineH   = fs + 3.f;
        const float pad     = 8.f * sc;
        const float panelW  = 220.f * sc;
        const float left    = hudPos.x;
        const float textX   = left + pad;
        const float curtime = interfaces::globalVars->curtime;

        int mode = (int)bot_tasks::currentMode;
        ImU32 modeCol = GetModeColor(mode);

        // Measure actual content height for accurate background
        bool underFire = bot_combat::g_health.underFire;
        bool hasTarget = follow_bot::targetIdx >= 0 && follow_bot::targetIdx < 128;
        float contentH = MeasureHUDHeight(lineH, pad, mode, underFire, hasTarget);

        // ---- Draw panel background first ----
        dl->AddRectFilled(
            ImVec2(left, hudPos.y),
            ImVec2(left + panelW, hudPos.y + contentH),
            COL_PANEL_BG, 6.f * sc);
        dl->AddRect(
            ImVec2(left, hudPos.y),
            ImVec2(left + panelW, hudPos.y + contentH),
            COL_PANEL_BORDER, 6.f * sc, 0, 1.f);

        // Accent line at top (colored by mode)
        dl->AddRectFilled(
            ImVec2(left + 2.f, hudPos.y),
            ImVec2(left + panelW - 2.f, hudPos.y + 2.f * sc),
            modeCol);

        // ---- Now draw all content on top ----
        float curY = hudPos.y + pad;

        // Bot mode label
        const char* modeName = bot_tasks::GetModeName();
        char modeBuf[48];
        snprintf(modeBuf, sizeof(modeBuf), "BOT: %s", modeName);
        DrawTextShadow(dl, font, fs, ImVec2(textX, curY), modeCol, modeBuf);
        curY += lineH;

        // Health bar
        int hp = 0;
        {
            int localIdx = interfaces::engine->GetLocalPlayer();
            if (localIdx >= 0 && localIdx < 128) {
                auto& bones = config::BoneRead();
                if (bones[localIdx].valid)
                    hp = bones[localIdx].health;
            }
        }
        int currentHp = bot_combat::g_health.current > 0 ? bot_combat::g_health.current : hp;
        if (currentHp <= 0) currentHp = hp;

        {
            float barX = textX;
            float barY = curY + 2.f;
            float barW = panelW - pad * 2.f;
            float barH = fs * 0.6f;
            float frac = (float)currentHp / 100.f;
            if (frac < 0.f) frac = 0.f;
            if (frac > 1.f) frac = 1.f;

            dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                              IM_COL32(30, 30, 36, 200), 2.f);
            if (frac > 0.f)
                dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * frac, barY + barH),
                                  HealthColor(currentHp), 2.f);
            dl->AddRect(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                        IM_COL32(80, 80, 90, 150), 2.f);

            char hpBuf[16];
            snprintf(hpBuf, sizeof(hpBuf), "%d HP", currentHp);
            ImVec2 hpSz = font->CalcTextSizeA(fsSmall, FLT_MAX, 0.f, hpBuf);
            dl->AddText(font, fsSmall,
                        ImVec2(barX + (barW - hpSz.x) * 0.5f, barY + (barH - hpSz.y) * 0.5f),
                        COL_TEXT_BRIGHT, hpBuf);
        }
        curY += lineH;

        // UNDER FIRE warning (flashing)
        if (underFire) {
            float flash = sinf(curtime * 6.f);
            if (flash > 0.f) {
                int alpha = (int)(flash * 255.f);
                DrawTextShadow(dl, font, fs, ImVec2(textX, curY),
                               WithAlpha(COL_UNDERFIRE, alpha), "!! UNDER FIRE !!");
            }
            curY += lineH;
        }

        // Target info
        if (hasTarget) {
            auto& bones = config::BoneRead();
            const auto& target = bones[follow_bot::targetIdx];
            if (target.valid) {
                const char* tname = target.rpName[0] ? target.rpName : target.name;
                char targetBuf[96];
                snprintf(targetBuf, sizeof(targetBuf), "Target: %s", tname);
                DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY), COL_ACCENT, targetBuf);
                curY += lineH;

                char distBuf[48];
                snprintf(distBuf, sizeof(distBuf), "Distance: %.0f u (%.0fm)",
                         target.distance, target.distance / 52.49f);
                DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY), COL_TEXT_DIM, distBuf);
                curY += lineH;
            } else {
                DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY), COL_TEXT_DIM, "Target lost");
                curY += lineH * 2.f;
            }
        } else {
            DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY), COL_TEXT_DIM, "No target");
            curY += lineH * 2.f;
        }

        // Threat count
        {
            int threats = bot_combat::g_threatCount;
            char threatBuf[32];
            snprintf(threatBuf, sizeof(threatBuf), "Threats: %d", threats);
            ImU32 threatCol = threats > 3 ? COL_THREAT_HIGH :
                              threats > 0 ? COL_THREAT_MED : COL_TEXT_DIM;
            DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY), threatCol, threatBuf);
            curY += lineH;
        }

        // Mode-specific details (always exactly 2 lines)
        switch (mode) {
            case 1: { // Follow
                const char* pathStr = follow_bot::g_hasPath ? "Path: OK" : "Path: LOST";
                ImU32 pathCol = follow_bot::g_hasPath ? COL_HEALTH_HIGH : COL_THREAT_HIGH;
                DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY), pathCol, pathStr);
                curY += lineH;

                char crumbBuf[32];
                snprintf(crumbBuf, sizeof(crumbBuf), "Crumbs: %d/%d",
                         follow_bot::g_crumbCount, follow_bot::MAX_CRUMBS);
                DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY), COL_TEXT_DIM, crumbBuf);
                break;
            }
            case 2: { // Guard
                float gx = bot_tasks::guardPos.x;
                float gy = bot_tasks::guardPos.y;
                int localIdx = interfaces::engine->GetLocalPlayer();
                float postDist = 0.f;
                if (localIdx >= 0 && localIdx < 128) {
                    auto& bones = config::BoneRead();
                    if (bones[localIdx].valid) {
                        float dx = bones[localIdx].absOrigin.x - gx;
                        float dy = bones[localIdx].absOrigin.y - gy;
                        postDist = sqrtf(dx * dx + dy * dy);
                    }
                }
                bool onPost = postDist <= bot_tasks::guardRadius;
                const char* guardStr = onPost ? "ON POST" : "RETURNING";
                ImU32 guardCol = onPost ? COL_GUARD_OK : COL_GUARD_RETURN;
                DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY), guardCol, guardStr);
                curY += lineH;

                char postBuf[48];
                snprintf(postBuf, sizeof(postBuf), "Post dist: %.0f u", postDist);
                DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY), COL_TEXT_DIM, postBuf);
                break;
            }
            case 3: { // Patrol
                char wpBuf[48];
                snprintf(wpBuf, sizeof(wpBuf), "Waypoint: %d / %d",
                         bot_tasks::patrolIdx + 1, bot_tasks::patrolCount);
                DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY), COL_PATROL_ACTIVE, wpBuf);
                curY += lineH;

                // Progress bar
                if (bot_tasks::patrolCount > 0) {
                    float barX = textX;
                    float barY = curY + 2.f;
                    float barW = panelW - pad * 2.f;
                    float barH = fs * 0.4f;
                    float frac = (float)(bot_tasks::patrolIdx + 1) / (float)bot_tasks::patrolCount;
                    dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
                                      IM_COL32(30, 30, 50, 200), 2.f);
                    dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * frac, barY + barH),
                                      COL_PATROL_ACTIVE, 2.f);
                }
                break;
            }
            case 4: { // Farm
                DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY),
                               COL_FARM_MONEY, "Scanning for loot...");
                break;
            }
            case 5: { // Flee
                float remaining = bot_tasks::fleeDuration - bot_tasks::fleeTimer;
                if (remaining < 0.f) remaining = 0.f;

                // Pulsing flee background accent behind the text
                float pulse = sinf(curtime * 5.f) * 0.15f + 0.85f;
                int pulseAlpha = (int)(pulse * 200.f);
                dl->AddRectFilled(
                    ImVec2(left + 1.f, curY - 1.f),
                    ImVec2(left + panelW - 1.f, curY + lineH * 2.f + 2.f),
                    WithAlpha(COL_FLEE_BG, pulseAlpha), 3.f);

                char fleeBuf[48];
                snprintf(fleeBuf, sizeof(fleeBuf), "FLEE: %.1fs left", remaining);
                DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY),
                               COL_THREAT_HIGH, fleeBuf);
                curY += lineH;
                DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY),
                               COL_TEXT_DIM, "Avoiding threats...");
                break;
            }
            default: { // Idle
                DrawTextShadow(dl, font, fsSmall, ImVec2(textX, curY),
                               COL_TEXT_DIM, "Standing by...");
                break;
            }
        }
    }

    // ========================================================================
    // 2. ENHANCED PATH VISUALIZATION
    // ========================================================================
    inline void DrawEnhancedPath(ImDrawList* dl, ImFont* font, float fontSize,
                                 int screenW, int screenH) {
        if (!drawPath) return;
        if (!follow_bot::enabled || follow_bot::targetIdx < 0) return;

        auto& bones = config::BoneRead();
        if (!bones[follow_bot::targetIdx].valid) return;

        int localIdx = interfaces::engine->GetLocalPlayer();
        if (localIdx < 0 || localIdx >= 128 || !bones[localIdx].valid) return;

        float curtime = interfaces::globalVars->curtime;

        // Collect screen-space breadcrumb points
        struct ScreenPt { float x, y; bool valid; float worldDist; };
        ScreenPt pts[follow_bot::MAX_CRUMBS + 2]; // +2 for local pos and target pos
        int ptCount = 0;

        // Start with local player position
        Vector ownPos = bones[localIdx].absOrigin;
        ownPos.z += 36.f;
        {
            float sx, sy;
            if (config::WorldToScreen(ownPos, sx, sy)) {
                pts[ptCount++] = {sx, sy, true, 0.f};
            }
        }

        // Add breadcrumb points (oldest to newest toward target)
        for (int i = follow_bot::g_crumbCount - 1; i >= 0; i--) {
            int idx = (follow_bot::g_crumbHead - 1 - i + follow_bot::MAX_CRUMBS * 2) % follow_bot::MAX_CRUMBS;
            Vector cp = follow_bot::g_crumbs[idx].pos;
            cp.z += 10.f;
            float sx, sy;
            if (config::WorldToScreen(cp, sx, sy)) {
                float dx = cp.x - ownPos.x;
                float dy = cp.y - ownPos.y;
                pts[ptCount++] = {sx, sy, true, sqrtf(dx * dx + dy * dy)};
                if (ptCount >= follow_bot::MAX_CRUMBS + 1) break;
            }
        }

        // End with target position
        Vector targetPos = bones[follow_bot::targetIdx].absOrigin;
        targetPos.z += 36.f;
        {
            float sx, sy;
            if (config::WorldToScreen(targetPos, sx, sy)) {
                float dx = targetPos.x - ownPos.x;
                float dy = targetPos.y - ownPos.y;
                pts[ptCount++] = {sx, sy, true, sqrtf(dx * dx + dy * dy)};
            }
        }

        if (ptCount < 2) return;

        // Find total screen-space path length for pulse animation
        float totalPathLen = 0.f;
        float segLengths[follow_bot::MAX_CRUMBS + 2];
        segLengths[0] = 0.f;
        for (int i = 1; i < ptCount; i++) {
            float dx = pts[i].x - pts[i - 1].x;
            float dy = pts[i].y - pts[i - 1].y;
            float segLen = sqrtf(dx * dx + dy * dy);
            totalPathLen += segLen;
            segLengths[i] = totalPathLen;
        }

        if (totalPathLen < 5.f) return;

        // Find max world distance for gradient
        float maxDist = 1.f;
        for (int i = 0; i < ptCount; i++)
            if (pts[i].worldDist > maxDist) maxDist = pts[i].worldDist;

        // Draw bezier curves between consecutive points with color gradient
        for (int i = 0; i < ptCount - 1; i++) {
            float t0 = (float)i / (float)(ptCount - 1);
            float t1 = (float)(i + 1) / (float)(ptCount - 1);
            ImU32 c0 = LerpColor(COL_PATH_NEAR, COL_PATH_FAR, t0);
            ImU32 c1 = LerpColor(COL_PATH_NEAR, COL_PATH_FAR, t1);
            ImU32 segCol = LerpColor(c0, c1, 0.5f);

            float mx = (pts[i].x + pts[i + 1].x) * 0.5f;
            float my = (pts[i].y + pts[i + 1].y) * 0.5f;

            // Compute control points for a smooth curve
            // Use slight perpendicular offset for organic look
            float dx = pts[i + 1].x - pts[i].x;
            float dy = pts[i + 1].y - pts[i].y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len < 1.f) continue;

            float nx = -dy / len;
            float ny = dx / len;

            // Alternate curve direction based on index for a sinuous path
            float curveAmount = 8.f * ((i % 2 == 0) ? 1.f : -1.f);

            ImVec2 p1(pts[i].x, pts[i].y);
            ImVec2 p4(pts[i + 1].x, pts[i + 1].y);
            ImVec2 p2(pts[i].x + dx * 0.33f + nx * curveAmount,
                      pts[i].y + dy * 0.33f + ny * curveAmount);
            ImVec2 p3(pts[i].x + dx * 0.66f + nx * curveAmount,
                      pts[i].y + dy * 0.66f + ny * curveAmount);

            dl->AddBezierCubic(p1, p2, p3, p4, segCol, 2.f, 16);
        }

        // Animated pulse dot traveling along the path
        float pulseSpeed = 0.3f; // normalized speed per second
        float pulseFrac = fmodf(curtime * pulseSpeed, 1.f);
        float pulseDist = pulseFrac * totalPathLen;

        // Find which segment the pulse is on
        for (int i = 1; i < ptCount; i++) {
            if (pulseDist <= segLengths[i] || i == ptCount - 1) {
                float segStart = segLengths[i - 1];
                float segEnd = segLengths[i];
                float segLen = segEnd - segStart;
                float localT = (segLen > 0.f) ? (pulseDist - segStart) / segLen : 0.f;
                if (localT < 0.f) localT = 0.f;
                if (localT > 1.f) localT = 1.f;

                float px = pts[i - 1].x + (pts[i].x - pts[i - 1].x) * localT;
                float py = pts[i - 1].y + (pts[i].y - pts[i - 1].y) * localT;

                // Glow ring
                dl->AddCircleFilled(ImVec2(px, py), 6.f, IM_COL32(0, 200, 255, 60), 12);
                dl->AddCircleFilled(ImVec2(px, py), 3.5f, COL_PATH_PULSE, 10);
                break;
            }
        }

        // Arrow head at the end (pointing toward target)
        if (ptCount >= 2) {
            float ex = pts[ptCount - 1].x;
            float ey = pts[ptCount - 1].y;
            float px = pts[ptCount - 2].x;
            float py = pts[ptCount - 2].y;
            float adx = ex - px;
            float ady = ey - py;
            float alen = sqrtf(adx * adx + ady * ady);
            if (alen > 5.f) {
                float ax = adx / alen;
                float ay = ady / alen;
                float arrowLen = 10.f;
                float arrowW = 5.f;
                ImVec2 tip(ex, ey);
                ImVec2 arrL(ex - ax * arrowLen + ay * arrowW, ey - ay * arrowLen - ax * arrowW);
                ImVec2 arrR(ex - ax * arrowLen - ay * arrowW, ey - ay * arrowLen + ax * arrowW);
                dl->AddTriangleFilled(tip, arrL, arrR, COL_PATH_FAR);
            }
        }
    }

    // ========================================================================
    // 3. THREAT DIRECTION INDICATORS
    // ========================================================================
    inline void DrawThreatIndicators(ImDrawList* dl, ImFont* font, float fontSize,
                                     int screenW, int screenH) {
        if (!drawThreats) return;
        if (!bot_combat::enabled) return;
        if (bot_combat::g_threatCount <= 0) return;

        int localIdx = interfaces::engine->GetLocalPlayer();
        if (localIdx < 0 || localIdx >= 128) return;

        auto& bones = config::BoneRead();
        if (!bones[localIdx].valid) return;

        Vector ownPos = bones[localIdx].absOrigin;
        float halfW = (float)screenW * 0.5f;
        float halfH = (float)screenH * 0.5f;
        float margin = 40.f;

        // Get camera forward direction from the view matrix
        int ri = config::g_viewReadIdx.load(std::memory_order_acquire);
        const auto& vm = config::g_viewMatrix[ri];

        // Extract camera angles from view matrix
        float camForwardX = vm[0][0];
        float camForwardY = vm[0][1];
        float camRightX = vm[1][0];
        float camRightY = vm[1][1];

        int maxThreats = bot_combat::g_threatCount;
        if (maxThreats > 16) maxThreats = 16;

        for (int i = 0; i < maxThreats; i++) {
            const auto& threat = bot_combat::g_threats[i];
            if (threat.idx <= 0 || threat.idx >= 128) continue;

            // Check if threat is already on screen
            float tsx, tsy;
            Vector threatPos = threat.pos;
            threatPos.z += 36.f;
            if (config::WorldToScreen(threatPos, tsx, tsy)) {
                if (tsx >= margin && tsx <= screenW - margin &&
                    tsy >= margin && tsy <= screenH - margin)
                    continue; // on screen, skip indicator
            }

            // Direction from us to threat in world space
            float dx = threat.pos.x - ownPos.x;
            float dy = threat.pos.y - ownPos.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist < 1.f) continue;

            // Project to screen-relative direction using camera axes
            float viewX = dx * camForwardX + dy * camForwardY;
            float viewY = dx * camRightX + dy * camRightY;
            float vLen = sqrtf(viewX * viewX + viewY * viewY);
            if (vLen < 0.01f) continue;
            viewX /= vLen;
            viewY /= vLen;

            // Map to screen edge
            float edgeX = halfW + viewY * (halfW - margin);
            float edgeY = halfH - viewX * (halfH - margin);

            // Clamp to screen edges
            if (edgeX < margin) edgeX = margin;
            if (edgeX > screenW - margin) edgeX = (float)screenW - margin;
            if (edgeY < margin) edgeY = margin;
            if (edgeY > screenH - margin) edgeY = (float)screenH - margin;

            // Color based on danger level
            ImU32 arrowCol;
            float arrowSize;
            if (threat.danger > 0.7f) {
                arrowCol = COL_THREAT_HIGH;
                arrowSize = 14.f;
            } else if (threat.danger > 0.3f) {
                arrowCol = COL_THREAT_MED;
                arrowSize = 11.f;
            } else {
                arrowCol = COL_THREAT_LOW;
                arrowSize = 8.f;
            }

            // Draw arrow pointing toward threat
            float adx = viewY;
            float ady = -viewX;
            float alen = sqrtf(adx * adx + ady * ady);
            if (alen > 0.01f) { adx /= alen; ady /= alen; }

            ImVec2 tip(edgeX + adx * arrowSize, edgeY + ady * arrowSize);
            ImVec2 bl(edgeX - adx * arrowSize * 0.4f + ady * arrowSize * 0.5f,
                      edgeY - ady * arrowSize * 0.4f - adx * arrowSize * 0.5f);
            ImVec2 br(edgeX - adx * arrowSize * 0.4f - ady * arrowSize * 0.5f,
                      edgeY - ady * arrowSize * 0.4f + adx * arrowSize * 0.5f);

            dl->AddTriangleFilled(tip, bl, br, arrowCol);

            // Distance text
            char distBuf[24];
            snprintf(distBuf, sizeof(distBuf), "%.0fm", threat.distance / 52.49f);
            ImVec2 distSz = font->CalcTextSizeA(fontSize * 0.7f, FLT_MAX, 0.f, distBuf);
            float dtx = edgeX - distSz.x * 0.5f;
            float dty = edgeY + arrowSize + 2.f;
            // Keep text on screen
            if (dtx < 2.f) dtx = 2.f;
            if (dtx + distSz.x > screenW - 2.f) dtx = screenW - 2.f - distSz.x;
            if (dty + distSz.y > screenH - 2.f) dty = edgeY - arrowSize - distSz.y - 2.f;

            DrawTextShadow(dl, font, fontSize * 0.7f, ImVec2(dtx, dty), arrowCol, distBuf);
        }
    }

    // ========================================================================
    // 4. GUARD RADIUS CIRCLE
    // ========================================================================
    inline void DrawGuardRadius(ImDrawList* dl, ImFont* font, float fontSize,
                                int screenW, int screenH) {
        if (bot_tasks::currentMode != bot_tasks::BotMode::Guard) return; // Only in Guard mode

        Vector guardCenter = bot_tasks::guardPos;
        float radius = bot_tasks::guardRadius;
        if (radius < 10.f) return;

        // Determine guard status color
        int localIdx = interfaces::engine->GetLocalPlayer();
        float postDist = 0.f;
        if (localIdx >= 0 && localIdx < 128) {
            auto& bones = config::BoneRead();
            if (bones[localIdx].valid) {
                float dx = bones[localIdx].absOrigin.x - guardCenter.x;
                float dy = bones[localIdx].absOrigin.y - guardCenter.y;
                postDist = sqrtf(dx * dx + dy * dy);
            }
        }
        bool onPost = postDist <= radius;
        ImU32 circleCol = onPost ? COL_GUARD_OK : COL_GUARD_RETURN;

        // Project circle perimeter points
        constexpr int SEGMENTS = 32;
        float screenPts[SEGMENTS][2];
        bool  ptValid[SEGMENTS];
        int   validCount = 0;

        for (int i = 0; i < SEGMENTS; i++) {
            float angle = (float)i / (float)SEGMENTS * 2.f * M_PI_F;
            Vector worldPt;
            worldPt.x = guardCenter.x + cosf(angle) * radius;
            worldPt.y = guardCenter.y + sinf(angle) * radius;
            worldPt.z = guardCenter.z; // ground level

            float sx, sy;
            ptValid[i] = config::WorldToScreen(worldPt, sx, sy);
            if (ptValid[i]) {
                screenPts[i][0] = sx;
                screenPts[i][1] = sy;
                validCount++;
            }
        }

        if (validCount < 4) return;

        // Draw dashed circle by connecting valid adjacent points
        float curtime = interfaces::globalVars->curtime;
        float dashPhase = fmodf(curtime * 2.f, 1.f);

        for (int i = 0; i < SEGMENTS; i++) {
            int next = (i + 1) % SEGMENTS;
            if (!ptValid[i] || !ptValid[next]) continue;

            // Dashed effect: skip every other pair based on index + time
            float dashIdx = (float)i / (float)SEGMENTS + dashPhase;
            int dashInt = (int)(dashIdx * SEGMENTS);
            if (dashInt % 3 == 0) continue; // skip every third segment for dashes

            dl->AddLine(
                ImVec2(screenPts[i][0], screenPts[i][1]),
                ImVec2(screenPts[next][0], screenPts[next][1]),
                circleCol, 1.5f);
        }

        // Label at center
        float cx, cy;
        if (config::WorldToScreen(guardCenter, cx, cy)) {
            const char* label = onPost ? "GUARD POST" : "RETURN TO POST";
            ImVec2 sz = font->CalcTextSizeA(fontSize * 0.75f, FLT_MAX, 0.f, label);
            DrawTextShadow(dl, font, fontSize * 0.75f,
                           ImVec2(cx - sz.x * 0.5f, cy - sz.y * 0.5f),
                           circleCol, label);
        }
    }

    // ========================================================================
    // 5. PATROL PATH LINES
    // ========================================================================
    inline void DrawPatrolPath(ImDrawList* dl, ImFont* font, float fontSize,
                               int screenW, int screenH) {
        if (bot_tasks::currentMode != bot_tasks::BotMode::Patrol) return; // Only in Patrol mode
        if (bot_tasks::patrolCount <= 0) return;

        float curtime = interfaces::globalVars->curtime;
        int currentWP = bot_tasks::patrolIdx;

        int localIdx = interfaces::engine->GetLocalPlayer();
        Vector ownPos{};
        bool haveOwnPos = false;
        if (localIdx >= 0 && localIdx < 128) {
            auto& bones = config::BoneRead();
            if (bones[localIdx].valid) {
                ownPos = bones[localIdx].absOrigin;
                haveOwnPos = true;
            }
        }

        // Draw lines between consecutive waypoints
        float prevSx = 0.f, prevSy = 0.f;
        bool prevValid = false;

        for (int i = 0; i < bot_tasks::patrolCount && i < 64; i++) {
            Vector wp = bot_tasks::patrolPoints[i];
            float sx, sy;
            bool onScreen = config::WorldToScreen(wp, sx, sy);

            if (onScreen && prevValid) {
                dl->AddLine(ImVec2(prevSx, prevSy), ImVec2(sx, sy),
                            COL_PATROL_LINE, 1.5f);
            }

            if (onScreen) {
                if (i < currentWP) {
                    // Completed waypoint: filled dot
                    dl->AddCircleFilled(ImVec2(sx, sy), 4.f, COL_PATROL_DONE, 8);
                } else if (i == currentWP) {
                    // Current waypoint: pulsing circle
                    float pulse = sinf(curtime * 4.f) * 0.3f + 0.7f;
                    float pulseRadius = 6.f + pulse * 4.f;
                    dl->AddCircle(ImVec2(sx, sy), pulseRadius,
                                  COL_PATROL_ACTIVE, 12, 2.f);
                    dl->AddCircleFilled(ImVec2(sx, sy), 4.f, COL_PATROL_ACTIVE, 8);

                    // Label
                    char wpBuf[16];
                    snprintf(wpBuf, sizeof(wpBuf), "WP %d", i + 1);
                    ImVec2 wpSz = font->CalcTextSizeA(fontSize * 0.7f, FLT_MAX, 0.f, wpBuf);
                    DrawTextShadow(dl, font, fontSize * 0.7f,
                                   ImVec2(sx - wpSz.x * 0.5f, sy + pulseRadius + 3.f),
                                   COL_PATROL_ACTIVE, wpBuf);
                } else {
                    // Upcoming waypoint: hollow dot
                    dl->AddCircle(ImVec2(sx, sy), 4.f, COL_PATROL_PEND, 8, 1.5f);
                }
            }

            if (onScreen) {
                prevSx = sx;
                prevSy = sy;
                prevValid = true;
            } else {
                prevValid = false;
            }
        }

        // Close the patrol loop: line from last to first
        if (bot_tasks::patrolCount > 2) {
            float firstSx, firstSy, lastSx, lastSy;
            bool firstOk = config::WorldToScreen(bot_tasks::patrolPoints[0], firstSx, firstSy);
            bool lastOk  = config::WorldToScreen(
                bot_tasks::patrolPoints[bot_tasks::patrolCount - 1], lastSx, lastSy);
            if (firstOk && lastOk) {
                dl->AddLine(ImVec2(lastSx, lastSy), ImVec2(firstSx, firstSy),
                            COL_PATROL_LINE, 1.0f);
            }
        }

        // Line from our position to next waypoint
        if (haveOwnPos && currentWP >= 0 && currentWP < bot_tasks::patrolCount) {
            Vector nextWP = bot_tasks::patrolPoints[currentWP];
            float osx, osy, nsx, nsy;
            Vector ownEye = ownPos;
            ownEye.z += 36.f;
            if (config::WorldToScreen(ownEye, osx, osy) &&
                config::WorldToScreen(nextWP, nsx, nsy)) {
                // Animated dashed line
                float dx = nsx - osx;
                float dy = nsy - osy;
                float lineLen = sqrtf(dx * dx + dy * dy);
                if (lineLen > 3.f) {
                    float dirX = dx / lineLen;
                    float dirY = dy / lineLen;
                    float phase = fmodf(curtime * 40.f, 13.f); // animated offset
                    float drawn = phase;
                    bool on = true;
                    while (drawn < lineLen) {
                        float seg = on ? 8.f : 5.f;
                        if (drawn + seg > lineLen) seg = lineLen - drawn;
                        if (on && drawn >= 0.f) {
                            dl->AddLine(
                                ImVec2(osx + dirX * drawn, osy + dirY * drawn),
                                ImVec2(osx + dirX * (drawn + seg), osy + dirY * (drawn + seg)),
                                COL_PATROL_ACTIVE, 1.5f);
                        }
                        drawn += seg;
                        on = !on;
                    }
                }
            }
        }
    }

    // ========================================================================
    // 6. FARM TARGET MARKERS
    // ========================================================================
    inline void DrawFarmMarkers(ImDrawList* dl, ImFont* font, float fontSize,
                                int screenW, int screenH) {
        if (bot_tasks::currentMode != bot_tasks::BotMode::Farm) return; // Only in Farm mode

        float curtime = interfaces::globalVars->curtime;

        // Scan bone cache for entities of interest (money, printers, weapons)
        // We use config::BoneRead to identify nearby valuable targets
        auto& bones = config::BoneRead();
        int localIdx = interfaces::engine->GetLocalPlayer();
        if (localIdx < 0 || localIdx >= 128 || !bones[localIdx].valid) return;

        Vector ownPos = bones[localIdx].absOrigin;
        float nearestDist = 999999.f;
        int nearestIdx = -1;

        // Iterate entities and draw markers for potential farm targets
        for (int i = 1; i < 128; i++) {
            if (i == localIdx) continue;
            const auto& rec = bones[i];
            if (!rec.valid || rec.dormant) continue;

            // Check if entity has lootable characteristics (money or weapons)
            bool isMoney = rec.money > 0;
            bool hasWeapon = rec.weapon[0] != '\0';

            if (!isMoney && !hasWeapon) continue;

            float sx, sy;
            Vector entPos = rec.absOrigin;
            entPos.z += 48.f;
            if (!config::WorldToScreen(entPos, sx, sy)) continue;

            float dx = rec.absOrigin.x - ownPos.x;
            float dy = rec.absOrigin.y - ownPos.y;
            float dist = sqrtf(dx * dx + dy * dy);

            // Track nearest
            if (dist < nearestDist) {
                nearestDist = dist;
                nearestIdx = i;
            }

            // Marker icon
            ImU32 markerCol = isMoney ? COL_FARM_MONEY : COL_FARM_WEAPON;
            const char* icon = isMoney ? "$" : "*";

            // Pulsing glow on nearest target
            if (i == nearestIdx) {
                float pulse = sinf(curtime * 5.f) * 0.3f + 0.7f;
                float glowRadius = 14.f + pulse * 6.f;
                dl->AddCircleFilled(ImVec2(sx, sy), glowRadius,
                                    WithAlpha(markerCol, (int)(40.f * pulse)), 12);
                dl->AddCircle(ImVec2(sx, sy), glowRadius,
                              WithAlpha(markerCol, (int)(100.f * pulse)), 12, 1.5f);
            }

            // Icon text
            ImVec2 iconSz = font->CalcTextSizeA(fontSize * 1.2f, FLT_MAX, 0.f, icon);
            DrawTextShadow(dl, font, fontSize * 1.2f,
                           ImVec2(sx - iconSz.x * 0.5f, sy - iconSz.y * 0.5f),
                           markerCol, icon);

            // Distance text below
            char distBuf[24];
            snprintf(distBuf, sizeof(distBuf), "%.0fm", dist / 52.49f);
            ImVec2 distSz = font->CalcTextSizeA(fontSize * 0.65f, FLT_MAX, 0.f, distBuf);
            DrawTextShadow(dl, font, fontSize * 0.65f,
                           ImVec2(sx - distSz.x * 0.5f, sy + iconSz.y * 0.5f + 2.f),
                           COL_TEXT_DIM, distBuf);
        }

        // Second pass: re-draw glow on the actual nearest
        if (nearestIdx > 0 && nearestIdx < 128) {
            const auto& rec = bones[nearestIdx];
            float sx, sy;
            Vector entPos = rec.absOrigin;
            entPos.z += 48.f;
            if (config::WorldToScreen(entPos, sx, sy)) {
                float pulse = sinf(curtime * 5.f) * 0.3f + 0.7f;
                float glowRadius = 14.f + pulse * 6.f;
                ImU32 markerCol = rec.money > 0 ? COL_FARM_MONEY : COL_FARM_WEAPON;
                dl->AddCircle(ImVec2(sx, sy), glowRadius,
                              WithAlpha(markerCol, (int)(160.f * pulse)), 12, 2.f);
            }
        }
    }

    // ========================================================================
    // 7. MINIMAP
    // ========================================================================
    inline void DrawMinimap(ImDrawList* dl, ImFont* font, float fontSize,
                            int screenW, int screenH) {
        if (!drawMinimap) return;
        if (!follow_bot::enabled) return;

        const float mapSize  = 180.f;
        const float mapX     = (float)screenW - mapSize - 20.f;
        const float mapY     = (float)screenH - mapSize - 20.f;
        const float halfMap  = mapSize * 0.5f;
        const float cx       = mapX + halfMap;
        const float cy       = mapY + halfMap;
        const float scale    = 1.f / 10.f; // 1 pixel = 10 source units
        const float maxRadPx = halfMap - 8.f;

        int localIdx = interfaces::engine->GetLocalPlayer();
        if (localIdx < 0 || localIdx >= 128) return;
        auto& bones = config::BoneRead();
        if (!bones[localIdx].valid) return;

        Vector ownPos = bones[localIdx].absOrigin;

        // Get camera yaw for rotation
        int ri = config::g_viewReadIdx.load(std::memory_order_acquire);
        const auto& vm = config::g_viewMatrix[ri];
        float localYaw = atan2f(vm[0][1], vm[0][0]);
        float cosY = cosf(-localYaw);
        float sinY = sinf(-localYaw);

        // Background
        dl->AddRectFilled(ImVec2(mapX, mapY), ImVec2(mapX + mapSize, mapY + mapSize),
                          COL_MINIMAP_BG, 4.f);
        dl->AddRect(ImVec2(mapX, mapY), ImVec2(mapX + mapSize, mapY + mapSize),
                    COL_MINIMAP_RING, 4.f, 0, 1.5f);

        // Compass ring marks (N, E, S, W)
        {
            const char* dirs[] = {"N", "E", "S", "W"};
            float angles[] = {M_PI_F * 0.5f, 0.f, -M_PI_F * 0.5f, M_PI_F};
            for (int d = 0; d < 4; d++) {
                float worldAngle = angles[d];
                // Rotate by camera yaw
                float rx = cosf(worldAngle) * cosY - sinf(worldAngle) * sinY;
                float ry = cosf(worldAngle) * sinY + sinf(worldAngle) * cosY;
                float px = cx + rx * (halfMap - 3.f);
                float py = cy - ry * (halfMap - 3.f);
                ImVec2 sz = font->CalcTextSizeA(fontSize * 0.6f, FLT_MAX, 0.f, dirs[d]);
                ImU32 dirCol = (d == 0) ? IM_COL32(255, 60, 60, 220) : IM_COL32(180, 180, 190, 160);
                dl->AddText(font, fontSize * 0.6f,
                            ImVec2(px - sz.x * 0.5f, py - sz.y * 0.5f),
                            dirCol, dirs[d]);
            }
        }

        // Crosshair at center
        dl->AddLine(ImVec2(cx - 6.f, cy), ImVec2(cx + 6.f, cy), IM_COL32(255, 255, 255, 40));
        dl->AddLine(ImVec2(cx, cy - 6.f), ImVec2(cx, cy + 6.f), IM_COL32(255, 255, 255, 40));

        // Helper: world pos to minimap pos
        auto worldToMinimap = [&](Vector world, float& outX, float& outY) {
            float dx = world.x - ownPos.x;
            float dy = world.y - ownPos.y;
            float rx = dx * cosY - dy * sinY;
            float ry = dx * sinY + dy * cosY;
            rx *= scale;
            ry *= scale;
            // Clamp to map bounds
            float dist = sqrtf(rx * rx + ry * ry);
            if (dist > maxRadPx) {
                float f = maxRadPx / dist;
                rx *= f;
                ry *= f;
            }
            outX = cx + rx;
            outY = cy - ry;
        };

        // Local player dot (center, white)
        dl->AddCircleFilled(ImVec2(cx, cy), 3.5f, IM_COL32(255, 255, 255, 255), 8);

        // Target dot (cyan)
        if (follow_bot::targetIdx >= 0 && follow_bot::targetIdx < 128 &&
            bones[follow_bot::targetIdx].valid) {
            float tx, ty;
            worldToMinimap(bones[follow_bot::targetIdx].absOrigin, tx, ty);
            dl->AddCircleFilled(ImVec2(tx, ty), 3.f, COL_ACCENT, 8);
        }

        // Threat dots (red)
        if (bot_combat::enabled) {
            int maxT = bot_combat::g_threatCount;
            if (maxT > 16) maxT = 16;
            for (int i = 0; i < maxT; i++) {
                const auto& threat = bot_combat::g_threats[i];
                if (threat.idx <= 0) continue;
                float tx, ty;
                worldToMinimap(threat.pos, tx, ty);
                float dotSize = 2.f + threat.danger * 2.f;
                dl->AddCircleFilled(ImVec2(tx, ty), dotSize, COL_THREAT_HIGH, 6);
            }
        }

        // Patrol waypoints (blue dots)
        if (bot_tasks::currentMode == bot_tasks::BotMode::Patrol && bot_tasks::patrolCount > 0) {
            for (int i = 0; i < bot_tasks::patrolCount && i < 64; i++) {
                float px, py;
                worldToMinimap(bot_tasks::patrolPoints[i], px, py);
                ImU32 wpCol = (i == bot_tasks::patrolIdx) ? COL_PATROL_ACTIVE : COL_PATROL_PEND;
                dl->AddCircleFilled(ImVec2(px, py), 2.5f, wpCol, 6);
            }
        }

        // Guard position (green ring)
        if (bot_tasks::currentMode == bot_tasks::BotMode::Guard) {
            float gx, gy;
            worldToMinimap(bot_tasks::guardPos, gx, gy);
            float guardRadPx = bot_tasks::guardRadius * scale;
            if (guardRadPx > maxRadPx) guardRadPx = maxRadPx;
            if (guardRadPx > 3.f)
                dl->AddCircle(ImVec2(gx, gy), guardRadPx, COL_GUARD_OK, 16, 1.f);
            dl->AddCircleFilled(ImVec2(gx, gy), 2.5f, COL_GUARD_OK, 6);
        }

        // Nav path overlay
        if (follow_bot::g_useNavPath && !follow_bot::g_navPath.empty()) {
            float prevMx = 0.f, prevMy = 0.f;
            bool hasPrev = false;
            for (int i = follow_bot::g_navPathIdx; i < (int)follow_bot::g_navPath.size() && i < 128; i++) {
                float mx, my;
                worldToMinimap(follow_bot::g_navPath[i], mx, my);
                if (hasPrev) {
                    dl->AddLine(ImVec2(prevMx, prevMy), ImVec2(mx, my),
                                IM_COL32(0, 200, 255, 80), 1.f);
                }
                prevMx = mx;
                prevMy = my;
                hasPrev = true;
            }
        }

        // Label
        const char* label = "MINIMAP";
        ImVec2 labelSz = font->CalcTextSizeA(fontSize * 0.7f, FLT_MAX, 0.f, label);
        dl->AddText(font, fontSize * 0.7f,
                    ImVec2(cx - labelSz.x * 0.5f, mapY - labelSz.y - 3.f),
                    IM_COL32(0, 180, 216, 180), label);
    }

    // ========================================================================
    // MAIN DRAW FUNCTION
    // ========================================================================
    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize,
                     int screenW, int screenH) {
        if (!follow_bot::enabled) return;
        if (config::g_panic.load(std::memory_order_relaxed)) return;

        DrawStatusHUD(dl, font, fontSize, screenW, screenH);
        DrawEnhancedPath(dl, font, fontSize, screenW, screenH);
        DrawThreatIndicators(dl, font, fontSize, screenW, screenH);
        DrawGuardRadius(dl, font, fontSize, screenW, screenH);
        DrawPatrolPath(dl, font, fontSize, screenW, screenH);
        DrawFarmMarkers(dl, font, fontSize, screenW, screenH);
        DrawMinimap(dl, font, fontSize, screenW, screenH);
    }

    // ========================================================================
    // RESET
    // ========================================================================
    inline void Reset() {
        drawHUD     = true;
        drawPath    = true;
        drawThreats = true;
        drawMinimap = false;
        hudScale    = 1.0f;
        hudPos      = {10.f, 200.f};
    }

} // namespace bot_visuals
