#pragma once
#include "../includes.hpp"

namespace luascripts {

	// -----------------------------------------------------------------------
	// 1. Warrant Shield -- auto-close fading doors when warranted + HUD warn
	// -----------------------------------------------------------------------
	inline const char* LUA_WARRANT_SHIELD_SETUP = R"lua(
pcall(function()
    if _fdll_warrant_shield_installed then return end
    _fdll_warrant_shield_installed = true
    _fdll_warrant_shield_warned = false

    local function closeFadingDoors()
        local lp = LocalPlayer()
        if not IsValid(lp) then return end
        for _, ent in ipairs(ents.GetAll()) do
            if IsValid(ent) and ent.isFadingDoor then
                local owner = nil
                if ent.CPPIGetOwner then
                    owner = ent:CPPIGetOwner()
                end
                if not IsValid(owner) and ent.Getowning_ent then
                    pcall(function() owner = ent:Getowning_ent() end)
                end
                if IsValid(owner) and owner == lp then
                    for key = 1, 9 do
                        pcall(function()
                            numpad.Activate(lp, key, true)
                        end)
                    end
                end
            end
        end
    end

    hook.Add("DarkRPVarChanged", "fdll_warrant_shield", function(ply, var, old, new)
        if ply ~= LocalPlayer() then return end
        if var ~= "wanted" then return end
        if new then
            closeFadingDoors()
            _fdll_warrant_shield_warned = true
            _fdll_warrant_shield_warntime = CurTime()

            hook.Add("HUDPaint", "fdll_warrant_hud", function()
                if not _fdll_warrant_shield_warned then
                    hook.Remove("HUDPaint", "fdll_warrant_hud")
                    return
                end
                local elapsed = CurTime() - (_fdll_warrant_shield_warntime or CurTime())
                if elapsed > 10 then
                    _fdll_warrant_shield_warned = false
                    hook.Remove("HUDPaint", "fdll_warrant_hud")
                    return
                end
                local alpha = math.Clamp(math.sin(CurTime() * 4) * 50 + 200, 0, 255)
                draw.SimpleText(
                    "WARRANTED - HIDING CONTRABAND",
                    "DermaLarge",
                    ScrW() / 2, 40,
                    Color(255, 30, 30, alpha),
                    TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER
                )
            end)

            timer.Simple(10, function()
                _fdll_warrant_shield_warned = false
                hook.Remove("HUDPaint", "fdll_warrant_hud")
            end)
        end
    end)
end)
)lua";

	inline const char* LUA_WARRANT_SHIELD_STOP = R"lua(
pcall(function()
    _fdll_warrant_shield_installed = false
    _fdll_warrant_shield_warned = false
    hook.Remove("DarkRPVarChanged", "fdll_warrant_shield")
    hook.Remove("HUDPaint", "fdll_warrant_hud")
end)
)lua";

	// -----------------------------------------------------------------------
	// 2. Anti-Arrest -- auto-evade nearby CP players + HUD warning
	// -----------------------------------------------------------------------
	inline const char* LUA_ANTI_ARREST_SETUP = R"lua(
pcall(function()
    if _fdll_anti_arrest_installed then return end
    _fdll_anti_arrest_installed = true
    _fdll_anti_arrest_cop_near = false
    _fdll_anti_arrest_escape = nil
    _fdll_anti_arrest_cop_name = ""
    _fdll_anti_arrest_cop_dist = 0

    local function isCop(ply)
        if not IsValid(ply) then return false end
        if ply.isCP then
            local ok, val = pcall(ply.isCP, ply)
            if ok and val then return true end
        end
        local ok2, tname = pcall(team.GetName, ply:Team())
        if ok2 and tname then
            local tl = string.lower(tname)
            if string.find(tl, "police") or string.find(tl, "civil") or
               string.find(tl, "mayor") or string.find(tl, "chief") or
               string.find(tl, "swat") then
                return true
            end
        end
        return false
    end

    hook.Add("Think", "fdll_anti_arrest", function()
        if not _fdll_anti_arrest_installed then return end
        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then
            _fdll_anti_arrest_escape = nil
            _fdll_anti_arrest_cop_near = false
            return
        end

        local lpPos = lp:GetPos()
        local closestDist = 250
        local closestPos = nil
        local closestName = ""

        for _, ply in ipairs(player.GetAll()) do
            if ply ~= lp and IsValid(ply) and ply:Alive() and isCop(ply) then
                local dist = lpPos:Distance(ply:GetPos())
                if dist < closestDist then
                    closestDist = dist
                    closestPos = ply:GetPos()
                    closestName = ply:Nick()
                end
            end
        end

        if closestPos then
            _fdll_anti_arrest_cop_near = true
            _fdll_anti_arrest_cop_name = closestName
            _fdll_anti_arrest_cop_dist = math.floor(closestDist)
            -- Escape direction: away from cop, flattened to XY
            local escapeDir = lpPos - closestPos
            escapeDir.z = 0
            if escapeDir:LengthSqr() > 1 then
                escapeDir:Normalize()
                _fdll_anti_arrest_escape = escapeDir
            end
        else
            _fdll_anti_arrest_cop_near = false
            _fdll_anti_arrest_escape = nil
        end
    end)

    -- CreateMove: steer movement AWAY from nearest cop without changing view
    hook.Add("CreateMove", "fdll_anti_arrest_move", function(cmd)
        if not _fdll_anti_arrest_escape then return end
        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then return end

        local escDir = _fdll_anti_arrest_escape
        local viewAng = cmd:GetViewAngles()
        local yawRad = math.rad(viewAng.y)

        -- Project escape direction onto view-relative axes
        local cosY = math.cos(yawRad)
        local sinY = math.sin(yawRad)
        -- Forward in world = (cos(yaw), sin(yaw)), Right = (sin(yaw), -cos(yaw))
        local fwdDot  =  escDir.x * cosY + escDir.y * sinY
        local sideDot =  escDir.x * sinY - escDir.y * cosY

        cmd:SetForwardMove(fwdDot * 10000)
        cmd:SetSideMove(sideDot * 10000)

        -- Auto-jump when on ground
        local flags = lp:GetMoveType() ~= MOVETYPE_NOCLIP and lp:OnGround()
        if flags then
            cmd:SetButtons(bit.bor(cmd:GetButtons(), IN_JUMP))
        end
    end)

    hook.Add("HUDPaint", "fdll_arrest_hud", function()
        if not _fdll_anti_arrest_installed then return end
        if _fdll_anti_arrest_cop_near then
            local alpha = math.Clamp(math.sin(CurTime() * 6) * 60 + 200, 0, 255)
            local txt = "COP NEARBY: " .. tostring(_fdll_anti_arrest_cop_name or "?")
                        .. " (" .. tostring(_fdll_anti_arrest_cop_dist or 0) .. "u)"
            draw.SimpleText(txt, "DermaLarge",
                ScrW() / 2, 70, Color(255, 255, 0, alpha),
                TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER)
        end
    end)
end)
)lua";

	inline const char* LUA_ANTI_ARREST_STOP = R"lua(
