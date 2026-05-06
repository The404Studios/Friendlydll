#pragma once
#include "../includes.hpp"

namespace luascripts {

	// -----------------------------------------------------------------------
	// 7. Loot Vacuum -- auto-pickup money, weapons, items within range
	// -----------------------------------------------------------------------
	inline const char* LUA_LOOT_VACUUM_SETUP = R"lua(
pcall(function()
    if _fdll_vacuum_installed then return end
    _fdll_vacuum_installed = true
    _fdll_vacuum_next = 0
    _fdll_vacuum_target = nil
    _fdll_vacuum_used = _fdll_vacuum_used or {}

    local lootClasses = {
        ["spawned_money"] = true, ["spawned_weapon"] = true,
        ["money_printer_money"] = true, ["printer_money"] = true,
        ["dropped_money"] = true
    }

    local function isLoot(ent)
        if not IsValid(ent) then return false end
        local cls = string.lower(ent:GetClass() or "")
        if lootClasses[cls] then return true end
        if string.sub(cls, 1, 8) == "dropped_" then return true end
        if string.sub(cls, 1, 5) == "item_" then return true end
        if string.find(cls, "pickup") then return true end
        if string.find(cls, "loot") then return true end
        if string.find(cls, "spawned_") then return true end
        return false
    end

    -- Think: find nearest loot entity to target
    hook.Add("Think", "fdll_loot_vacuum", function()
        if not _fdll_vacuum_installed then return end
        local now = CurTime()
        if now < _fdll_vacuum_next then return end
        _fdll_vacuum_next = now + 0.15

        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then return end

        local lpPos = lp:GetPos()
        local bestEnt = nil
        local bestDist = 200

        local nearby = ents.FindInSphere(lpPos, 200)
        for _, ent in ipairs(nearby) do
            if IsValid(ent) and not ent:IsPlayer() and not ent:IsNPC() then
                local eidx = ent:EntIndex()
                if not _fdll_vacuum_used[eidx] and isLoot(ent) then
                    local dist = lpPos:Distance(ent:GetPos())
                    if dist < bestDist then
                        bestDist = dist
                        bestEnt = ent
                    end
                end
            end
        end

        if IsValid(bestEnt) then
            _fdll_vacuum_target = bestEnt
            _fdll_vacuum_used[bestEnt:EntIndex()] = now
        end

        -- Prune used-entity tracker
        if not _fdll_vacuum_prune or now > _fdll_vacuum_prune then
            _fdll_vacuum_prune = now + 5
            for k, v in pairs(_fdll_vacuum_used) do
                if now - v > 3 then _fdll_vacuum_used[k] = nil end
            end
        end
    end)

    -- CreateMove: silently aim at loot entity and press USE for one tick
    hook.Add("CreateMove", "fdll_loot_vacuum_use", function(cmd)
        if not _fdll_vacuum_target or not IsValid(_fdll_vacuum_target) then return end
        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then
            _fdll_vacuum_target = nil
            return
        end

        local eyePos = lp:EyePos()
        local entPos = _fdll_vacuum_target:GetPos() + Vector(0, 0, 5)
        local dir = entPos - eyePos

        -- Only if within use range (~100 units)
        if dir:LengthSqr() > 10000 then
            _fdll_vacuum_target = nil
            return
        end

        -- Silent aim: change viewangles in CUserCmd (server sees it, client doesn't)
        local useAng = dir:Angle()
        cmd:SetViewAngles(useAng)
        cmd:SetButtons(bit.bor(cmd:GetButtons(), IN_USE))

        _fdll_vacuum_target = nil
    end)
end)
)lua";

