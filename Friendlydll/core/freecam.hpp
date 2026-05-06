#pragma once
#include "../includes.hpp"

namespace freecam {

    inline bool enabled = false;
    inline int toggle_key = VK_F3;
    inline float speed = 500.f;
    inline bool g_active = false;
    inline bool g_installed = false;

    inline void Install() {
        if (g_installed) return;

        bool ok = lualoader::Execute(R"lua(
            pcall(function()
                _fdll_fc_pos = LocalPlayer():EyePos()
                _fdll_fc_speed = 500

                hook.Add("CalcView", "_fdll_fc", function(ply, pos, angles, fov)
                    _fdll_fc_ang = angles
                    return {origin = _fdll_fc_pos, angles = angles, fov = fov}
                end)

                hook.Add("Think", "_fdll_fc_move", function()
                    if not _fdll_fc_ang then return end
                    local sp = _fdll_fc_speed * FrameTime()
                    if input.IsKeyDown(KEY_LSHIFT) then sp = sp * 3 end
                    local fw = _fdll_fc_ang:Forward() * sp
                    local rt = _fdll_fc_ang:Right() * sp

                    if input.IsKeyDown(KEY_W) then _fdll_fc_pos = _fdll_fc_pos + fw end
                    if input.IsKeyDown(KEY_S) then _fdll_fc_pos = _fdll_fc_pos - fw end
                    if input.IsKeyDown(KEY_D) then _fdll_fc_pos = _fdll_fc_pos + rt end
                    if input.IsKeyDown(KEY_A) then _fdll_fc_pos = _fdll_fc_pos - rt end
                    if input.IsKeyDown(KEY_SPACE) then _fdll_fc_pos = _fdll_fc_pos + Vector(0,0,sp) end
                    if input.IsKeyDown(KEY_LCONTROL) then _fdll_fc_pos = _fdll_fc_pos - Vector(0,0,sp) end
                end)

                hook.Add("ShouldDrawLocalPlayer", "_fdll_fc_body", function()
                    return true
                end)

                hook.Add("InputMouseApply", "_fdll_fc_nomouse", function(cmd, x, y, ang)
                    return true
                end)
            end)
        )lua");
        if (ok) g_installed = true;
    }

    inline void Uninstall() {
        if (!g_installed) return;

        lualoader::Execute(R"lua(
            pcall(function()
                hook.Remove("CalcView", "_fdll_fc")
                hook.Remove("Think", "_fdll_fc_move")
                hook.Remove("ShouldDrawLocalPlayer", "_fdll_fc_body")
                hook.Remove("InputMouseApply", "_fdll_fc_nomouse")
                _fdll_fc_pos = nil
                _fdll_fc_ang = nil
            end)
        )lua");
        g_installed = false;
    }

    inline void UpdateSpeed() {
        if (!g_installed) return;
        auto cmd = std::format(
            "pcall(function() _fdll_fc_speed = {} end)",
            static_cast<int>(speed));
        lualoader::Execute(cmd);
    }

    inline void CheckToggle() {
        if (!enabled) {
            if (g_active) { Uninstall(); g_active = false; }
            return;
        }

        if (GetAsyncKeyState(toggle_key) & 1) {
            g_active = !g_active;
            if (g_active)
                Install();
            else
                Uninstall();
        }
    }

    inline void DrawIndicator(ImDrawList* dl, ImFont* font, float fontSize,
                              int screenW, int screenH) {
        if (!g_active) return;

        const char* text = "FREECAM";
        float titleSize = fontSize * 1.6f;
        ImVec2 sz = font->CalcTextSizeA(titleSize, FLT_MAX, 0.f, text);
        float tx = (static_cast<float>(screenW) - sz.x) * 0.5f;
        float ty = static_cast<float>(screenH) - 80.f;

        float pulse = sinf(static_cast<float>(ImGui::GetTime()) * 3.f) * 0.2f + 0.8f;
        int alpha = static_cast<int>(pulse * 255.f);

        dl->AddRectFilled(ImVec2(tx - 10.f, ty - 4.f), ImVec2(tx + sz.x + 10.f, ty + sz.y + 4.f),
                          IM_COL32(0, 0, 0, 160), 4.f);
        dl->AddText(font, titleSize, ImVec2(tx + 1.f, ty + 1.f), IM_COL32(0, 0, 0, 200), text);
        dl->AddText(font, titleSize, ImVec2(tx, ty), IM_COL32(0, 200, 255, alpha), text);

        // Build hint with dynamic key name
        auto VKToName = [](int vk) -> const char* {
            switch (vk) {
                case VK_F1: return "F1"; case VK_F2: return "F2"; case VK_F3: return "F3";
                case VK_F4: return "F4"; case VK_F5: return "F5"; case VK_F6: return "F6";
                case VK_F7: return "F7"; case VK_F8: return "F8"; case VK_F9: return "F9";
                case VK_F10: return "F10"; case VK_F11: return "F11"; case VK_F12: return "F12";
                case VK_INSERT: return "INS"; case VK_DELETE: return "DEL";
                case VK_HOME: return "HOME"; case VK_END: return "END";
                case VK_PRIOR: return "PGUP"; case VK_NEXT: return "PGDN";
                case VK_ESCAPE: return "ESC"; case VK_TAB: return "TAB";
                case VK_CAPITAL: return "CAPS"; case VK_NUMLOCK: return "NUMLOCK";
                case VK_PAUSE: return "PAUSE"; case VK_SCROLL: return "SCRLOCK";
                default: return nullptr;
            }
        };
        char hintBuf[64];
        const char* keyName = VKToName(toggle_key);
        if (keyName)
            snprintf(hintBuf, sizeof(hintBuf), "WASD+Space/Ctrl | Shift=Fast | %s=Exit", keyName);
        else
            snprintf(hintBuf, sizeof(hintBuf), "WASD+Space/Ctrl | Shift=Fast | 0x%02X=Exit", toggle_key);
        ImVec2 hs = font->CalcTextSizeA(fontSize * 0.8f, FLT_MAX, 0.f, hintBuf);
        float hx = (static_cast<float>(screenW) - hs.x) * 0.5f;
        dl->AddText(font, fontSize * 0.8f, ImVec2(hx, ty + sz.y + 6.f),
                    IM_COL32(180, 180, 180, 160), hintBuf);
    }

} // namespace freecam
