#pragma once

namespace luascripts {

	// -----------------------------------------------------------------------
	// 10. Permission Bypass Engine -- detour DarkRP restriction functions
	// -----------------------------------------------------------------------
	inline const char* LUA_PERMISSION_BYPASS_SETUP = R"lua(
pcall(function()
    if _fdll_perm_bypass_installed then return end
    _fdll_perm_bypass_installed = true
    _fdll_perm_originals = _fdll_perm_originals or {}

    local function safeHook(tbl, key, replacement)
        pcall(function()
            if tbl and tbl[key] and type(tbl[key]) == "function" then
                _fdll_perm_originals[key] = tbl[key]
                tbl[key] = replacement
            end
        end)
    end

    -- Override gamemode restriction hooks
    pcall(function()
        local gm = GAMEMODE or gmod.GetGamemode()
        if not gm then return end

        local restrictionHooks = {
            "canBuyPistol", "canBuyShipment", "canBuyAmmo",
            "canBuyCustomEntity", "canDropWeapon", "canPocket",
            "canDemote", "canChangeJob", "canBuyVehicle",
            "canBuyFood", "PlayerCanPickupWeapon", "PlayerCanPickupItem",
            "canRequestHit", "canUnownAllDoors"
        }

        for _, hookName in ipairs(restrictionHooks) do
            pcall(function()
                if gm[hookName] then
                    _fdll_perm_originals["gm_" .. hookName] = gm[hookName]
                    gm[hookName] = function(self, ply, ...)
                        if IsValid(ply) and ply == LocalPlayer() then
                            return true
                        end
                        local orig = _fdll_perm_originals["gm_" .. hookName]
                        if orig then return orig(self, ply, ...) end
                        return true
                    end
                end
            end)
        end
    end)

    -- Override DarkRP module functions
    pcall(function()
        if DarkRP then
            local drpFuncs = {
                "canBuyPistol", "canBuyShipment", "canBuyAmmo",
                "canBuyCustomEntity", "canBuyVehicle"
            }
            for _, fn in ipairs(drpFuncs) do
                pcall(function()
                    if DarkRP[fn] then
                        _fdll_perm_originals["drp_" .. fn] = DarkRP[fn]
                        DarkRP[fn] = function(ply, ...)
                            if IsValid(ply) and ply == LocalPlayer() then
                                return true
                            end
                            local orig = _fdll_perm_originals["drp_" .. fn]
                            if orig then return orig(ply, ...) end
                            return true
                        end
                    end
                end)
            end
        end
    end)

    -- Bypass job player limits by patching RPExtraTeams
    pcall(function()
        if RPExtraTeams then
            _fdll_perm_originals["job_maxes"] = {}
            for id, jdata in pairs(RPExtraTeams) do
                if jdata.max then
                    _fdll_perm_originals["job_maxes"][id] = jdata.max
                    jdata.max = 999
                end
                if jdata.maxPlayers then
                    jdata.maxPlayers = 999
                end
                if jdata.Max then
                    jdata.Max = 999
                end
            end
        end
    end)

    -- Override prop limit checks
    pcall(function()
        local lp = LocalPlayer()
        if IsValid(lp) then
            local origCheckLimit = lp.CheckLimit
            if origCheckLimit then
                _fdll_perm_originals["CheckLimit"] = origCheckLimit
                local meta = FindMetaTable("Player")
                if meta then
                    meta.CheckLimit = function(self, limitType)
                        if self == LocalPlayer() then return true end
                        if _fdll_perm_originals["CheckLimit"] then
                            return _fdll_perm_originals["CheckLimit"](self, limitType)
                        end
                        return true
                    end
                end
            end
        end
    end)

    -- Hook to bypass custom addon permission systems
    hook.Add("Think", "fdll_perm_bypass_think", function()
        if not _fdll_perm_bypass_installed then return end
        pcall(function()
            local lp = LocalPlayer()
            if not IsValid(lp) then return end
            -- Keep NWBool overrides active
            pcall(function() lp:SetNWBool("can_buy", true) end)
            pcall(function() lp:SetNWBool("has_license", true) end)
        end)
    end)

    hook.Add("HUDPaint", "fdll_perm_bypass_hud", function()
        if not _fdll_perm_bypass_installed then return end
        draw.SimpleText("PERMS BYPASSED", "DermaDefault",
            ScrW() - 110, ScrH() - 20,
            Color(0, 220, 180, 180), TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER)
    end)
end)
)lua";

	inline const char* LUA_PERMISSION_BYPASS_STOP = R"lua(
