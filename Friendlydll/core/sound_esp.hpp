#pragma once
#include "../includes.hpp"
#include <vector>
#include <string>
#include <sstream>

namespace sound_esp {

    struct SoundEvent {
        Vector pos;
        char name[64];
        float time;
        float fadeTime;
        bool valid;
    };

    inline std::vector<SoundEvent> g_sounds;
    inline bool g_hookInstalled = false;

    inline const char* LUA_SOUND_HOOK = R"lua(
pcall(function()
    if _fdll_soundhook then return end
    _fdll_soundhook = true
    _fdll_sounds = _fdll_sounds or {}
    hook.Add("EntityEmitSound", "_fdll_soundesp", function(data)
        if not data.Pos then return end
        table.insert(_fdll_sounds, {
            x = math.floor(data.Pos.x),
            y = math.floor(data.Pos.y),
            z = math.floor(data.Pos.z),
            n = string.sub(data.SoundName or "?", 1, 60),
            t = CurTime()
        })
        if #_fdll_sounds > 50 then table.remove(_fdll_sounds, 1) end
    end)
end)
)lua";

    inline const char* LUA_SOUND_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_sounds then return end
    local now = CurTime()
    local fresh = {}
    for i = 1, #_fdll_sounds do
        local s = _fdll_sounds[i]
        if s and (now - s.t) < 3 then
            fresh[#fresh + 1] = s
            r = r .. s.x .. "\t" .. s.y .. "\t" .. s.z .. "\t" .. s.n .. "\t" .. string.format("%.2f", s.t) .. "\n"
        end
    end
    _fdll_sounds = fresh
end)
return r
)lua";

    inline void InstallHook() {
        if (g_hookInstalled) return;
        if (!config::luastate) return;
        if (lualoader::Execute(std::string(LUA_SOUND_HOOK)))
            g_hookInstalled = true;
    }

    inline void Uninstall() {
        if (!g_hookInstalled) return;
        g_hookInstalled = false;

        lualoader::Execute(R"lua(
pcall(function()
    hook.Remove("EntityEmitSound", "_fdll_soundesp")
    _fdll_soundhook = nil
    _fdll_sounds = nil
end)
)lua");
        g_sounds.clear();
    }

    inline void Toggle() {
        if (config::sound_esp && !g_hookInstalled)
            InstallHook();
        else if (!config::sound_esp && g_hookInstalled)
            Uninstall();
    }

    inline void Update() {
        Toggle();
        if (!config::sound_esp) return;
        if (!config::luastate) return;
        InstallHook();

        auto result = lualoader::ExecuteAndGetResult(std::string(LUA_SOUND_READ));
        if (result.empty()) { g_sounds.clear(); return; }

        g_sounds.clear();
        std::istringstream ss(result);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            size_t t1 = line.find('\t');
            if (t1 == std::string::npos) continue;
            size_t t2 = line.find('\t', t1 + 1);
            if (t2 == std::string::npos) continue;
            size_t t3 = line.find('\t', t2 + 1);
            if (t3 == std::string::npos) continue;
            size_t t4 = line.find('\t', t3 + 1);
            if (t4 == std::string::npos) continue;

            SoundEvent e{};
            e.pos.x = static_cast<float>(std::atof(line.substr(0, t1).c_str()));
            e.pos.y = static_cast<float>(std::atof(line.substr(t1 + 1, t2 - t1 - 1).c_str()));
            e.pos.z = static_cast<float>(std::atof(line.substr(t2 + 1, t3 - t2 - 1).c_str()));
            strncpy_s(e.name, line.substr(t3 + 1, t4 - t3 - 1).c_str(), 63);
            e.time = static_cast<float>(std::atof(line.substr(t4 + 1).c_str()));
            e.fadeTime = 3.0f;
            e.valid = true;
            g_sounds.push_back(e);
        }
    }

    inline void Draw(ImDrawList* dl, ImFont* font, float fontSize, int screenW, int screenH) {
        if (!config::sound_esp) return;

        for (const auto& snd : g_sounds) {
            if (!snd.valid) continue;

            float sx, sy;
            if (!config::WorldToScreen(snd.pos, sx, sy)) continue;
            if (sx < -50.f || sx > screenW + 50.f || sy < -50.f || sy > screenH + 50.f) continue;

            // Sound ripple circle (fading ring)
            float radius = 12.f;
            ImU32 ringCol = IM_COL32(255, 200, 50, 150);
            dl->AddCircle(ImVec2(sx, sy), radius, ringCol, 16, 1.5f);
            dl->AddCircle(ImVec2(sx, sy), radius * 0.5f, IM_COL32(255, 200, 50, 80), 12, 1.f);

            // Label: shortened sound name
            const char* shortName = snd.name;
            // Skip path prefixes
            const char* lastSlash = strrchr(snd.name, '/');
            if (lastSlash) shortName = lastSlash + 1;
            const char* lastBackslash = strrchr(shortName, '\\');
            if (lastBackslash) shortName = lastBackslash + 1;

            ImVec2 textSz = font->CalcTextSizeA(fontSize * 0.8f, FLT_MAX, 0.f, shortName);
            float textX = sx - textSz.x * 0.5f;
            float textY = sy + radius + 2.f;
            dl->AddText(font, fontSize * 0.8f, ImVec2(textX + 1.f, textY + 1.f),
                        IM_COL32(0, 0, 0, 180), shortName);
            dl->AddText(font, fontSize * 0.8f, ImVec2(textX, textY),
                        IM_COL32(255, 220, 100, 200), shortName);
        }
    }

} // namespace sound_esp
