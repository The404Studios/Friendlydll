#pragma once

namespace luascripts {

	// -----------------------------------------------------------------------
	// 16. Admin Spoof -- make client-side checks think you're admin
	// -----------------------------------------------------------------------
	inline const char* LUA_ADMIN_SPOOF_SETUP = R"lua(
pcall(function()
    if _fdll_admin_spoof_installed then return end
    _fdll_admin_spoof_installed = true
    _fdll_admin_originals = _fdll_admin_originals or {}

    local meta = FindMetaTable("Player")
    if not meta then return end

    -- Override IsAdmin
    pcall(function()
        if meta.IsAdmin then
            _fdll_admin_originals["IsAdmin"] = meta.IsAdmin
            meta.IsAdmin = function(self)
                if self == LocalPlayer() then return true end
                return _fdll_admin_originals["IsAdmin"](self)
            end
        end
    end)

    -- Override IsSuperAdmin
    pcall(function()
        if meta.IsSuperAdmin then
            _fdll_admin_originals["IsSuperAdmin"] = meta.IsSuperAdmin
            meta.IsSuperAdmin = function(self)
                if self == LocalPlayer() then return true end
                return _fdll_admin_originals["IsSuperAdmin"](self)
            end
        end
    end)

    -- Override GetUserGroup
    pcall(function()
        if meta.GetUserGroup then
            _fdll_admin_originals["GetUserGroup"] = meta.GetUserGroup
            meta.GetUserGroup = function(self)
                if self == LocalPlayer() then return "superadmin" end
                return _fdll_admin_originals["GetUserGroup"](self)
            end
        end
    end)

    -- Hook ULib permission checks if ULib exists
    pcall(function()
        if ULib and ULib.ucl and ULib.ucl.query then
            _fdll_admin_originals["ulx_query"] = ULib.ucl.query
            ULib.ucl.query = function(ply, perm, ...)
                if IsValid(ply) and ply == LocalPlayer() then return true end
                return _fdll_admin_originals["ulx_query"](ply, perm, ...)
            end
        end
    end)

    -- Hook SAM permission checks if SAM exists
    pcall(function()
        if sam and sam.has_permission then
            _fdll_admin_originals["sam_perm"] = sam.has_permission
            sam.has_permission = function(ply, perm, ...)
                if IsValid(ply) and ply == LocalPlayer() then return true end
                return _fdll_admin_originals["sam_perm"](ply, perm, ...)
            end
        end
    end)

    -- Hook FAdmin if it exists
    pcall(function()
        if FAdmin and FAdmin.Access and FAdmin.Access.PlayerHasPrivilege then
            _fdll_admin_originals["fadmin_priv"] = FAdmin.Access.PlayerHasPrivilege
            FAdmin.Access.PlayerHasPrivilege = function(ply, priv, ...)
                if IsValid(ply) and ply == LocalPlayer() then return true end
                return _fdll_admin_originals["fadmin_priv"](ply, priv, ...)
            end
        end
    end)

    hook.Add("HUDPaint", "fdll_admin_spoof_hud", function()
        if not _fdll_admin_spoof_installed then return end
        local alpha = math.Clamp(math.sin(CurTime() * 2) * 30 + 200, 0, 255)
        draw.SimpleText("ADMIN SPOOFED", "DermaDefault",
            ScrW() - 80, 12, Color(255, 80, 80, alpha),
            TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER)
    end)
end)
)lua";

	inline const char* LUA_ADMIN_SPOOF_STOP = R"lua(
pcall(function()
    _fdll_admin_spoof_installed = false

    local meta = FindMetaTable("Player")
    if meta and _fdll_admin_originals then
        local restore = {"IsAdmin", "IsSuperAdmin", "GetUserGroup"}
        for _, fn in ipairs(restore) do
            pcall(function()
                if _fdll_admin_originals[fn] then
                    meta[fn] = _fdll_admin_originals[fn]
                end
            end)
        end
    end

    pcall(function()
        if ULib and ULib.ucl and _fdll_admin_originals and _fdll_admin_originals["ulx_query"] then
            ULib.ucl.query = _fdll_admin_originals["ulx_query"]
        end
    end)

    pcall(function()
        if sam and _fdll_admin_originals and _fdll_admin_originals["sam_perm"] then
            sam.has_permission = _fdll_admin_originals["sam_perm"]
        end
    end)

    pcall(function()
        if FAdmin and FAdmin.Access and _fdll_admin_originals and _fdll_admin_originals["fadmin_priv"] then
            FAdmin.Access.PlayerHasPrivilege = _fdll_admin_originals["fadmin_priv"]
        end
    end)

    _fdll_admin_originals = nil
    hook.Remove("HUDPaint", "fdll_admin_spoof_hud")
end)
)lua";

	// -----------------------------------------------------------------------
	// 17. Admin Menu Unlock -- try to open restricted admin panels
	// -----------------------------------------------------------------------
	inline const char* LUA_ADMIN_MENU_UNLOCK = R"lua(
