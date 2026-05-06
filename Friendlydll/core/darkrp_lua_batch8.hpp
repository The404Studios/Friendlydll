#pragma once
#include "../includes.hpp"

namespace luascripts {

	// -----------------------------------------------------------------------
	// 16. Salary Exploit -- speed up DarkRP payday timer for rapid income
	// -----------------------------------------------------------------------
	inline const char* LUA_SALARY_EXPLOIT_SETUP = R"lua(
pcall(function()
    if _fdll_salary_exploit_installed then return end
    _fdll_salary_exploit_installed = true
    _fdll_salary_income_total = _fdll_salary_income_total or 0
    _fdll_salary_income_tick = _fdll_salary_income_tick or 0
    _fdll_salary_start_time = CurTime()
    _fdll_salary_last_balance = 0
    _fdll_salary_accelerated = false

    -- Grab initial balance
    pcall(function()
        local lp = LocalPlayer()
        if IsValid(lp) and lp.getDarkRPVar then
            _fdll_salary_last_balance = tonumber(lp:getDarkRPVar("money")) or 0
        end
    end)

    -- Method 1: Adjust the DarkRP_PayDay timer to fire every 1 second
    pcall(function()
        if timer.Exists("DarkRP_PayDay") then
            timer.Adjust("DarkRP_PayDay", 1, 0)
            _fdll_salary_accelerated = true
        end
    end)

    -- Method 2: Try alternate timer names used by different DarkRP versions
    pcall(function()
        local timerNames = {
            "payday", "PayDay", "darkrp_payday", "DarkRP PayDay",
            "DarkRPPayday", "salary", "SalaryTimer", "paycheck",
            "darkrp_salary", "DarkRP_Salary", "DarkRP_paycheck"
        }
        for _, tname in ipairs(timerNames) do
            if timer.Exists(tname) then
                timer.Adjust(tname, 1, 0)
                _fdll_salary_accelerated = true
            end
        end
    end)

    -- Method 3: Try setting GAMEMODE.Config.payaliday to 1 second
    pcall(function()
        if GAMEMODE and GAMEMODE.Config then
            GAMEMODE.Config.payaliday = 1
        end
    end)

    -- Method 4: Try DarkRP.setConfig if available
    pcall(function()
        if DarkRP and DarkRP.setConfig then
            DarkRP.setConfig("payaliday", 1)
        end
    end)

    -- Method 5: Try manipulating the config table directly
    pcall(function()
        if GAMEMODE and GAMEMODE.Config then
            GAMEMODE.Config.paydelay = 1
        end
    end)

    -- Method 6: Scan timer list via debug library for payday-related timers
    pcall(function()
        if timer.GetTable then
            local timers = timer.GetTable()
            if timers then
                for name, data in pairs(timers) do
                    local lname = string.lower(tostring(name))
                    if string.find(lname, "pay") or string.find(lname, "salary")
                       or string.find(lname, "income") or string.find(lname, "wage") then
                        timer.Adjust(tostring(name), 1, 0)
                        _fdll_salary_accelerated = true
                    end
                end
            end
        end
    end)

    -- Method 7: Create our own payday timer if we couldn't find the original
    pcall(function()
        if not _fdll_salary_accelerated then
            timer.Create("fdll_force_payday", 2, 0, function()
                if not _fdll_salary_exploit_installed then return end
                pcall(function()
                    RunConsoleCommand("say", "/payday")
                end)
                pcall(function()
                    local lp = LocalPlayer()
                    if IsValid(lp) and lp.ConCommand then
                        lp:ConCommand("darkrp payday")
                    end
                end)
                pcall(function()
                    if DarkRP and DarkRP.payDay then
                        DarkRP.payDay()
                    end
                end)
            end)
        end
    end)

    -- Track balance changes to measure income rate
    hook.Add("Think", "fdll_salary_tracker", function()
        if not _fdll_salary_exploit_installed then return end
        local lp = LocalPlayer()
        if not IsValid(lp) then return end

        pcall(function()
            if lp.getDarkRPVar then
                local currentMoney = tonumber(lp:getDarkRPVar("money")) or 0
                if currentMoney > _fdll_salary_last_balance then
                    local gained = currentMoney - _fdll_salary_last_balance
                    _fdll_salary_income_total = _fdll_salary_income_total + gained
                    _fdll_salary_income_tick = _fdll_salary_income_tick + gained
                end
                _fdll_salary_last_balance = currentMoney
            end
        end)
    end)

    -- Reset per-minute tick counter every 60 seconds
    timer.Create("fdll_salary_tick_reset", 60, 0, function()
        _fdll_salary_income_tick = 0
    end)

    -- HUD: show income rate and acceleration status
    hook.Add("HUDPaint", "fdll_salary_hud", function()
        if not _fdll_salary_exploit_installed then
            hook.Remove("HUDPaint", "fdll_salary_hud")
            return
        end

        local elapsed = math.max(CurTime() - _fdll_salary_start_time, 1)
        local perMin = math.floor((_fdll_salary_income_total / elapsed) * 60)

        local statusText = _fdll_salary_accelerated and "(ACCELERATED)" or "(monitoring)"
        local statusColor = _fdll_salary_accelerated and Color(0, 255, 0) or Color(255, 200, 0)

        local x = ScrW() - 300
        local y = ScrH() / 2 - 40

        draw.RoundedBox(6, x - 10, y - 5, 290, 55, Color(0, 0, 0, 180))

        draw.SimpleText(
            "Income: $" .. string.Comma(perMin) .. "/min " .. statusText,
            "DermaDefault",
            x, y,
            statusColor,
            TEXT_ALIGN_LEFT, TEXT_ALIGN_TOP
        )

        draw.SimpleText(
            "Total Earned: $" .. string.Comma(math.floor(_fdll_salary_income_total)),
            "DermaDefault",
            x, y + 18,
            Color(255, 255, 255),
            TEXT_ALIGN_LEFT, TEXT_ALIGN_TOP
        )

        -- Salary info
        local salaryAmt = 0
        pcall(function()
            local lp = LocalPlayer()
            if IsValid(lp) and lp.getDarkRPVar then
                salaryAmt = tonumber(lp:getDarkRPVar("salary")) or 0
            end
        end)
        if salaryAmt > 0 then
            draw.SimpleText(
                "Salary: $" .. string.Comma(salaryAmt) .. "/tick",
                "DermaDefault",
                x, y + 36,
                Color(200, 200, 200),
                TEXT_ALIGN_LEFT, TEXT_ALIGN_TOP
            )
        end
    end)
end)
)lua";

	inline const char* LUA_SALARY_EXPLOIT_STOP = R"lua(
