#pragma once
#include "../includes.hpp"

namespace luascripts {

	// -----------------------------------------------------------------------
	// 13. Shipment Scanner -- find all DarkRP shipments on the map
	// -----------------------------------------------------------------------
	inline const char* LUA_SHIPMENT_SCANNER = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "No shipments found on map" return end
    local lpPos = lp:GetPos()

    local shipments = {}

    for _, ent in ipairs(ents.GetAll()) do
        if not IsValid(ent) then continue end
        local cls = ent:GetClass()
        local isShipment = false

        if cls == "spawned_shipment" then
            isShipment = true
        elseif string.find(string.lower(cls), "shipment") then
            isShipment = true
        end

        if isShipment then
            local ownerName = "Unknown"
            pcall(function()
                local ow = ent:Getowning_ent()
                if IsValid(ow) and ow:IsPlayer() then ownerName = ow:Nick() end
            end)
            if ownerName == "Unknown" then
                pcall(function()
                    if ent.CPPIGetOwner then
                        local ow = ent:CPPIGetOwner()
                        if IsValid(ow) and ow:IsPlayer() then ownerName = ow:Nick() end
                    end
                end)
            end

            local contents = "Unknown"
            pcall(function()
                local c = ent:GetNWString("contents", "")
                if c ~= "" then contents = c end
            end)
            if contents == "Unknown" then
                pcall(function()
                    if ent.dt and ent.dt.contents then contents = tostring(ent.dt.contents) end
                end)
            end
            if contents == "Unknown" then
                pcall(function()
                    local c = ent:GetContents()
                    if c and c ~= "" then contents = tostring(c) end
                end)
            end

            local count = 0
            pcall(function()
                count = ent:GetNWInt("count", 0)
            end)

            local pos = ent:GetPos()
            local dist = math.floor(lpPos:Distance(pos))

            table.insert(shipments, {
                cls = cls,
                owner = ownerName,
                contents = contents,
                count = count,
                dist = dist,
                x = math.floor(pos.x),
                y = math.floor(pos.y),
                z = math.floor(pos.z)
            })
        end
    end

    if #shipments == 0 then
        r = "No shipments found on map"
        return
    end

    table.sort(shipments, function(a, b) return a.dist < b.dist end)

    r = "SHIPMENTS FOUND: " .. #shipments .. "\n"
    for _, s in ipairs(shipments) do
        r = r .. s.cls .. "\t" .. s.owner .. "\t" .. s.contents .. "\t" .. tostring(s.count)
            .. "\t" .. tostring(s.dist) .. "\t" .. tostring(s.x) .. "\t" .. tostring(s.y)
            .. "\t" .. tostring(s.z) .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 14. Base Scanner -- find all doors/props owned by LocalPlayer
	// -----------------------------------------------------------------------
	inline const char* LUA_BASE_SCANNER = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "YOUR BASE: 0 doors, 0 fading doors, 0 props" return end
    local lpPos = lp:GetPos()

    local doors = {}
    local fadingDoors = {}
    local props = {}
    local other = {}

    for _, ent in ipairs(ents.GetAll()) do
        if not IsValid(ent) then continue end

        local owned = false
        pcall(function()
            if ent.CPPIGetOwner and ent:CPPIGetOwner() == lp then owned = true end
        end)
        if not owned then
            pcall(function()
                if ent.Getowning_ent and ent:Getowning_ent() == lp then owned = true end
            end)
        end
        if not owned then
            pcall(function()
                if ent.OwnedBy and ent:OwnedBy(lp) then owned = true end
            end)
        end

        if owned then
            local cls = ent:GetClass()
            local pos = ent:GetPos()
            local dist = math.floor(lpPos:Distance(pos))
            local isDoor = string.find(string.lower(cls), "door") ~= nil
            local isFading = ent.isFadingDoor == true

