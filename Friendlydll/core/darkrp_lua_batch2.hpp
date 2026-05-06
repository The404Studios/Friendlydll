#pragma once

namespace luascripts {

	// -----------------------------------------------------------------------
	// 4. Auto-Bounty -- auto place hit on attacker when you die
	// -----------------------------------------------------------------------
	inline const char* LUA_AUTO_BOUNTY_SETUP = R"lua(
pcall(function()
    if _fdll_auto_bounty_installed then return end
    _fdll_auto_bounty_installed = true
    _fdll_bounty_lasttime = _fdll_bounty_lasttime or 0

    hook.Add("PlayerDeath", "fdll_auto_bounty", function(victim, inflictor, attacker)
        pcall(function()
            if not IsValid(victim) or not IsValid(attacker) then return end
            if victim ~= LocalPlayer() then return end
            if not attacker:IsPlayer() then return end
            if attacker == LocalPlayer() then return end

            if CurTime() < _fdll_bounty_lasttime + 30 then return end
            _fdll_bounty_lasttime = CurTime()

            local attackerName = attacker:Nick() or "unknown"
            LocalPlayer():ConCommand("say /hit " .. attackerName)

            chat.AddText(
                Color(255, 165, 0), "[BOUNTY] ",
                Color(255, 255, 255), "Hit placed on " .. attackerName
            )
        end)
    end)
end)
)lua";

	inline const char* LUA_AUTO_BOUNTY_STOP = R"lua(
pcall(function()
    _fdll_auto_bounty_installed = false
    _fdll_bounty_lasttime = 0
    hook.Remove("PlayerDeath", "fdll_auto_bounty")
end)
)lua";

	// -----------------------------------------------------------------------
	// 5. Door Auto-Close -- auto close your fading doors after 3 seconds
	// -----------------------------------------------------------------------
	inline const char* LUA_DOOR_AUTOCLOSE_SETUP = R"lua(
pcall(function()
    if _fdll_door_autoclose_installed then return end
    _fdll_door_autoclose_installed = true
    _fdll_door_open_times = _fdll_door_open_times or {}

    timer.Create("fdll_door_autoclose", 1, 0, function()
        pcall(function()
            local lp = LocalPlayer()
            if not IsValid(lp) or not lp:Alive() then return end

            for _, ent in ipairs(ents.GetAll()) do
                pcall(function()
                    if not IsValid(ent) then return end
                    if not ent.isFadingDoor then return end

                    -- check ownership via multiple methods
                    local isOwner = false
                    local ok1, r1 = pcall(function() return ent:CPPIGetOwner() == lp end)
                    if ok1 and r1 then isOwner = true end
                    if not isOwner then
                        local ok2, r2 = pcall(function() return ent:Getowning_ent() == lp end)
                        if ok2 and r2 then isOwner = true end
                    end
                    if not isOwner then return end

                    -- check if door is currently open (faded/invisible)
                    local isOpen = false
                    local ok3, r3 = pcall(function() return ent:GetDTBool(0) end)
                    if ok3 and r3 == true then
                        isOpen = true
                    end
                    if not isOpen then
                        local ok4, r4 = pcall(function()
                            local mat = ent:GetMaterial()
                            return mat and (mat == "sprites/heatwave" or string.find(mat, "invisible"))
                        end)
                        if ok4 and r4 then isOpen = true end
                    end

                    local idx = ent:EntIndex()
                    if isOpen then
                        if not _fdll_door_open_times[idx] then
                            _fdll_door_open_times[idx] = CurTime()
                        elseif CurTime() - _fdll_door_open_times[idx] > 3 then
                            -- try to close: attempt numpad activation for keys 1-9
                            pcall(function()
                                local kv = ent:GetKeyValues()
                                if kv and kv["key"] then
                                    local key = tonumber(kv["key"])
                                    if key and numpad and numpad.Activate then
                                        numpad.Activate(lp, key)
                                        timer.Simple(0.3, function()
                                            pcall(function()
                                                if numpad and numpad.Deactivate then
                                                    numpad.Deactivate(lp, key)
                                                end
                                            end)
                                        end)
                                    end
                                end
                            end)
                            -- fallback: try toggling via use
                            pcall(function()
                                if ent.Fire then
                                    ent:Fire("Use", "", 0)
                                end
                            end)
                            _fdll_door_open_times[idx] = nil
                        end
                    else
                        _fdll_door_open_times[idx] = nil
                    end
                end)
            end
        end)
    end)
end)
)lua";

	inline const char* LUA_DOOR_AUTOCLOSE_STOP = R"lua(
pcall(function()
    _fdll_door_autoclose_installed = false
    _fdll_door_open_times = nil
    timer.Remove("fdll_door_autoclose")
end)
)lua";

	// -----------------------------------------------------------------------
	// 6. Proximity Alert -- warn when players approach your position
	// -----------------------------------------------------------------------
	inline const char* LUA_PROXIMITY_ALERT_SETUP = R"lua(