pcall(function()
    _fdll_salary_exploit_installed = false
    _fdll_salary_accelerated = false

    hook.Remove("Think", "fdll_salary_tracker")
    hook.Remove("HUDPaint", "fdll_salary_hud")
    timer.Remove("fdll_salary_tick_reset")
    timer.Remove("fdll_force_payday")

    -- Attempt to restore default payday interval (default is usually 120 seconds)
    pcall(function()
        if timer.Exists("DarkRP_PayDay") then
            timer.Adjust("DarkRP_PayDay", 120, 0)
        end
    end)
    pcall(function()
        if GAMEMODE and GAMEMODE.Config then
            GAMEMODE.Config.payaliday = 120
            GAMEMODE.Config.paydelay = 120
        end
    end)
end)
)lua";

	// -----------------------------------------------------------------------
	// 17. Printer Overclock -- speed up owned money printer generation
	// -----------------------------------------------------------------------
	inline const char* LUA_PRINTER_OVERCLOCK_SETUP = R"lua(
pcall(function()
    if _fdll_printer_oc_installed then return end
    _fdll_printer_oc_installed = true
    _fdll_printer_oc_bonus = _fdll_printer_oc_bonus or 0
    _fdll_printer_oc_count = 0
    _fdll_printer_oc_next = 0

    local function isMyPrinter(ent)
        local lp = LocalPlayer()
        if not IsValid(lp) or not IsValid(ent) then return false end

        local cls = string.lower(ent:GetClass() or "")
        local isPrinter = string.find(cls, "printer") ~= nil
            or string.find(cls, "money") ~= nil
            or cls == "money_printer"
            or cls == "golden_printer"
            or cls == "diamond_printer"
            or cls == "silver_printer"
            or cls == "bronze_printer"

        if not isPrinter then return false end

        local isOwner = false
        pcall(function()
            if ent.CPPIGetOwner then
                local ow = ent:CPPIGetOwner()
                if IsValid(ow) and ow == lp then isOwner = true end
            end
        end)
        if not isOwner then
            pcall(function()
                if ent.Getowning_ent then
                    local ow = ent:Getowning_ent()
                    if IsValid(ow) and ow == lp then isOwner = true end
                end
            end)
        end
        if not isOwner then
            pcall(function()
                local ow = ent:GetNWEntity("owning_ent", NULL)
                if IsValid(ow) and ow == lp then isOwner = true end
            end)
        end

        return isOwner
    end

    local function overclockPrinter(ent)
        if not IsValid(ent) then return end

        -- Method 1: Set NextPrint to now so it prints immediately
        pcall(function()
            if ent.NextPrint then
                ent.NextPrint = CurTime()
            end
        end)

        -- Method 2: Try setting dt table values for faster printing
        pcall(function()
            if ent.dt then
                if ent.dt.NextPrint ~= nil then ent.dt.NextPrint = CurTime() end
                if ent.dt.PrintSpeed ~= nil then ent.dt.PrintSpeed = 0.1 end
                if ent.dt.Speed ~= nil then ent.dt.Speed = 10 end
            end
        end)

        -- Method 3: SetNW values related to print timing
        pcall(function()
            ent:SetNWFloat("NextPrint", CurTime())
            ent:SetNWFloat("PrintTime", 0.5)
            ent:SetNWInt("PrintSpeed", 10)
        end)

        -- Method 4: Fire input to speed up
        pcall(function()
            ent:Fire("SetSpeed", "10")
        end)

        -- Method 5: Call CreateMoney if it exists
        pcall(function()
            if ent.CreateMoney then
                ent:CreateMoney()
                _fdll_printer_oc_bonus = _fdll_printer_oc_bonus + 1
            end
        end)

        -- Method 6: Call PrintMoney or similar functions
        pcall(function()
            if ent.PrintMoney then ent:PrintMoney() end
        end)
        pcall(function()
            if ent.CreateMoneyBag then ent:CreateMoneyBag() end
        end)
        pcall(function()
            if ent.SpawnMoney then ent:SpawnMoney() end
        end)

        -- Method 7: Modify datatable floats/ints that might control speed
        pcall(function()
            for i = 0, 15 do
                pcall(function()
                    local val = ent:GetDTFloat(i)
                    if val and val > CurTime() then
                        ent:SetDTFloat(i, CurTime())
                    end
                end)
            end
        end)

        -- Method 8: Reduce the entity's internal timer interval
        pcall(function()
            if ent.SetPrintSpeed then ent:SetPrintSpeed(0.1) end
        end)
        pcall(function()
            if ent.SetSpeed then ent:SetSpeed(10) end
        end)
    end

    -- Think hook: re-apply overclock every 0.5 seconds
    hook.Add("Think", "fdll_printer_overclock", function()
        if not _fdll_printer_oc_installed then return end
        local now = CurTime()
        if now < _fdll_printer_oc_next then return end
        _fdll_printer_oc_next = now + 0.5

        local lp = LocalPlayer()
        if not IsValid(lp) or not lp:Alive() then return end

        _fdll_printer_oc_count = 0

        for _, ent in ipairs(ents.GetAll()) do
            if IsValid(ent) then
                local ok, isMine = pcall(isMyPrinter, ent)
                if ok and isMine then
                    _fdll_printer_oc_count = _fdll_printer_oc_count + 1
                    pcall(overclockPrinter, ent)
                end
            end
        end
    end)

    -- HUD: show overclock status
    hook.Add("HUDPaint", "fdll_printer_oc_hud", function()
        if not _fdll_printer_oc_installed then
            hook.Remove("HUDPaint", "fdll_printer_oc_hud")
            return
        end

        local x = ScrW() - 300
        local y = ScrH() / 2 + 25

        draw.RoundedBox(6, x - 10, y - 5, 290, 38, Color(0, 0, 0, 180))

        local printerColor = _fdll_printer_oc_count > 0 and Color(0, 255, 100) or Color(255, 100, 100)
        draw.SimpleText(
            "Printers OC: " .. _fdll_printer_oc_count .. " | Bonus cycles: " .. _fdll_printer_oc_bonus,
            "DermaDefault",
            x, y,
            printerColor,
            TEXT_ALIGN_LEFT, TEXT_ALIGN_TOP
        )

        draw.SimpleText(
            "Overclock: ACTIVE (0.5s tick)",
            "DermaDefault",
            x, y + 16,
            Color(0, 200, 255),
            TEXT_ALIGN_LEFT, TEXT_ALIGN_TOP
        )
    end)
end)
)lua";

	inline const char* LUA_PRINTER_OVERCLOCK_STOP = R"lua(