pcall(function()
    _fdll_anti_arrest_installed = false
    _fdll_anti_arrest_cop_near = false
    _fdll_anti_arrest_escape = nil
    hook.Remove("Think", "fdll_anti_arrest")
    hook.Remove("CreateMove", "fdll_anti_arrest_move")
    hook.Remove("HUDPaint", "fdll_arrest_hud")
end)
)lua";

	// -----------------------------------------------------------------------
	// 3. Base Alarm -- detect lockpick/keypad crack sounds near your base
	// -----------------------------------------------------------------------
	inline const char* LUA_BASE_ALARM_SETUP = R"lua(
pcall(function()
    if _fdll_base_alarm_installed then return end
    _fdll_base_alarm_installed = true
    _fdll_base_alarm_active = false
    _fdll_base_alarm_time = 0
    _fdll_base_alarm_attacker = "Unknown"

    local triggerSounds = {
        "lockpick", "keypad_crack", "door_break", "break_door",
        "lock_pick", "crack", "breakin", "crowbar_hit"
    }

    hook.Add("EntityEmitSound", "fdll_base_alarm", function(data)
        if not _fdll_base_alarm_installed then return end
        local lp = LocalPlayer()
        if not IsValid(lp) then return end

        local snd = string.lower(data.SoundName or "")
        local matched = false
        for _, trigger in ipairs(triggerSounds) do
            if string.find(snd, trigger) then
                matched = true
                break
            end
        end
        if not matched then return end

        local origin = data.Pos or (IsValid(data.Entity) and data.Entity:GetPos()) or nil
        if not origin then return end

        local dist = lp:GetPos():Distance(origin)
        if dist > 1500 then return end

        local attackerName = "Unknown"
        if IsValid(data.Entity) and data.Entity:IsPlayer() then
            attackerName = data.Entity:Nick()
        elseif IsValid(data.Entity) then
            local owner = nil
            if data.Entity.CPPIGetOwner then
                pcall(function() owner = data.Entity:CPPIGetOwner() end)
            end
            if IsValid(owner) and owner:IsPlayer() then
                attackerName = owner:Nick()
            else
                attackerName = data.Entity:GetClass()
            end
        end

        _fdll_base_alarm_active = true
        _fdll_base_alarm_time = CurTime()
        _fdll_base_alarm_attacker = attackerName

        pcall(function()
            surface.PlaySound("buttons/button17.wav")
        end)

        pcall(function()
            chat.AddText(
                Color(255, 0, 0), "[ALARM] ",
                Color(255, 255, 255), "Intrusion detected! Suspect: " .. attackerName
            )
        end)

        hook.Add("HUDPaint", "fdll_alarm_hud", function()
            if not _fdll_base_alarm_active then
                hook.Remove("HUDPaint", "fdll_alarm_hud")
                return
            end
            local elapsed = CurTime() - _fdll_base_alarm_time
            if elapsed > 5 then
                _fdll_base_alarm_active = false
                hook.Remove("HUDPaint", "fdll_alarm_hud")
                return
            end
            local fade = math.Clamp(1.0 - (elapsed / 5.0), 0, 1)
            local pulse = math.sin(CurTime() * 8) * 0.3 + 0.7
            local alpha = math.Clamp(fade * pulse * 255, 0, 255)

            draw.RoundedBox(0, 0, 0, ScrW(), ScrH(), Color(255, 0, 0, alpha * 0.15))

            draw.SimpleText(
                "INTRUSION DETECTED",
                "DermaLarge",
                ScrW() / 2, ScrH() / 2 - 30,
                Color(255, 50, 50, alpha),
                TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER
            )
            draw.SimpleText(
                "Suspect: " .. tostring(_fdll_base_alarm_attacker),
                "DermaDefault",
                ScrW() / 2, ScrH() / 2 + 10,
                Color(255, 200, 200, alpha),
                TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER
            )
        end)
    end)
end)
)lua";

	inline const char* LUA_BASE_ALARM_STOP = R"lua(
pcall(function()
    _fdll_base_alarm_installed = false
    _fdll_base_alarm_active = false
    hook.Remove("EntityEmitSound", "fdll_base_alarm")
    hook.Remove("HUDPaint", "fdll_alarm_hud")
end)
)lua";

} // namespace luascripts