local r = ""
pcall(function()
    r = "=== ADMIN MENU UNLOCK ===\n"
    local attempts = {}

    -- ULX
    pcall(function()
        if ulx then
            RunConsoleCommand("xgui", "open")
            attempts[#attempts + 1] = "ULX XGUI: SENT"
        else
            attempts[#attempts + 1] = "ULX: NOT INSTALLED"
        end
    end)

    -- SAM
    pcall(function()
        if sam then
            RunConsoleCommand("sam_menu")
            attempts[#attempts + 1] = "SAM Menu: SENT"
        else
            attempts[#attempts + 1] = "SAM: NOT INSTALLED"
        end
    end)

    -- FAdmin
    pcall(function()
        if FAdmin then
            RunConsoleCommand("fadmin")
            attempts[#attempts + 1] = "FAdmin: SENT"
        else
            attempts[#attempts + 1] = "FAdmin: NOT INSTALLED"
        end
    end)

    -- Generic admin panels
    pcall(function()
        RunConsoleCommand("ulx", "menu")
        attempts[#attempts + 1] = "ulx menu: SENT"
    end)

    -- DarkRP admin tab
    pcall(function()
        if DarkRP and DarkRP.openF4Menu then
            DarkRP.openF4Menu()
            attempts[#attempts + 1] = "DarkRP F4: OPENED"
        end
    end)

    -- Serverguard
    pcall(function()
        if serverguard then
            RunConsoleCommand("serverguard_menu")
            attempts[#attempts + 1] = "ServerGuard: SENT"
        else
            attempts[#attempts + 1] = "ServerGuard: NOT INSTALLED"
        end
    end)

    for _, a in ipairs(attempts) do
        r = r .. "  " .. a .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 18. Admin Command Exploit -- test admin commands for validation gaps
	// -----------------------------------------------------------------------
	inline const char* LUA_ADMIN_CMD_EXPLOIT = R"lua(
local r = ""
pcall(function()
    r = "=== ADMIN COMMAND SCAN ===\n"

    -- Find admin-related console commands
    local adminCmds = {}
    pcall(function()
        local allCmds = concommand.GetTable()
        if allCmds then
            local keywords = {"admin", "ulx", "sam", "fadmin", "ban", "kick",
                              "god", "noclip", "slay", "jail", "mute", "gag",
                              "setmoney", "setjob", "give", "tp", "teleport",
                              "setgroup", "setrank"}
            for name, _ in pairs(allCmds) do
                local lower = string.lower(name)
                for _, kw in ipairs(keywords) do
                    if string.find(lower, kw) then
                        adminCmds[#adminCmds + 1] = name
                        break
                    end
                end
            end
        end
    end)

    table.sort(adminCmds)
    r = r .. "ADMIN COMMANDS FOUND: " .. #adminCmds .. "\n"
    for i, cmd in ipairs(adminCmds) do
        if i > 50 then
            r = r .. "  ... and " .. (#adminCmds - 50) .. " more\n"
            break
        end
        r = r .. "  " .. cmd .. "\n"
    end

    -- Check current admin status
    r = r .. "\nCURRENT STATUS:\n"
    pcall(function()
        local lp = LocalPlayer()
        if IsValid(lp) then
            r = r .. "  IsAdmin: " .. tostring(lp:IsAdmin()) .. "\n"
            r = r .. "  IsSuperAdmin: " .. tostring(lp:IsSuperAdmin()) .. "\n"
            r = r .. "  UserGroup: " .. tostring(lp:GetUserGroup()) .. "\n"
        end
    end)

    -- Check which admin mods are installed
    r = r .. "\nADMIN MODS:\n"
    local mods = {
        {"ULX/ULib", function() return ULib ~= nil end},
        {"SAM", function() return sam ~= nil end},
        {"FAdmin", function() return FAdmin ~= nil end},
        {"Evolve", function() return evolve ~= nil end},
        {"ServerGuard", function() return serverguard ~= nil end},
        {"xAdmin", function() return xAdmin ~= nil end},
        {"sAdmin", function() return sAdmin ~= nil end},
    }
    for _, mod in ipairs(mods) do
        local installed = false
        pcall(function() installed = mod[2]() end)
        r = r .. "  " .. mod[1] .. ": " .. (installed and "INSTALLED" or "not found") .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 19. Privilege Scanner -- deep scan of admin permission system
	// -----------------------------------------------------------------------
	inline const char* LUA_PRIVILEGE_SCANNER = R"lua(
local r = ""
pcall(function()
    r = "=== PRIVILEGE INTELLIGENCE ===\n"

    -- List all admins on server
    r = r .. "ADMINS ONLINE:\n"
    pcall(function()
        for _, ply in ipairs(player.GetAll()) do
            pcall(function()
                local group = ply:GetUserGroup()
                if group and group ~= "user" and group ~= "User" then
                    r = r .. "  " .. ply:Nick() .. " [" .. group .. "]"
                    if ply:IsAdmin() then r = r .. " (admin)" end
                    if ply:IsSuperAdmin() then r = r .. " (superadmin)" end
                    r = r .. "\n"
                end
            end)
        end
    end)

    -- ULib groups
    pcall(function()
        if ULib and ULib.ucl and ULib.ucl.groups then
            r = r .. "\nULIB GROUPS:\n"
            for name, data in pairs(ULib.ucl.groups) do
                local perms = 0
                if data.allow then
                    for _ in pairs(data.allow) do perms = perms + 1 end
                end
                r = r .. "  " .. name .. " (" .. perms .. " perms)"
                if data.inherit_from then
                    r = r .. " inherits: " .. tostring(data.inherit_from)
                end
                r = r .. "\n"
            end
        end
    end)

    -- SAM ranks
    pcall(function()
        if sam and sam.ranks then
            r = r .. "\nSAM RANKS:\n"
            local ranks = sam.ranks.GetAll and sam.ranks.GetAll() or {}
            for name, data in pairs(ranks) do
                r = r .. "  " .. tostring(name) .. "\n"
            end
        end
    end)

    -- Check for common admin-only hooks
    r = r .. "\nADMIN HOOKS:\n"
    pcall(function()
        local hookTable = hook.GetTable()
        if hookTable then
            local adminHookNames = {"PlayerSay", "CanTool", "CanProperty",
                                     "PhysgunPickup", "GravGunPickupAllowed"}
            for _, hname in ipairs(adminHookNames) do
                if hookTable[hname] then
                    local count = 0
                    for _ in pairs(hookTable[hname]) do count = count + 1 end
                    r = r .. "  " .. hname .. ": " .. count .. " hooks\n"
                end
            end
        end
    end)

    -- Check for client-side admin checks (potential bypasses)
    r = r .. "\nBYPASS VECTORS:\n"
    pcall(function()
        local lp = LocalPlayer()
        if not IsValid(lp) then return end

        -- Check if admin status is NW synced (client-trusting = exploitable)
        local nwAdmin = lp:GetNWBool("IsAdmin", nil)
        if nwAdmin ~= nil then
            r = r .. "  NWBool 'IsAdmin' found (client-modifiable)\n"
        end

        local nwGroup = lp:GetNWString("UserGroup", nil)
        if nwGroup and nwGroup ~= "" then
            r = r .. "  NWString 'UserGroup' = " .. nwGroup .. "\n"
        end
    end)
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 20. Fake Admin Actions -- social engineering tools
	// -----------------------------------------------------------------------
	inline const char* LUA_FAKE_ADMIN_ACTIONS = R"lua(
local r = ""
pcall(function()
    r = "Fake admin actions sent\n"

    -- Fake admin notification in local chat
    pcall(function()
        chat.AddText(
            Color(255, 0, 0), "[ADMIN] ",
            Color(255, 255, 255), "Server restart in 5 minutes. Save your progress."
        )
    end)

    -- Fake ULX-style notification
    pcall(function()
        chat.AddText(
            Color(151, 211, 255), "(ADMIN) ",
            Color(0, 242, 0), "Console",
            Color(255, 255, 255), " has started a map vote."
        )
    end)

    -- Play admin sound
    pcall(function()
        surface.PlaySound("buttons/button9.wav")
    end)

    r = r .. "Sent fake admin chat messages (local only)\n"
end)
return r
)lua";

} // namespace luascripts