pcall(function()
    _fdll_printer_oc_installed = false
    _fdll_printer_oc_count = 0
    hook.Remove("Think", "fdll_printer_overclock")
    hook.Remove("HUDPaint", "fdll_printer_oc_hud")
end)
)lua";

	// -----------------------------------------------------------------------
	// 18. Shop Exploit Scanner -- find exploitable purchases and pricing bugs
	// -----------------------------------------------------------------------
	inline const char* LUA_SHOP_EXPLOIT = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "ERROR: No local player" return end

    local results = {}
    local negativePrice = {}
    local profitItems = {}
    local moneyGens = {}
    local pricingBugs = {}

    -- Scan DarkRP categories for items
    pcall(function()
        if DarkRP and DarkRP.getCategories then
            local cats = DarkRP.getCategories()
            if type(cats) == "table" then
                for catName, catData in pairs(cats) do
                    if type(catData) == "table" and catData.members then
                        for _, item in ipairs(catData.members) do
                            if type(item) == "table" then
                                local name = item.name or item.Name or "Unknown"
                                local price = tonumber(item.price or item.Price or item.cost or 0) or 0
                                local sell = tonumber(item.sellPrice or item.SellPrice or 0) or 0
                                if price < 0 then
                                    table.insert(negativePrice, name .. " ($" .. price .. ") FREE MONEY")
                                end
                                if sell > price and price > 0 then
                                    local profit = sell - price
                                    table.insert(profitItems, name .. " buy:$" .. price .. " sell:$" .. sell .. " profit:$" .. profit)
                                end
                            end
                        end
                    end
                end
            end
        end
    end)

    -- Scan CustomShipments table
    pcall(function()
        if CustomShipments then
            for idx, ship in pairs(CustomShipments) do
                if type(ship) == "table" then
                    local name = ship.name or ship.Name or ("Shipment#" .. tostring(idx))
                    local price = tonumber(ship.price or ship.pricesep or 0) or 0
                    local amount = tonumber(ship.amount or ship.Amount or 1) or 1
                    local sepPrice = tonumber(ship.pricesep or ship.priceSep or 0) or 0
                    local sellPrice = tonumber(ship.sellPrice or 0) or 0

                    if price < 0 or sepPrice < 0 then
                        table.insert(negativePrice, name .. " ($" .. price .. "/" .. sepPrice .. ") NEGATIVE PRICE")
                    end
                    if amount > 0 and price > 0 then
                        local perUnit = price / amount
                        if sepPrice > 0 and perUnit < sepPrice then
                            local profit = (sepPrice * amount) - price
                            table.insert(profitItems, name .. " bulk:$" .. price .. "/" .. amount .. " sep:$" .. sepPrice .. " profit:$" .. profit)
                        end
                    end
                    if sellPrice > 0 and sepPrice > 0 and sellPrice > sepPrice then
                        table.insert(profitItems, name .. " buy:$" .. sepPrice .. " sell:$" .. sellPrice .. " profit:$" .. (sellPrice - sepPrice))
                    end
                end
            end
        end
    end)

    -- Scan DarkRPEntities for money generators
    pcall(function()
        if DarkRPEntities then
            for idx, ent in pairs(DarkRPEntities) do
                if type(ent) == "table" then
                    local name = ent.name or ent.Name or ("Entity#" .. tostring(idx))
                    local price = tonumber(ent.price or ent.Price or 0) or 0
                    local cls = ent.ent or ent.entity or ""

                    if price < 0 then
                        table.insert(negativePrice, name .. " ($" .. price .. ") NEGATIVE ENTITY PRICE")
                    end

                    local lowerCls = string.lower(tostring(cls))
                    local lowerName = string.lower(tostring(name))
                    if string.find(lowerCls, "printer") or string.find(lowerName, "printer")
                       or string.find(lowerCls, "money") or string.find(lowerName, "money")
                       or string.find(lowerName, "generator") or string.find(lowerName, "miner") then
                        local roi = "unknown"
                        if price > 0 then
                            roi = "$" .. price .. " cost"
                        end
                        table.insert(moneyGens, name .. " (" .. cls .. ") " .. roi)
                    end
                end
            end
        end
    end)

    -- Scan FoodItems / special items
    pcall(function()
        if FoodItems then
            for idx, food in pairs(FoodItems) do
                if type(food) == "table" then
                    local name = food.name or ("Food#" .. tostring(idx))
                    local price = tonumber(food.price or 0) or 0
                    if price < 0 then
                        table.insert(negativePrice, name .. " ($" .. price .. ") NEGATIVE FOOD")
                    end
                end
            end
        end
    end)

    -- Scan for zero-cost entities in job loadouts
    pcall(function()
        if RPExtraTeams then
            for _, job in pairs(RPExtraTeams) do
                if type(job) == "table" then
                    local salary = tonumber(job.salary or 0) or 0
                    local name = job.name or "Unknown Job"
                    if salary > 500 then
                        table.insert(results, "HIGH SALARY JOB: " .. name .. " $" .. salary .. "/tick")
                    end
                    if job.weapons and type(job.weapons) == "table" then
                        for _, wep in ipairs(job.weapons) do
                            local weaponSell = 0
                            pcall(function()
                                if CustomShipments then
                                    for _, s in pairs(CustomShipments) do
                                        if s.entity == wep and s.pricesep then
                                            weaponSell = tonumber(s.pricesep) or 0
                                        end
                                    end
                                end
                            end)
                            if weaponSell > 0 then
                                table.insert(results, "JOB WEAPON SELL: " .. name .. " spawns " .. wep .. " (sell $" .. weaponSell .. ")")
                            end
                        end
                    end
                end
            end
        end
    end)

    -- Build the report
    r = "=== SHOP EXPLOIT ANALYSIS ===\n"

    r = r .. "\n[NEGATIVE PRICES - FREE MONEY]\n"
    if #negativePrice > 0 then
        for _, v in ipairs(negativePrice) do r = r .. "  !! " .. v .. "\n" end
    else
        r = r .. "  None found\n"
    end

    r = r .. "\n[BUY/SELL ARBITRAGE]\n"
    if #profitItems > 0 then
        for _, v in ipairs(profitItems) do r = r .. "  $$ " .. v .. "\n" end
    else
        r = r .. "  None found\n"
    end

    r = r .. "\n[MONEY GENERATORS - ROI]\n"
    if #moneyGens > 0 then
        for _, v in ipairs(moneyGens) do r = r .. "  >> " .. v .. "\n" end
    else
        r = r .. "  None found\n"
    end

    r = r .. "\n[HIGH VALUE EXPLOITS]\n"
    if #results > 0 then
        for _, v in ipairs(results) do r = r .. "  ** " .. v .. "\n" end
    else
        r = r .. "  None found\n"
    end

    r = r .. "\n[PRICING BUGS]\n"
    if #pricingBugs > 0 then
        for _, v in ipairs(pricingBugs) do r = r .. "  BUG: " .. v .. "\n" end
    else
        r = r .. "  None detected\n"
    end
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 19. Money Drop Exploit -- attempt money manipulation via drop commands
	// -----------------------------------------------------------------------
	inline const char* LUA_MONEY_DROP_EXPLOIT = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "ERROR: No local player" return end

    local results = {}
    local currentMoney = 0
    pcall(function()
        if lp.getDarkRPVar then
            currentMoney = tonumber(lp:getDarkRPVar("money")) or 0
        end
    end)

    table.insert(results, "Starting balance: $" .. string.Comma(currentMoney))

    -- Test 1: Try dropping negative money
    pcall(function()
        RunConsoleCommand("say", "/dropmoney -1")
        table.insert(results, "Attempted: /dropmoney -1")
    end)

    -- Test 2: Try dropping zero
    pcall(function()
        timer.Simple(0.3, function()
            RunConsoleCommand("say", "/dropmoney 0")
        end)
        table.insert(results, "Attempted: /dropmoney 0")
    end)

    -- Test 3: Try dropping negative large amount
    pcall(function()
        timer.Simple(0.6, function()
            RunConsoleCommand("say", "/drop -1000")
        end)
        table.insert(results, "Attempted: /drop -1000")
    end)

    -- Test 4: Test integer overflow boundary
    pcall(function()
        timer.Simple(0.9, function()
            RunConsoleCommand("say", "/dropmoney 2147483647")
        end)
        table.insert(results, "Attempted: /dropmoney 2147483647 (INT_MAX)")
    end)

    -- Test 5: Try dropping more than balance
    pcall(function()
        timer.Simple(1.2, function()
            local overshoot = currentMoney + 999999
            RunConsoleCommand("say", "/dropmoney " .. tostring(overshoot))
        end)
        table.insert(results, "Attempted: /dropmoney " .. tostring(currentMoney + 999999) .. " (over balance)")
    end)

    -- Test 6: Try alternative drop commands
    pcall(function()
        timer.Simple(1.5, function()
            RunConsoleCommand("say", "/moneydrop -500")
        end)
        table.insert(results, "Attempted: /moneydrop -500")
    end)

    -- Test 7: Try darkrp_dropmoney console command directly
    pcall(function()
        timer.Simple(1.8, function()
            RunConsoleCommand("darkrp_dropmoney", "-1000")
        end)
        table.insert(results, "Attempted: darkrp_dropmoney -1000 (console)")
    end)

    -- Test 8: Try give money to self with negative
    pcall(function()
        timer.Simple(2.1, function()
            local sid = lp:SteamID()
            RunConsoleCommand("say", "/give -5000")
        end)
        table.insert(results, "Attempted: /give -5000 (self)")
    end)

    -- Delayed balance check after all attempts
    pcall(function()
        timer.Simple(3.0, function()
            local afterMoney = 0
            pcall(function()
                local lp2 = LocalPlayer()
                if IsValid(lp2) and lp2.getDarkRPVar then
                    afterMoney = tonumber(lp2:getDarkRPVar("money")) or 0
                end
            end)
            local diff = afterMoney - currentMoney
            local msg = "[MoneyDrop] Before: $" .. string.Comma(currentMoney)
                .. " After: $" .. string.Comma(afterMoney)
                .. " Diff: $" .. string.Comma(diff)
            if diff > 0 then
                msg = msg .. " EXPLOIT WORKED - GAINED MONEY"
            elseif diff < 0 then
                msg = msg .. " Lost money (normal behavior)"
            else
                msg = msg .. " No change (commands may be blocked)"
            end
            pcall(function()
                chat.AddText(Color(255, 200, 0), msg)
            end)
        end)
    end)

    r = "=== MONEY DROP EXPLOIT ===\n"
    for _, v in ipairs(results) do
        r = r .. v .. "\n"
    end
    r = r .. "\nBalance check will appear in chat after 3 seconds.\n"
    r = r .. "Watch for error messages in chat - they reveal server-side validation gaps."
