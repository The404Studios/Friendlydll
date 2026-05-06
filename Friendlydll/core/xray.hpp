#pragma once
#include "../includes.hpp"

namespace xray {

    inline bool enabled = false;
    inline int mode = 0; // 0=wireframe, 1=transparent, 2=textured transparent
    inline float wall_alpha = 0.3f;

    inline bool g_installed = false;
    inline int g_installedMode = -1;
    inline float g_installedAlpha = -1.f;

    inline void Install() {
        if (g_installed) return;

        auto cmd = std::format(R"lua(
pcall(function()
    if _fdll_xray_installed then return end
    _fdll_xray_installed = true

    local mode = {}
    local alpha = {:.2f}

    _fdll_xray_mats = _fdll_xray_mats or {{}}
    _fdll_xray_origmats = _fdll_xray_origmats or {{}}

    local wireframe = CreateMaterial("_fdll_xray_wire", "Wireframe", {{
        ["$basetexture"] = "models/debug/debugwhite",
        ["$ignorez"] = 1,
        ["$wireframe"] = 1,
    }})
    local transparent = CreateMaterial("_fdll_xray_trans", "VertexLitGeneric", {{
        ["$basetexture"] = "models/debug/debugwhite",
        ["$ignorez"] = 1,
        ["$translucent"] = 1,
        ["$alpha"] = alpha,
        ["$color2"] = "[0.3 0.5 0.8]",
    }})

    hook.Add("PreDrawOpaqueRenderables", "_fdll_xray", function(depth, sky)
        if sky then return end
        if mode == 0 then
            render.MaterialOverride(wireframe)
        elseif mode == 1 then
            render.MaterialOverride(transparent)
        elseif mode == 2 then
            render.MaterialOverride(nil)
        end
    end)

    hook.Add("PostDrawOpaqueRenderables", "_fdll_xray_post", function()
        render.MaterialOverride(nil)
    end)

    hook.Add("PreDrawTranslucentRenderables", "_fdll_xray_trans", function(depth, sky)
        if sky then return end
        if mode == 1 or mode == 2 then
            render.MaterialOverride(transparent)
        end
    end)

    hook.Add("PostDrawTranslucentRenderables", "_fdll_xray_post2", function()
        render.MaterialOverride(nil)
    end)
end)
)lua", mode, wall_alpha);
        if (lualoader::Execute(cmd)) {
            g_installed = true;
            g_installedMode = mode;
            g_installedAlpha = wall_alpha;
        }
    }

    inline void Uninstall() {
        if (!g_installed) return;
        g_installed = false;

        lualoader::Execute(R"lua(
pcall(function()
    hook.Remove("PreDrawOpaqueRenderables", "_fdll_xray")
    hook.Remove("PostDrawOpaqueRenderables", "_fdll_xray_post")
    hook.Remove("PreDrawTranslucentRenderables", "_fdll_xray_trans")
    hook.Remove("PostDrawTranslucentRenderables", "_fdll_xray_post2")
    render.MaterialOverride(nil)
    _fdll_xray_installed = nil
end)
)lua");
    }

    inline void Toggle() {
        if (enabled && g_installed) {
            // Reinstall if mode or wall_alpha changed since last install
            if (mode != g_installedMode || wall_alpha != g_installedAlpha) {
                Uninstall();
                Install();
            }
        }
        else if (enabled && !g_installed)
            Install();
        else if (!enabled && g_installed)
            Uninstall();
    }

} // namespace xray
