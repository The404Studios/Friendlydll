#pragma once
#include "../includes.hpp"

namespace luascripts {

	// -----------------------------------------------------------------------
	// 10. Hitman Intel -- query active hit target info (one-shot)
	// -----------------------------------------------------------------------
	inline const char* LUA_HITMAN_INTEL = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "NO LOCAL PLAYER" return end

    local hitTarget = nil

    -- Method 1: getDarkRPVar("hitTarget") stores the entity or entindex
    pcall(function()
        if lp.getDarkRPVar then
            local ht = lp:getDarkRPVar("hitTarget")
            if ht then
                if type(ht) == "number" then
                    hitTarget = Entity(ht)
                elseif IsValid(ht) and ht:IsPlayer() then
                    hitTarget = ht
                end
            end
        end
    end)

    -- Method 2: getDarkRPVar("hit") as entindex
    if not IsValid(hitTarget) then
        pcall(function()
            if lp.getDarkRPVar then
                local ht = lp:getDarkRPVar("hit")
                if ht and type(ht) == "number" and ht > 0 then
                    hitTarget = Entity(ht)
                end
            end
        end)
    end

    -- Method 3: check global DarkRP hit tables
    if not IsValid(hitTarget) then
        pcall(function()
            if DarkRP and DarkRP.getHitTarget then
                local ht = DarkRP.getHitTarget(lp)
                if IsValid(ht) and ht:IsPlayer() then
                    hitTarget = ht
                end
            end
        end)
    end

    -- Method 4: scan _G for common hitman tables
    if not IsValid(hitTarget) then
        pcall(function()
            local hitTables = {"HitmanHits", "HITMAN", "hitman_hits", "ActiveHits"}
            for _, tblName in ipairs(hitTables) do
                local tbl = _G[tblName]
                if type(tbl) == "table" then
                    local lpIdx = lp:EntIndex()
                    local lpSid = lp:SteamID()
                    for k, v in pairs(tbl) do
                        if k == lpIdx or k == lpSid then
                            local tgt = nil
                            if type(v) == "number" then tgt = Entity(v)
                            elseif type(v) == "table" and v.target then
                                if type(v.target) == "number" then tgt = Entity(v.target)
                                elseif IsValid(v.target) then tgt = v.target end
                            elseif IsValid(v) and v:IsPlayer() then tgt = v end
                            if IsValid(tgt) and tgt:IsPlayer() then
                                hitTarget = tgt
                                break
                            end
                        end
                    end
                    if IsValid(hitTarget) then break end
                end
            end
        end)
    end

    -- Build report for our hit target
    if IsValid(hitTarget) and hitTarget:IsPlayer() then
        local name = hitTarget:Nick() or "?"
        local hp = hitTarget:Health() or 0
        local pos = hitTarget:GetPos()
        local dist = math.floor(lp:GetPos():Distance(pos))
        local job = ""
        pcall(function()
            if hitTarget.getDarkRPVar then
                job = tostring(hitTarget:getDarkRPVar("job") or "Unknown")
            end
        end)
        if job == "" then
            pcall(function() job = team.GetName(hitTarget:Team()) or "Unknown" end)
        end
        local wep = "None"
        pcall(function()
            local w = hitTarget:GetActiveWeapon()
            if IsValid(w) then wep = w:GetClass() end
        end)
        r = r .. "TARGET: " .. name .. "\n"
        r = r .. "HP: " .. tostring(hp) .. "\n"
        r = r .. "JOB: " .. job .. "\n"
        r = r .. "DIST: " .. tostring(dist) .. "\n"
        r = r .. "WEAPON: " .. wep .. "\n"
        r = r .. "POS: " .. math.floor(pos.x) .. ", " .. math.floor(pos.y) .. ", " .. math.floor(pos.z) .. "\n"
    else
        r = "NO ACTIVE HIT\n"
    end

    -- Scan for other hitmen's targets if accessible
    pcall(function()
        local otherHits = ""
        for _, ply in ipairs(player.GetAll()) do
            if ply ~= lp and IsValid(ply) then
                local theirTarget = nil
                pcall(function()
                    if ply.getDarkRPVar then
                        local ht = ply:getDarkRPVar("hitTarget")
                        if ht then
                            if type(ht) == "number" then theirTarget = Entity(ht)
                            elseif IsValid(ht) then theirTarget = ht end
                        end
                    end
                end)
                if not IsValid(theirTarget) then
                    pcall(function()
                        if ply.getDarkRPVar then
                            local ht = ply:getDarkRPVar("hit")
                            if ht and type(ht) == "number" and ht > 0 then
                                theirTarget = Entity(ht)
                            end
                        end
                    end)
                end
                if IsValid(theirTarget) and theirTarget:IsPlayer() then
                    otherHits = otherHits .. "  " .. ply:Nick() .. " -> " .. theirTarget:Nick() .. "\n"
                end
            end
        end
        if otherHits ~= "" then
            r = r .. "OTHER HITS:\n" .. otherHits
        end
    end)
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 11. Identity Clone -- copy looked-at player's identity (one-shot)
	// -----------------------------------------------------------------------
	inline const char* LUA_IDENTITY_CLONE = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "No local player" return end