            if isFading then
                local state = "closed"
                pcall(function()
                    if ent:GetNWBool("FadingDoorOpen", false) then state = "open" end
                end)
                pcall(function()
                    if ent.IsOpen and ent:IsOpen() then state = "open" end
                end)
                table.insert(fadingDoors, {
                    cls = cls,
                    pos = math.floor(pos.x) .. " " .. math.floor(pos.y) .. " " .. math.floor(pos.z),
                    state = state,
                    dist = dist
                })
            elseif isDoor then
                local lockState = "unknown"
                pcall(function()
                    if ent:GetNWBool("DoorLocked", false) then
                        lockState = "locked"
                    else
                        lockState = "unlocked"
                    end
                end)
                pcall(function()
                    if ent.IsDoorLocked then
                        if ent:IsDoorLocked() then
                            lockState = "locked"
                        else
                            lockState = "unlocked"
                        end
                    end
                end)
                table.insert(doors, {
                    cls = cls,
                    pos = math.floor(pos.x) .. " " .. math.floor(pos.y) .. " " .. math.floor(pos.z),
                    lockState = lockState,
                    dist = dist
                })
            elseif string.find(cls, "prop_") then
                table.insert(props, { cls = cls, dist = dist })
            else
                table.insert(other, { cls = cls, dist = dist })
            end
        end
    end

    table.sort(doors, function(a, b) return a.dist < b.dist end)
    table.sort(fadingDoors, function(a, b) return a.dist < b.dist end)

    r = "YOUR BASE: " .. #doors .. " doors, " .. #fadingDoors .. " fading doors, " .. #props .. " props\n"

    if #doors > 0 then
        r = r .. "DOORS:\n"
        for _, d in ipairs(doors) do
            r = r .. d.cls .. "\t" .. d.pos .. "\t" .. d.lockState .. "\n"
        end
    end

    if #fadingDoors > 0 then
        r = r .. "FADING DOORS:\n"
        for _, fd in ipairs(fadingDoors) do
            r = r .. fd.cls .. "\t" .. fd.pos .. "\t" .. fd.state .. "\n"
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 15. Auto-Demote -- vote-demote the player you are looking at
	// -----------------------------------------------------------------------
	inline const char* LUA_AUTO_DEMOTE = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "ERROR: LocalPlayer invalid" return end

    if _fdll_demote_last and CurTime() - _fdll_demote_last < 60 then
        local remaining = math.ceil(60 - (CurTime() - _fdll_demote_last))
        r = "COOLDOWN: " .. remaining .. "s remaining"
        pcall(function()
            chat.AddText(Color(255, 0, 0), "[DEMOTE] ", Color(255, 255, 255),
                "Cooldown active! " .. remaining .. "s remaining")
        end)
        return
    end

    local tr = lp:GetEyeTrace()
    if not tr or not IsValid(tr.Entity) or not tr.Entity:IsPlayer() then
        r = "ERROR: Look at a player first"
        pcall(function()
            chat.AddText(Color(255, 0, 0), "[DEMOTE] ", Color(255, 255, 255),
                "Look at a player first!")
        end)
        return
    end

    local target = tr.Entity
    local targetName = target:Nick()
    local job = "Unknown"
    pcall(function()
        job = team.GetName(target:Team()) or "Unknown"
    end)
    pcall(function()
        if target.getDarkRPVar then
            local j = target:getDarkRPVar("job")
            if j and j ~= "" then job = j end
        end
    end)

    _fdll_demote_last = CurTime()

    pcall(function()
        RunConsoleCommand("say", "/demote " .. targetName .. " Breaking server rules")
    end)

    pcall(function()
        chat.AddText(
            Color(255, 165, 0), "[DEMOTE] ",
            Color(255, 255, 255), "Started vote to demote " .. targetName .. " (" .. job .. ")"
        )
    end)

    r = "DEMOTE: Voted to demote " .. targetName .. " (" .. job .. ")"
end)
return r
)lua";

} // namespace luascripts
