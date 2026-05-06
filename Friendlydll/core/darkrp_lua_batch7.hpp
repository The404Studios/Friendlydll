#pragma once
#include "../includes.hpp"

namespace luascripts {

	// -----------------------------------------------------------------------
	// 19. Entity Telekinesis -- pull/push entities with R/T keys + extended USE
	// -----------------------------------------------------------------------
	inline const char* LUA_ENTITY_TELEKINESIS_SETUP = R"lua(
pcall(function()
    if _fdll_telekinesis_installed then return end
    _fdll_telekinesis_installed = true
    _fdll_tk_target = nil
    _fdll_tk_pulling = false
    _fdll_tk_pushing = false
    _fdll_tk_line_start = nil
    _fdll_tk_line_end = nil
    _fdll_tk_last_action = 0
    _fdll_tk_extended_use_target = nil

    -- Think hook: track the entity the player is looking at (extended range, through walls)
    hook.Add("Think", "fdll_telekinesis_think", function()
        if not _fdll_telekinesis_installed then return end
        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then
            _fdll_tk_target = nil
            _fdll_tk_line_start = nil
            _fdll_tk_line_end = nil
            return
        end

        local eyePos = lp:EyePos()
        local eyeDir = lp:GetAimVector()
        local endPos = eyePos + eyeDir * 2000

        -- Primary trace: find entities through walls using MASK_ALL
        local bestEnt = nil
        local bestDist = math.huge

        -- Standard trace first
        pcall(function()
            local tr = util.TraceLine({
                start = eyePos,
                endpos = endPos,
                filter = lp,
                mask = MASK_ALL
            })
            if tr.Hit and IsValid(tr.Entity) and not tr.Entity:IsPlayer() then
                bestEnt = tr.Entity
                bestDist = eyePos:Distance(tr.Entity:GetPos())
            end
        end)

        -- If wall blocked, scan nearby entities along the aim line
        if not IsValid(bestEnt) then
            pcall(function()
                for _, ent in ipairs(ents.FindInSphere(eyePos, 2000)) do
                    if IsValid(ent) and not ent:IsPlayer() and ent:EntIndex() > 0 then
                        local entPos = ent:GetPos()
                        -- Check if entity is roughly along our aim line
                        local toEnt = entPos - eyePos
                        local dist = toEnt:Length()
                        if dist > 10 and dist < 2000 then
                            local dot = toEnt:GetNormalized():Dot(eyeDir)
                            if dot > 0.98 then -- within ~11 degrees of crosshair
                                if dist < bestDist then
                                    bestDist = dist
                                    bestEnt = ent
                                end
                            end
                        end
                    end
                end
            end)
        end

        _fdll_tk_target = bestEnt

        -- Handle pull (R key = KEY_R = 18) and push (T key = KEY_T = 20)
        local now = CurTime()
        if now < _fdll_tk_last_action + 0.05 then return end

        local pulling = input.IsKeyDown(KEY_R)
        local pushing = input.IsKeyDown(KEY_T)
        _fdll_tk_pulling = pulling
        _fdll_tk_pushing = pushing

        if (pulling or pushing) and IsValid(_fdll_tk_target) then
            _fdll_tk_last_action = now
            local ent = _fdll_tk_target
            local lpPos = lp:GetPos() + Vector(0, 0, 40)
            local entPos = ent:GetPos()
            local dir = (lpPos - entPos):GetNormalized()
            if pushing then dir = -dir end
            local force = dir * 800

            _fdll_tk_line_start = lp:EyePos()
            _fdll_tk_line_end = ent:GetPos()

            -- Method 1: SetPos directly (small incremental moves)
            pcall(function()
                if ent.SetPos then
                    local newPos = entPos + dir * 5
                    ent:SetPos(newPos)
                end
            end)

            -- Method 2: Physics object velocity
            pcall(function()
                if ent.GetPhysicsObject then
                    local phys = ent:GetPhysicsObject()
                    if IsValid(phys) then
                        phys:SetVelocity(force)
                        phys:Wake()
                    end
                end
            end)

            -- Method 3: Fallback Fire("Use") for usable entities
            pcall(function()
                if ent.Fire then
                    ent:Fire("Use", "", 0)
                end
            end)
        else
            _fdll_tk_line_start = nil
            _fdll_tk_line_end = nil
        end
    end)

    -- CreateMove hook: extend USE range to 1000 units
    hook.Add("CreateMove", "fdll_telekinesis_use", function(cmd)
        if not _fdll_telekinesis_installed then return end
        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then return end

        -- Only act when USE (+use / IN_USE) is held
        if bit.band(cmd:GetButtons(), IN_USE) == 0 then
            _fdll_tk_extended_use_target = nil
            return
        end

        -- Standard USE range is ~100 units; we extend to 1000
        local eyePos = lp:EyePos()
        local eyeAng = cmd:GetViewAngles()
        local eyeDir = eyeAng:Forward()
        local endPos = eyePos + eyeDir * 1000

        local tr = nil
        pcall(function()
            tr = util.TraceLine({
                start = eyePos,
                endpos = endPos,
                filter = lp,
                mask = MASK_SHOT
            })
        end)

        if tr and tr.Hit and IsValid(tr.Entity) and tr.Entity:EntIndex() > 0 then
            local ent = tr.Entity
            local entPos = ent:GetPos()
            local dist = eyePos:Distance(entPos)

            -- Only redirect aim if entity is beyond normal USE range
            if dist > 100 and dist <= 1000 then
                -- Calculate angle to entity for one tick
                local toEnt = (entPos + ent:OBBCenter()) - eyePos
                local targetAng = toEnt:Angle()
                cmd:SetViewAngles(targetAng)
                _fdll_tk_extended_use_target = ent
            end
        end
    end)

    -- HUDPaint: draw visual feedback line from player to target
    hook.Add("HUDPaint", "fdll_telekinesis_hud", function()
        if not _fdll_telekinesis_installed then return end
        local lp = LocalPlayer()
        if not IsValid(lp) then return end

        -- Draw status indicator
        local statusText = "TELEKINESIS: ON"
        local statusCol = Color(100, 255, 100, 200)
        if IsValid(_fdll_tk_target) then
            local cls = _fdll_tk_target:GetClass() or "?"
            local dist = math.floor(lp:GetPos():Distance(_fdll_tk_target:GetPos()))
            statusText = "TK TARGET: " .. cls .. " [" .. dist .. "u]"
            if _fdll_tk_pulling then
                statusText = statusText .. " << PULLING"
                statusCol = Color(100, 150, 255, 220)
            elseif _fdll_tk_pushing then
                statusText = statusText .. " PUSHING >>"
                statusCol = Color(255, 150, 100, 220)
            end
        end

        pcall(function()
            draw.SimpleText(
                statusText, "DermaDefault",
                ScrW() / 2, ScrH() - 60,
                statusCol,
                TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER
            )
        end)

        -- Draw beam line to manipulated entity
        if _fdll_tk_line_start and _fdll_tk_line_end then
            pcall(function()
                local s = _fdll_tk_line_start:ToScreen()
                local e = _fdll_tk_line_end:ToScreen()
                if s.visible and e.visible then
                    local beamCol = Color(100, 150, 255, 180)
                    if _fdll_tk_pushing then
                        beamCol = Color(255, 100, 50, 180)
                    end
                    surface.SetDrawColor(beamCol)
                    surface.DrawLine(s.x, s.y, e.x, e.y)
                    -- Draw small circle at target
                    local radius = 6
                    for i = 0, 360, 15 do
                        local rad = math.rad(i)
                        local rad2 = math.rad(i + 15)
                        surface.DrawLine(
                            e.x + math.cos(rad) * radius, e.y + math.sin(rad) * radius,
                            e.x + math.cos(rad2) * radius, e.y + math.sin(rad2) * radius
                        )
                    end
                end
            end)
        end

        -- Draw crosshair indicator when extended USE is active
        if IsValid(_fdll_tk_extended_use_target) then
            pcall(function()
                local tPos = (_fdll_tk_extended_use_target:GetPos() + _fdll_tk_extended_use_target:OBBCenter()):ToScreen()
                if tPos.visible then
                    surface.SetDrawColor(Color(0, 255, 0, 200))
                    surface.DrawLine(tPos.x - 10, tPos.y, tPos.x + 10, tPos.y)
                    surface.DrawLine(tPos.x, tPos.y - 10, tPos.x, tPos.y + 10)
                    draw.SimpleText("USE", "DermaDefault", tPos.x, tPos.y - 16,
                        Color(0, 255, 0, 200), TEXT_ALIGN_CENTER, TEXT_ALIGN_BOTTOM)
                end
            end)
        end
    end)
end)
)lua";

	inline const char* LUA_ENTITY_TELEKINESIS_STOP = R"lua(