    local tr = lp:GetEyeTrace()
    if not tr.Hit or not IsValid(tr.Entity) or not tr.Entity:IsPlayer() then
        pcall(function()
            chat.AddText(
                Color(255, 0, 0), "[CLONE] ",
                Color(255, 255, 255), "Look at a player first!"
            )
        end)
        r = "NO TARGET - Look at a player"
        return
    end

    local target = tr.Entity
    local targetName = target:Nick() or "Unknown"
    local modelPath = target:GetModel() or ""
    local rpName = nil

    -- Get RP name if available
    pcall(function()
        if target.getDarkRPVar then
            rpName = target:getDarkRPVar("rpname")
        end
    end)

    -- Clone steam name display
    pcall(function()
        RunConsoleCommand("setinfo", "name", targetName)
    end)

    -- Clone player model
    pcall(function()
        if modelPath ~= "" then
            RunConsoleCommand("cl_playermodel", modelPath)
            lp:SetModel(modelPath)
        end
    end)

    -- Clone RP name if DarkRP
    if rpName and rpName ~= "" then
        pcall(function()
            RunConsoleCommand("say", "/rpname " .. rpName)
        end)
    elseif rpName == nil then
        -- Fallback: try /rpname with steam name
        pcall(function()
            if DarkRP then
                RunConsoleCommand("say", "/rpname " .. targetName)
            end
        end)
    end

    -- Notify in chat
    pcall(function()
        chat.AddText(
            Color(0, 255, 0), "[CLONE] ",
            Color(255, 255, 255), "Identity cloned from " .. targetName
        )
    end)