end)
return r
)lua";

	// -----------------------------------------------------------------------
	// 20. Wallet Scanner -- scan all player economies and server wealth
	// -----------------------------------------------------------------------
	inline const char* LUA_WALLET_SCANNER = R"lua(
local r = ""
pcall(function()
    local lp = LocalPlayer()
    if not IsValid(lp) then r = "ERROR: No local player" return end

    local players = {}
    local totalServerMoney = 0
    local totalSalaries = 0

    -- Scan all players for economic data
    for _, ply in ipairs(player.GetAll()) do
        if IsValid(ply) then
            local data = {
                name = ply:Nick() or "Unknown",
                money = 0,
                salary = 0,
                job = "Unknown",
                team = ply:Team() or 0
            }

            -- Get money
            pcall(function()
                if ply.getDarkRPVar then
                    data.money = tonumber(ply:getDarkRPVar("money")) or 0
                end
            end)
            if data.money == 0 then
                pcall(function()
                    data.money = tonumber(ply:GetNWInt("DarkRP_Money", 0)) or 0
                end)
            end

            -- Get salary
            pcall(function()
                if ply.getDarkRPVar then
                    data.salary = tonumber(ply:getDarkRPVar("salary")) or 0
                end
            end)
            if data.salary == 0 then
                pcall(function()
                    data.salary = tonumber(ply:GetNWInt("DarkRP_Salary", 0)) or 0
                end)
            end

            -- Get job name
            pcall(function()
                if ply.getDarkRPVar then
                    local j = ply:getDarkRPVar("job")
                    if j and j ~= "" then data.job = tostring(j) end
                end
            end)
            if data.job == "Unknown" then
                pcall(function()
                    data.job = team.GetName(ply:Team()) or "Unknown"
                end)
            end

            totalServerMoney = totalServerMoney + data.money
            totalSalaries = totalSalaries + data.salary
            table.insert(players, data)
        end
    end

    -- Sort by wealth descending
    table.sort(players, function(a, b) return a.money > b.money end)

    -- Calculate entity values (printers, etc.)
    local entityValue = 0
    local printerCount = 0
    pcall(function()
        for _, ent in ipairs(ents.GetAll()) do
            if IsValid(ent) then
                local cls = string.lower(ent:GetClass() or "")
                if string.find(cls, "printer") or string.find(cls, "money") then
                    printerCount = printerCount + 1
                    -- Estimate value based on entity type
                    pcall(function()
                        local storedMoney = ent:GetNWInt("PrintedMoney", 0)
                            + ent:GetNWInt("MoneyStored", 0)
                            + ent:GetNWInt("money", 0)
                        entityValue = entityValue + storedMoney
                    end)
                end
            end
        end
    end)

    -- Calculate inequality metrics
    local richest = 0
    local poorest = 999999999
    local median = 0
    local playerCount = #players

    if playerCount > 0 then
        richest = players[1].money
        poorest = players[playerCount].money
        median = players[math.ceil(playerCount / 2)].money
    end

    -- Gini coefficient (simplified)
    local gini = 0
    pcall(function()
        if playerCount > 1 and totalServerMoney > 0 then
            local sumDiffs = 0
            for i, a in ipairs(players) do
                for j, b in ipairs(players) do
                    sumDiffs = sumDiffs + math.abs(a.money - b.money)
                end
            end
            gini = sumDiffs / (2 * playerCount * totalServerMoney)
        end
    end)

    -- Build the report
    r = "=== SERVER ECONOMY SCAN ===\n"
    r = r .. "Players: " .. playerCount .. "\n"
    r = r .. "Total Player Wealth: $" .. string.Comma(totalServerMoney) .. "\n"
    r = r .. "Total Salary/tick: $" .. string.Comma(totalSalaries) .. "\n"
    r = r .. "Money Printers Active: " .. printerCount .. "\n"
    r = r .. "Printer Stored Value: $" .. string.Comma(entityValue) .. "\n"
    r = r .. "Combined Economy: $" .. string.Comma(totalServerMoney + entityValue) .. "\n"

    r = r .. "\n--- Inequality ---\n"
    r = r .. "Richest: $" .. string.Comma(richest) .. "\n"
    r = r .. "Poorest: $" .. string.Comma(poorest) .. "\n"
    r = r .. "Median: $" .. string.Comma(median) .. "\n"
    r = r .. "Gini Coefficient: " .. string.format("%.3f", gini) .. "\n"

    if gini > 0.6 then
        r = r .. "Economy: HIGHLY UNEQUAL (exploit potential high)\n"
    elseif gini > 0.4 then
        r = r .. "Economy: MODERATELY UNEQUAL\n"
    else
        r = r .. "Economy: RELATIVELY EQUAL\n"
    end

    r = r .. "\n--- Player Wallets (by wealth) ---\n"
    for i, p in ipairs(players) do
        local marker = ""
        if p.name == lp:Nick() then marker = " << YOU" end
        r = r .. string.format("#%d  $%s  [%s]  salary:$%s  %s%s\n",
            i,
            string.Comma(p.money),
            p.job,
            string.Comma(p.salary),
            p.name,
            marker
        )
    end

    -- Money sources and sinks analysis
    r = r .. "\n--- Money Sources ---\n"
    r = r .. "Salaries: $" .. string.Comma(totalSalaries) .. "/tick across all players\n"
    r = r .. "Printers: " .. printerCount .. " active (est. value $" .. string.Comma(entityValue) .. ")\n"

    -- Check for high-salary exploitable jobs
    r = r .. "\n--- Optimal Strategy ---\n"
    local bestSalaryJob = nil
    local bestSalary = 0
    pcall(function()
        if RPExtraTeams then
            for _, job in pairs(RPExtraTeams) do
                if type(job) == "table" then
                    local salary = tonumber(job.salary or 0) or 0
                    local maxPlayers = tonumber(job.max or 999) or 999
                    local current = team.NumPlayers(job.team or 0)
                    if salary > bestSalary and (maxPlayers == 0 or current < maxPlayers) then
                        bestSalary = salary
                        bestSalaryJob = job.name or "Unknown"
                    end
                end
            end
        end
    end)
    if bestSalaryJob then
        r = r .. "Best available job: " .. bestSalaryJob .. " ($" .. string.Comma(bestSalary) .. "/tick)\n"
    end
    r = r .. "Combine with Salary Exploit for maximum income.\n"
end)
return r
)lua";

} // namespace luascripts