pcall(function()
    _fdll_telekinesis_installed = false
    _fdll_tk_target = nil
    _fdll_tk_pulling = false
    _fdll_tk_pushing = false
    _fdll_tk_line_start = nil
    _fdll_tk_line_end = nil
    _fdll_tk_extended_use_target = nil
    hook.Remove("Think", "fdll_telekinesis_think")
    hook.Remove("CreateMove", "fdll_telekinesis_use")
    hook.Remove("HUDPaint", "fdll_telekinesis_hud")
end)
)lua";

	// -----------------------------------------------------------------------
	// 20. Ownership Steal -- attempt to claim ownership of traced entity
	// -----------------------------------------------------------------------
	inline const char* LUA_OWNERSHIP_STEAL = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "ERROR: LocalPlayer invalid" return end

    local tr = lp:GetEyeTrace()
    if not tr or not IsValid(tr.Entity) then
        r = "ERROR: Look at an entity first"
        return
    end

    local ent = tr.Entity
    local cls = ent:GetClass() or "unknown"
    local idx = ent:EntIndex()
    r = "OWNERSHIP STEAL: " .. cls .. " [#" .. idx .. "]\n"

    local results = {}

    -- Method 1: CPPISetOwner
    pcall(function()
        if ent.CPPISetOwner then
            ent:CPPISetOwner(lp)
            -- Verify
            local ok2, owner = pcall(function()
                if ent.CPPIGetOwner then return ent:CPPIGetOwner() end
                return nil
            end)
            if ok2 and IsValid(owner) and owner == lp then
                table.insert(results, "CPPISetOwner: SUCCESS (verified)")
            else
                table.insert(results, "CPPISetOwner: CALLED (unverified)")
            end
        else
            table.insert(results, "CPPISetOwner: NOT AVAILABLE")
        end
    end)

    -- Method 2: owning_ent datatable
    pcall(function()
        if ent.SetDTEntity then
            ent:SetDTEntity(0, lp)
            table.insert(results, "SetDTEntity(owning_ent): CALLED")
        else
            table.insert(results, "SetDTEntity: NOT AVAILABLE")
        end
    end)

    pcall(function()
        if ent.Setowning_ent then
            ent:Setowning_ent(lp)
            -- Verify
            local ok2, cur = pcall(ent.Getowning_ent, ent)
            if ok2 and IsValid(cur) and cur == lp then
                table.insert(results, "Setowning_ent: SUCCESS (verified)")
            else
                table.insert(results, "Setowning_ent: CALLED (unverified)")
            end
        else
            table.insert(results, "Setowning_ent: NOT AVAILABLE")
        end
    end)

    -- Method 3: NWEntity Owner
    pcall(function()
        ent:SetNWEntity("Owner", lp)
        local check = ent:GetNWEntity("Owner")
        if IsValid(check) and check == lp then
            table.insert(results, "SetNWEntity Owner: SUCCESS (verified)")
        else
            table.insert(results, "SetNWEntity Owner: CALLED (unverified)")
        end
    end)

    pcall(function()
        ent:SetNWEntity("owning_ent", lp)
        table.insert(results, "SetNWEntity owning_ent: CALLED")
    end)

    -- Method 4: Door-specific ownership via Fire
    pcall(function()
        local isDoor = string.find(cls, "door") ~= nil
        if isDoor then
            ent:Fire("SetOwner", tostring(lp:EntIndex()), 0)
            table.insert(results, "Fire SetOwner (door): CALLED")

            -- Also try DarkRP door ownership
            if ent.keysOwn then
                ent:keysOwn(lp)
                table.insert(results, "keysOwn (DarkRP door): CALLED")
            end

            if ent.addKeysDoorOwner then
                ent:addKeysDoorOwner(lp)
                table.insert(results, "addKeysDoorOwner: CALLED")
            end
        end
    end)

    -- Method 5: Fading door numpad registration
    pcall(function()
        if ent.isFadingDoor or string.find(cls, "fading") then
            for key = 1, 9 do
                pcall(function()
                    numpad.OnDown(lp, key, "FadingDoor_Toggle", ent)
                end)
            end
            table.insert(results, "Fading door numpad register: CALLED (keys 1-9)")
        end
    end)

    -- Method 6: DarkRP-specific setKeysNonOwnable / setDoorOwner
    pcall(function()
        if ent.setDoorOwner then
            ent:setDoorOwner(lp)
            table.insert(results, "setDoorOwner: CALLED")
        end
    end)

    for _, line in ipairs(results) do
        r = r .. "  " .. line .. "\n"
    end

    if #results == 0 then
        r = r .. "  No ownership methods available on this entity\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 21. Entity Teleport -- teleport traced entity to player position
	// -----------------------------------------------------------------------
	inline const char* LUA_ENTITY_TELEPORT = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "ERROR: LocalPlayer invalid" return end

    local tr = lp:GetEyeTrace()
    if not tr or not IsValid(tr.Entity) then
        r = "ERROR: Look at an entity first"
        return
    end

    local ent = tr.Entity
    local cls = ent:GetClass() or "unknown"
    local idx = ent:EntIndex()
    local oldPos = ent:GetPos()
    local targetPos = lp:GetPos() + lp:GetForward() * 50 + Vector(0, 0, 10)

    r = "ENTITY TELEPORT: " .. cls .. " [#" .. idx .. "]\n"
    r = r .. "FROM: " .. math.floor(oldPos.x) .. ", " .. math.floor(oldPos.y) .. ", " .. math.floor(oldPos.z) .. "\n"
    r = r .. "TO: " .. math.floor(targetPos.x) .. ", " .. math.floor(targetPos.y) .. ", " .. math.floor(targetPos.z) .. "\n"

    local success = false

    -- Method 1: Direct SetPos on entity
    pcall(function()
        if ent.SetPos then
            ent:SetPos(targetPos)
            local newPos = ent:GetPos()
            local dist = newPos:Distance(targetPos)
            if dist < 50 then
                success = true
                r = r .. "SetPos: SUCCESS (delta " .. math.floor(dist) .. "u)\n"
            else
                r = r .. "SetPos: CALLED (entity may have rejected move)\n"
            end
        else
            r = r .. "SetPos: NOT AVAILABLE\n"
        end
    end)

    -- Method 2: Physics object SetPos
    pcall(function()
        if ent.GetPhysicsObject then
            local phys = ent:GetPhysicsObject()
            if IsValid(phys) then
                phys:SetPos(targetPos)
                phys:SetVelocity(Vector(0, 0, 0))
                phys:SetAngles(Angle(0, 0, 0))
                phys:Wake()
                r = r .. "PhysObj SetPos: CALLED\n"
                success = true
            else
                r = r .. "PhysObj: INVALID\n"
            end
        end
    end)

    -- Method 3: PhysicsObject EnableMotion + teleport
    pcall(function()
        if ent.GetPhysicsObject then
            local phys = ent:GetPhysicsObject()
            if IsValid(phys) then
                phys:EnableMotion(true)
                phys:SetPos(targetPos)
                phys:SetVelocity(Vector(0, 0, 1))
                phys:Wake()
                r = r .. "PhysObj EnableMotion+SetPos: CALLED\n"
            end
        end
    end)

    -- Method 4: For props, try SetMoveType + SetPos
    pcall(function()
        if string.find(cls, "prop") then
            ent:SetMoveType(MOVETYPE_VPHYSICS)
            ent:SetPos(targetPos)
            r = r .. "Prop MoveType+SetPos: CALLED\n"
        end
    end)

    if success then
        r = r .. "RESULT: Teleport likely succeeded\n"
    else
        r = r .. "RESULT: Teleport may have been blocked server-side\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 22. Mass Grab -- pull all nearby unowned entities toward player
	// -----------------------------------------------------------------------
	inline const char* LUA_MASS_GRAB = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "ERROR: LocalPlayer invalid" return end

    local lpPos = lp:GetPos() + Vector(0, 0, 40)
    local radius = 500
    local affected = 0
    local skipped = 0
    local errors = 0
    local details = {}

    for _, ent in ipairs(ents.FindInSphere(lpPos, radius)) do
        if not IsValid(ent) then continue end
        if ent:IsPlayer() then continue end
        if ent:IsWeapon() then continue end
        if ent:EntIndex() <= 0 then continue end

        local cls = ent:GetClass() or ""

        -- Skip world entities and map brushes
        if cls == "worldspawn" or cls == "" then continue end
        if string.find(cls, "func_") then continue end

        -- Check if entity is unowned or world-owned
        local isUnowned = true
        pcall(function()
            if ent.CPPIGetOwner then
                local owner = ent:CPPIGetOwner()
                if IsValid(owner) and owner:IsPlayer() and owner ~= lp then
                    isUnowned = false
                end
            end
        end)

        if not isUnowned then
            pcall(function()
                if ent.Getowning_ent then
                    local owner = ent:Getowning_ent()
                    if IsValid(owner) and owner:IsPlayer() and owner ~= lp then
                        isUnowned = false
                    end
                end
            end)
        end

        if not isUnowned then
            skipped = skipped + 1
            continue
        end

        -- Calculate pull direction toward player
        local entPos = ent:GetPos()
        local dir = (lpPos - entPos):GetNormalized()
        local pullForce = dir * 600
        local pulled = false

        -- Method 1: Physics velocity
        pcall(function()
            if ent.GetPhysicsObject then
                local phys = ent:GetPhysicsObject()
                if IsValid(phys) then
                    phys:EnableMotion(true)
                    phys:Wake()
                    phys:SetVelocity(pullForce)
                    pulled = true
                end
            end
        end)

        -- Method 2: Direct SetPos nudge if no physics
        if not pulled then
            pcall(function()
                if ent.SetPos then
                    ent:SetPos(entPos + dir * 10)
                    pulled = true
                end
            end)
        end

        if pulled then
            affected = affected + 1
            local dist = math.floor(lpPos:Distance(entPos))
            table.insert(details, cls .. " [#" .. ent:EntIndex() .. "] " .. dist .. "u")
        else
            errors = errors + 1
        end
    end

    r = "MASS GRAB REPORT\n"
    r = r .. "Radius: " .. radius .. "u\n"
    r = r .. "Pulled: " .. affected .. " entities\n"
    r = r .. "Skipped (owned): " .. skipped .. "\n"
    r = r .. "Errors: " .. errors .. "\n"

    if #details > 0 then
        r = r .. "\nAFFECTED:\n"
        -- Cap display at 30 entries to avoid huge output
        local showCount = math.min(#details, 30)
        for i = 1, showCount do
            r = r .. "  " .. details[i] .. "\n"
        end
        if #details > showCount then
            r = r .. "  ... and " .. (#details - showCount) .. " more\n"
        end
    end
end)
return r
)lua";

} // namespace luascripts