    r = "Cloned: " .. targetName .. "\nModel: " .. (modelPath or "?")
    if rpName then r = r .. "\nRP Name: " .. rpName end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 12. Server Lua Dump -- scan loaded addons, globals, hooks (one-shot)
	// -----------------------------------------------------------------------
	inline const char* LUA_SERVER_LUA_DUMP = R"lua(
local r = ""
pcall(function()
    -- SECTION 1: Scan for known addon systems in _G
    local addonList = {}
    local adminList = {}
    local addonChecks = {
        {name="DarkRP",     keys={"DarkRP", "DARKRP", "darkrp"},           admin=false},
        {name="ULX",        keys={"ulx", "ULX"},                           admin=true},
        {name="ULib",       keys={"ULib"},                                  admin=true},
        {name="FAdmin",     keys={"FAdmin"},                                admin=true},
        {name="SAM",        keys={"sam", "SAM"},                            admin=true},
        {name="bLogs",      keys={"bLogs", "blogs"},                        admin=false},
        {name="PermaProps",  keys={"PermaProps"},                           admin=false},
        {name="Clockwork",  keys={"Clockwork", "CLOCKWORK"},                admin=false},
        {name="FPP",        keys={"FPP"},                                   admin=false},
        {name="CAMI",       keys={"CAMI"},                                  admin=true},
        {name="PAC3",       keys={"pac"},                                   admin=false},
        {name="Pointshop",  keys={"PS", "Pointshop", "pointshop"},          admin=false},
        {name="Helix",      keys={"Helix", "ix"},                           admin=false},
        {name="NutScript",  keys={"nut"},                                   admin=false},
        {name="TFA",        keys={"TFA"},                                   admin=false},
        {name="CW2",        keys={"CW2", "CustomizableWeaponry"},           admin=false},
        {name="Evolve",     keys={"evolve", "Evolve"},                      admin=true},
        {name="ServerGuard", keys={"ServerGuard", "serverguard"},           admin=true},
        {name="xAdmin",     keys={"xAdmin"},                                admin=true},
        {name="Maestro",    keys={"Maestro", "maestro"},                    admin=true},
        {name="Atlas",      keys={"Atlas", "atlas"},                        admin=false},
        {name="ArcBank",    keys={"ArcBank"},                               admin=false},
        {name="TTT",        keys={"tttc", "ROLE_INNOCENT", "GetRoundState"}, admin=false},
    }

    for _, check in ipairs(addonChecks) do
        for _, key in ipairs(check.keys) do
            local found = false
            pcall(function()
                if _G[key] ~= nil then found = true end
            end)
            if found then
                local info = check.name
                -- Try to get version info
                pcall(function()
                    local tbl = _G[check.keys[1]]
                    if type(tbl) == "table" then
                        if tbl.Version then info = info .. " v" .. tostring(tbl.Version) end
                        if tbl.version then info = info .. " v" .. tostring(tbl.version) end
                        if tbl.VERSION then info = info .. " v" .. tostring(tbl.VERSION) end
                    end
                end)
                if check.admin then
                    table.insert(adminList, info)
                else
                    table.insert(addonList, info)
                end
                break
            end
        end
    end

    -- Try to get admin list from ULX
    pcall(function()
        if ULib and ULib.ucl and ULib.ucl.authed then
            for sid, data in pairs(ULib.ucl.authed) do
                if type(data) == "table" and (data.group == "superadmin" or data.group == "admin") then
                    local name = data.name or sid
                    for _, adm in ipairs(adminList) do
                        if adm == name then name = nil break end
                    end
                    if name then
                        table.insert(adminList, "  [" .. (data.group or "?") .. "] " .. tostring(name))
                    end
                end
            end
        end
    end)

    -- SECTION 2: Workshop addons
    local workshopAddons = {}
    pcall(function()
        if engine and engine.GetAddons then
            local addons = engine.GetAddons()
            if addons then
                for i, addon in ipairs(addons) do
                    if i > 30 then break end
                    local title = addon.title or addon.name or "?"
                    local wsid = addon.wsid or addon.id or "?"
                    table.insert(workshopAddons, title .. " [" .. tostring(wsid) .. "]")
                end
            end
        end
    end)

    -- SECTION 3: Hook dump (event names + count per event)
    local hookData = {}
    pcall(function()
        if hook and hook.GetTable then
            local tbl = hook.GetTable()
            if tbl then
                for event, hooks in pairs(tbl) do
                    if type(hooks) == "table" then
                        local count = 0
                        for _ in pairs(hooks) do count = count + 1 end
                        table.insert(hookData, {name=tostring(event), count=count})
                    end
                end
                table.sort(hookData, function(a, b) return a.count > b.count end)
            end
        end
    end)

    -- SECTION 4: Lua file sources from debug registry
    local luaFiles = {}
    local luaFileSeen = {}
    pcall(function()
        local reg = debug.getregistry()
        if reg then
            local scanned = 0
            for k, v in pairs(reg) do
                if scanned > 2000 then break end
                scanned = scanned + 1
                if type(v) == "function" then
                    pcall(function()
                        local info = debug.getinfo(v, "S")
                        if info and info.source and info.source ~= "=[C]" then
                            local src = info.source
                            if not luaFileSeen[src] then
                                luaFileSeen[src] = true
                                table.insert(luaFiles, src)
                            end
                        end
                    end)
                end
            end
        end
    end)

    -- Also scan _G for functions
    pcall(function()
        local scanned = 0
        for k, v in pairs(_G) do
            if scanned > 500 then break end
            scanned = scanned + 1
            if type(v) == "function" then
                pcall(function()
                    local info = debug.getinfo(v, "S")
                    if info and info.source and info.source ~= "=[C]" then
                        local src = info.source
                        if not luaFileSeen[src] then
                            luaFileSeen[src] = true
                            table.insert(luaFiles, src)
                        end
                    end
                end)
            elseif type(v) == "table" then
                pcall(function()
                    for k2, v2 in pairs(v) do
                        if type(v2) == "function" then
                            local info = debug.getinfo(v2, "S")
                            if info and info.source and info.source ~= "=[C]" then
                                local src = info.source
                                if not luaFileSeen[src] then
                                    luaFileSeen[src] = true
                                    table.insert(luaFiles, src)
                                end
                            end
                        end
                    end
                end)
            end
        end
    end)

    -- Build report
    r = r .. "ADDONS:\n"
    if #addonList > 0 then
        for _, a in ipairs(addonList) do r = r .. "\t" .. a .. "\n" end
    else
        r = r .. "\t(none detected)\n"
    end

    r = r .. "ADMIN SYSTEMS:\n"
    if #adminList > 0 then
        for _, a in ipairs(adminList) do r = r .. "\t" .. a .. "\n" end
    else
        r = r .. "\t(none detected)\n"
    end

    r = r .. "WORKSHOP:\n"
    if #workshopAddons > 0 then
        for _, a in ipairs(workshopAddons) do r = r .. "\t" .. a .. "\n" end
    else
        r = r .. "\t(unavailable)\n"
    end

    r = r .. "HOOKS:\n"
    local hookLimit = math.min(#hookData, 40)
    for i = 1, hookLimit do
        r = r .. "\t" .. hookData[i].name .. "\t(" .. hookData[i].count .. ")\n"
    end
    if #hookData > 40 then
        r = r .. "\t... +" .. (#hookData - 40) .. " more events\n"
    end

    r = r .. "LUA FILES:\n"
    table.sort(luaFiles)
    local fileLimit = math.min(#luaFiles, 60)
    for i = 1, fileLimit do
        r = r .. "\t" .. luaFiles[i] .. "\n"
    end
    if #luaFiles > 60 then
        r = r .. "\t... +" .. (#luaFiles - 60) .. " more files\n"
    end
end)
return r
)lua";

} // namespace luascripts