pcall(function()
    _fdll_perm_bypass_installed = false

    -- Restore gamemode hooks
    pcall(function()
        local gm = GAMEMODE or gmod.GetGamemode()
        if gm and _fdll_perm_originals then
            for key, fn in pairs(_fdll_perm_originals) do
                if type(key) == "string" and string.sub(key, 1, 3) == "gm_" then
                    local hookName = string.sub(key, 4)
                    pcall(function() gm[hookName] = fn end)
                end
            end
        end
    end)

    -- Restore DarkRP functions
    pcall(function()
        if DarkRP and _fdll_perm_originals then
            for key, fn in pairs(_fdll_perm_originals) do
                if type(key) == "string" and string.sub(key, 1, 4) == "drp_" then
                    local funcName = string.sub(key, 5)
                    pcall(function() DarkRP[funcName] = fn end)
                end
            end
        end
    end)

    -- Restore job limits
    pcall(function()
        if RPExtraTeams and _fdll_perm_originals and _fdll_perm_originals["job_maxes"] then
            for id, maxVal in pairs(_fdll_perm_originals["job_maxes"]) do
                if RPExtraTeams[id] then
                    RPExtraTeams[id].max = maxVal
                end
            end
        end
    end)

    -- Restore CheckLimit
    pcall(function()
        if _fdll_perm_originals and _fdll_perm_originals["CheckLimit"] then
            local meta = FindMetaTable("Player")
            if meta then
                meta.CheckLimit = _fdll_perm_originals["CheckLimit"]
            end
        end
    end)

    _fdll_perm_originals = nil
    hook.Remove("Think", "fdll_perm_bypass_think")
    hook.Remove("HUDPaint", "fdll_perm_bypass_hud")
end)
)lua";

	// -----------------------------------------------------------------------
	// 11. Permission Scan -- check which restrictions are active
	// -----------------------------------------------------------------------
	inline const char* LUA_PERMISSION_SCAN = R"lua(