	inline const char* LUA_LOOT_VACUUM_STOP = R"lua(
pcall(function()
    _fdll_vacuum_installed = false
    _fdll_vacuum_target = nil
    _fdll_vacuum_used = {}
    hook.Remove("Think", "fdll_loot_vacuum")
    hook.Remove("CreateMove", "fdll_loot_vacuum_use")
end)
)lua";

	// -----------------------------------------------------------------------
	// 8. Police Scanner -- one-shot query of all cops on server
	// -----------------------------------------------------------------------
	inline const char* LUA_POLICE_SCANNER = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local lpPos = lp:GetPos()
    local myYaw = lp:EyeAngles().y

    local compassDirs = {"N","NE","E","SE","S","SW","W","NW"}
    local function getBearing(theirPos)
        local dx = theirPos.x - lpPos.x
        local dy = theirPos.y - lpPos.y
        local worldAng = math.deg(math.atan2(dy, dx))
        local rel = (worldAng - myYaw + 360) % 360
        local idx = math.floor((rel + 22.5) / 45) % 8 + 1
        return compassDirs[idx], math.floor(rel)
    end

    local function isCop(ply)
        if not IsValid(ply) then return false end
        if ply.isCP then
            local ok, val = pcall(ply.isCP, ply)
            if ok and val then return true end
        end
        local ok2, tname = pcall(team.GetName, ply:Team())
        if ok2 and tname then
            local tl = string.lower(tname)
            if string.find(tl, "police") or string.find(tl, "civil protection")
               or string.find(tl, "chief") or string.find(tl, "mayor")
               or string.find(tl, "swat") or string.find(tl, "sheriff")
               or string.find(tl, "secret service") or string.find(tl, "fbi") then
                return true
            end
        end
        return false
    end

    local cops = {}

    for _, ply in ipairs(player.GetAll()) do
        if IsValid(ply) and isCop(ply) then
            local ok, entry = pcall(function()
                local name = ply:Nick() or "?"
                local job = ""
                if ply.getDarkRPVar then
                    job = tostring(ply:getDarkRPVar("job") or "")
                end
                if job == "" then
                    local s, tn = pcall(team.GetName, ply:Team())
                    if s and tn then job = tn end
                end

                local hp = ply:Health() or 0
                local armor = ply:Armor() or 0
                local pos = ply:GetPos()
                local dist = math.floor(lpPos:Distance(pos))
                local compass, bearing = getBearing(pos)

                local wep = ""
                pcall(function()
                    local aw = ply:GetActiveWeapon()
                    if IsValid(aw) then wep = aw:GetClass() end
                end)

                local moving = false
                pcall(function()
                    if ply:GetVelocity():LengthSqr() > 2500 then
                        local vel = ply:GetVelocity():GetNormalized()
                        local toMe = (lpPos - pos):GetNormalized()
                        moving = vel:Dot(toMe) > 0.5
                    end
                end)

                return {
                    name = name, job = job, hp = hp, armor = armor,
                    wep = wep, dist = dist,
                    compass = compass, bearing = bearing,
                    approaching = moving
                }
            end)
            if ok and entry then
                cops[#cops + 1] = entry
            end
        end
    end

    table.sort(cops, function(a, b) return a.dist < b.dist end)

    r = "COPS FOUND: " .. #cops .. "\n"
    for _, c in ipairs(cops) do
        local status = c.approaching and " >>APPROACHING" or ""
        r = r .. c.compass .. " " .. c.bearing .. "deg\t" .. c.name .. "\t" .. c.job
            .. "\tHP:" .. c.hp .. "\tAR:" .. c.armor
            .. "\t" .. c.wep .. "\t" .. c.dist .. "u" .. status .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 9. Economy Analyzer -- one-shot scan of DarkRP job table + economy
	// -----------------------------------------------------------------------
	inline const char* LUA_ECONOMY_ANALYZER = R"lua(
local r = ""
pcall(function()
    local jobs = {}
    local bestJob = ""
    local bestSalary = 0
    local totalJobs = 0

    -- Read RPExtraTeams (main DarkRP job table)
    if RPExtraTeams then
        for _, jdata in pairs(RPExtraTeams) do
            pcall(function()
                local name = jdata.name or jdata.Name or "Unknown"
                local salary = tonumber(jdata.salary or jdata.pay or jdata.Salary or 0) or 0
                local maxPlayers = tonumber(jdata.max or jdata.maxPlayers or jdata.Max or 0) or 0
                local teamId = jdata.team

                local current = 0
                if teamId then
                    local ok, cnt = pcall(team.NumPlayers, teamId)
                    if ok and cnt then current = cnt end
                end

                local avail = 0
                if maxPlayers > 0 then
                    avail = math.max(0, maxPlayers - current)
                end

                totalJobs = totalJobs + 1
                if salary > bestSalary then
                    bestSalary = salary
                    bestJob = name
                end

                jobs[#jobs + 1] = {
                    name = name,
                    salary = salary,
                    current = current,
                    max = maxPlayers,
                    avail = avail
                }
            end)
        end
    end

    -- Also try DarkRP.getCategories for any extra data
    if DarkRP and DarkRP.getCategories then
        pcall(function()
            local cats = DarkRP.getCategories()
            if cats and cats.jobs then
                for _, cat in pairs(cats.jobs) do
                    -- categories mostly group existing jobs, skip duplicates
                end
            end
        end)
    end

    -- Check for money multipliers in GAMEMODE.Config
    local multiplier = ""
    pcall(function()
        if GAMEMODE and GAMEMODE.Config then
            local cfg = GAMEMODE.Config
            if cfg.payaliday then
                multiplier = multiplier .. "PayDay=" .. tostring(cfg.payaliday) .. " "
            end
            if cfg.payaliday == nil and cfg.paydelay then
                multiplier = multiplier .. "PayDelay=" .. tostring(cfg.paydelay) .. " "
            end
            if cfg.payaliday == nil and cfg.paydaytime then
                multiplier = multiplier .. "PayDayTime=" .. tostring(cfg.paydaytime) .. " "
            end
            if cfg.normalsalary then
                multiplier = multiplier .. "NormalSalary=" .. tostring(cfg.normalsalary) .. " "
            end
            if cfg.maxpocketitems then
                multiplier = multiplier .. "MaxPocket=" .. tostring(cfg.maxpocketitems) .. " "
            end
            if cfg.doormaxown then
                multiplier = multiplier .. "MaxDoors=" .. tostring(cfg.doormaxown) .. " "
            end
        end
    end)

    -- Sort by salary descending
    table.sort(jobs, function(a, b) return a.salary > b.salary end)

    -- Summary line
    r = "TOTAL JOBS: " .. totalJobs .. " | BEST: " .. bestJob .. " ($" .. bestSalary .. ")"
    if multiplier ~= "" then
        r = r .. " | CONFIG: " .. multiplier
    end
    r = r .. "\n"

    -- Job lines
    for _, j in ipairs(jobs) do
        local maxStr = (j.max > 0) and tostring(j.max) or "INF"
        r = r .. j.name .. "\t" .. j.salary .. "\t" .. j.current .. "/" .. maxStr
            .. "\t" .. j.avail .. "\n"
    end
end)
return r
)lua";

} // namespace luascripts