pcall(function()
    if _fdll_proximity_installed then return end
    _fdll_proximity_installed = true
    _fdll_nearby_players = _fdll_nearby_players or {}
    _fdll_alerted_players = _fdll_alerted_players or {}

    local compassDirs = {"N","NE","E","SE","S","SW","W","NW"}

    local function getCompass(myPos, myYaw, theirPos)
        local dx = theirPos.x - myPos.x
        local dy = theirPos.y - myPos.y
        -- World angle to target (Source: 0=east, 90=north)
        local worldAng = math.deg(math.atan2(dy, dx))
        -- Relative to view: how far CW from where we're looking
        local rel = (worldAng - myYaw + 360) % 360
        -- Map 0-360 into 8 compass bins (0=forward=N on radar)
        local idx = math.floor((rel + 22.5) / 45) % 8 + 1
        return compassDirs[idx], rel
    end

    hook.Add("Think", "fdll_proximity_alert", function()
        pcall(function()
            local lp = LocalPlayer()
            if not IsValid(lp) or not lp:Alive() then
                _fdll_nearby_players = {}
                return
            end

            local myPos = lp:GetPos()
            local myYaw = lp:EyeAngles().y
            local nearby = {}
            local currentNear = {}

            for _, ply in ipairs(player.GetAll()) do
                pcall(function()
                    if ply == lp then return end
                    if not IsValid(ply) or not ply:Alive() then return end

                    local theirPos = ply:GetPos()
                    local dist = theirPos:Distance(myPos)
                    if dist < 600 then
                        local name = ply:Nick() or "unknown"
                        local compass, relAngle = getCompass(myPos, myYaw, theirPos)

                        local approaching = false
                        pcall(function()
                            local vel = ply:GetVelocity()
                            if vel:LengthSqr() > 100 then
                                local toMe = (myPos - theirPos):GetNormalized()
                                approaching = vel:GetNormalized():Dot(toMe) > 0.5
                            end
                        end)

                        nearby[#nearby + 1] = {
                            name = name,
                            dist = math.floor(dist),
                            compass = compass,
                            angle = relAngle,
                            closing = approaching
                        }
                        currentNear[name] = true

                        if not _fdll_alerted_players[name] then
                            _fdll_alerted_players[name] = true
                            surface.PlaySound("buttons/lightswitch2.wav")
                        end
                    end
                end)
            end

            for name, _ in pairs(_fdll_alerted_players) do
                if not currentNear[name] then
                    _fdll_alerted_players[name] = nil
                end
            end

            table.sort(nearby, function(a, b) return a.dist < b.dist end)
            _fdll_nearby_players = nearby
        end)
    end)

    hook.Add("HUDPaint", "fdll_proximity_hud", function()
        pcall(function()
            if not _fdll_nearby_players or #_fdll_nearby_players == 0 then return end

            local pulse = math.sin(CurTime() * 3) * 30 + 200
            local alpha = math.Clamp(math.floor(pulse), 0, 255)

            local panelW = 280
            local lineH = 18
            local headerH = 26
            local panelH = headerH + #_fdll_nearby_players * lineH + 6
            local x = ScrW() - panelW - 12
            local y = 12

            draw.RoundedBox(6, x, y, panelW, panelH, Color(20, 20, 20, alpha))
            draw.RoundedBox(6, x, y, panelW, headerH, Color(200, 80, 0, alpha))
            draw.SimpleText("PROXIMITY ALERT", "DermaDefaultBold",
                x + panelW / 2, y + headerH / 2,
                Color(255, 255, 255, alpha), TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER)

            for i, info in ipairs(_fdll_nearby_players) do
                local ly = y + headerH + (i - 1) * lineH + 3
                -- Compass direction
                local dirCol = info.closing and Color(255, 80, 80, alpha) or Color(100, 200, 255, alpha)
                draw.SimpleText(info.compass, "DermaDefaultBold",
                    x + 8, ly, dirCol, TEXT_ALIGN_LEFT)
                -- Name (red if approaching)
                local nameCol = info.closing and Color(255, 100, 100, alpha) or Color(255, 200, 100, alpha)
                draw.SimpleText(info.name, "DermaDefault",
                    x + 32, ly, nameCol, TEXT_ALIGN_LEFT)
                -- Distance + approach indicator
                local distTxt = info.dist .. "u"
                if info.closing then distTxt = distTxt .. " >>" end
                draw.SimpleText(distTxt, "DermaDefault",
                    x + panelW - 8, ly, Color(180, 180, 180, alpha), TEXT_ALIGN_RIGHT)
            end
        end)
    end)
end)
)lua";

	inline const char* LUA_PROXIMITY_ALERT_STOP = R"lua(
pcall(function()
    _fdll_proximity_installed = false
    _fdll_nearby_players = nil
    _fdll_alerted_players = nil
    hook.Remove("Think", "fdll_proximity_alert")
    hook.Remove("HUDPaint", "fdll_proximity_hud")
end)
)lua";

} // namespace luascripts