local r = ""
pcall(function()
    local gm = GAMEMODE or gmod.GetGamemode()
    if not gm then r = "No gamemode found\n" return end

    r = "=== PERMISSION SCAN ===\n"

    local restrictionHooks = {
        "canBuyPistol", "canBuyShipment", "canBuyAmmo",
        "canBuyCustomEntity", "canDropWeapon", "canPocket",
        "canDemote", "canChangeJob", "canBuyVehicle",
        "PlayerCanPickupWeapon", "PlayerCanPickupItem",
        "canRequestHit"
    }

    r = r .. "GAMEMODE HOOKS:\n"
    for _, hookName in ipairs(restrictionHooks) do
        local status = "MISSING"
        pcall(function()
            if gm[hookName] then
                if _fdll_perm_originals and _fdll_perm_originals["gm_" .. hookName] then
                    status = "DETOURED"
                else
                    status = "ACTIVE"
                end
            end
        end)
        r = r .. "  " .. hookName .. ": " .. status .. "\n"
    end

    -- Check prop limits
    r = r .. "\nLIMITS:\n"
    pcall(function()
        local cvars = {"sbox_maxprops", "sbox_maxragdolls", "sbox_maxeffects", "sbox_maxnpcs"}
        for _, cv in ipairs(cvars) do
            local val = "?"
            pcall(function()
                local c = GetConVar(cv)
                if c then val = tostring(c:GetInt()) end
            end)
            r = r .. "  " .. cv .. " = " .. val .. "\n"
        end
    end)

    -- Check job limits
    r = r .. "\nJOB LIMITS:\n"
    pcall(function()
        if RPExtraTeams then
            local count = 0
            local unlimited = 0
            for _, jdata in pairs(RPExtraTeams) do
                count = count + 1
                local mx = tonumber(jdata.max or jdata.maxPlayers or 0) or 0
                if mx >= 999 then unlimited = unlimited + 1 end
            end
            r = r .. "  Total jobs: " .. count .. " | Unlimited: " .. unlimited .. "\n"
        end
    end)

    r = r .. "\nBYPASS STATUS: " .. (_fdll_perm_bypass_installed and "ACTIVE" or "INACTIVE") .. "\n"
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 12. Cooldown Bypass -- skip DarkRP cooldowns
	// -----------------------------------------------------------------------
	inline const char* LUA_COOLDOWN_BYPASS_SETUP = R"lua(
pcall(function()
    if _fdll_cooldown_bypass_installed then return end
    _fdll_cooldown_bypass_installed = true
    _fdll_cooldown_originals = _fdll_cooldown_originals or {}

    -- Speed up DarkRP payday timer
    pcall(function()
        if timer.Exists("DarkRP_PayDay") then
            timer.Adjust("DarkRP_PayDay", 5, 0)
        end
        if timer.Exists("payaliday") then
            timer.Adjust("payaliday", 5, 0)
        end
    end)

    -- Override cooldown tracking variables
    pcall(function()
        local lp = LocalPlayer()
        if not IsValid(lp) then return end
        if lp.getDarkRPVar then
            -- Clear any active cooldowns
            pcall(function()
                if lp.setDarkRPVar then
                    lp:setDarkRPVar("lastJobChange", 0)
                    lp:setDarkRPVar("lastDemote", 0)
                    lp:setDarkRPVar("lastHitAccepted", 0)
                end
            end)
        end
    end)

    -- Hook job change to clear cooldown immediately
    hook.Add("DarkRPVarChanged", "fdll_cooldown_bypass", function(ply, var, old, new)
        if ply ~= LocalPlayer() then return end
        pcall(function()
            if string.find(var, "cooldown") or string.find(var, "last") then
                if ply.setDarkRPVar then
                    ply:setDarkRPVar(var, 0)
                end
            end
        end)
    end)

    -- Periodically clear cooldowns
    timer.Create("fdll_cooldown_bypass", 2, 0, function()
        pcall(function()
            if not _fdll_cooldown_bypass_installed then
                timer.Remove("fdll_cooldown_bypass")
                return
            end
            local lp = LocalPlayer()
            if not IsValid(lp) then return end

            -- Try to reset known cooldown NW vars
            local cooldownVars = {
                "lastJobChange", "lastDemote", "lastHitAccepted",
                "lastArrest", "lastVote", "lastWarrant"
            }
            for _, cv in ipairs(cooldownVars) do
                pcall(function()
                    if lp.setDarkRPVar then
                        lp:setDarkRPVar(cv, 0)
                    end
                    lp:SetNWFloat(cv, 0)
                    lp:SetNWInt(cv, 0)
                end)
            end
        end)
    end)
end)
)lua";

	inline const char* LUA_COOLDOWN_BYPASS_STOP = R"lua(
pcall(function()
    _fdll_cooldown_bypass_installed = false
    _fdll_cooldown_originals = nil
    hook.Remove("DarkRPVarChanged", "fdll_cooldown_bypass")
    timer.Remove("fdll_cooldown_bypass")

    -- Restore payday timer to default
    pcall(function()
        if timer.Exists("DarkRP_PayDay") then
            timer.Adjust("DarkRP_PayDay", 120, 0)
        end
    end)
end)
)lua";

} // namespace luascripts
