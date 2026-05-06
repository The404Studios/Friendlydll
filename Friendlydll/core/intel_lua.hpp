#pragma once
#include "../includes.hpp"
#include <sstream>
#include <unordered_map>
#include <string>

namespace intel_lua {

	// ---- Player data parsed from Lua result ----
	struct PlayerLuaData {
		char rpName[64];
		char job[64];
		char weapon[64];
		char weaponList[128];
		char gang[64];
		bool isAdmin;
		bool isSuperAdmin;
		int observerMode;
		int observerTarget;
		int money;
		bool isWanted;
	};

	// ---- Lua query script ----
	// Returns two sections separated by ===ENTITIES===
	// Player section: entIndex \t nick \t job \t weapon \t weaponList \t isAdmin \t isSuperAdmin \t observerMode \t observerTarget \t money \t isWanted
	// Entity section: posX \t posY \t posZ \t className \t ownerName \t type \t storedMoney

	inline const char* kIntelQuery =
		"local r=\"\" "

		"local function cn(w) "
		"local ok2,pn=pcall(function() return w:GetPrintName() or \"\" end) "
		"if not ok2 then pn=\"\" end "
		"if pn~=\"\" and string.sub(pn,1,1)==\"#\" then "
		"local s,ph=pcall(language.GetPhrase,string.sub(pn,2)) "
		"if s and ph and ph~=\"\" then pn=ph end "
		"end "
		"if pn==\"\" or string.sub(pn,1,1)==\"#\" then "
		"local ok3,cl=pcall(function() return w:GetClass() or \"\" end) "
		"pn=ok3 and cl or \"\" "
		"end "
		"pn=string.gsub(pn,\"^gmod_\",\"\") "
		"pn=string.gsub(pn,\"^weapon_\",\"\") "
		"pn=string.gsub(pn,\"^swep_\",\"\") "
		"return pn "
		"end "

		"for _,p in ipairs(player.GetAll()) do "
		"local ok,line=pcall(function() "
		"local i=p:EntIndex() "
		"local n=p:Nick() or \"\" "
		"local rn=\"\" "
		"if p.getDarkRPVar then rn=tostring(p:getDarkRPVar(\"rpname\") or \"\") end "
		"if rn==\"\" then rn=n end "
		"local j=\"\" "
		"if p.getDarkRPVar then j=tostring(p:getDarkRPVar(\"job\") or \"\") end "
		"if j==\"\" then local s,tn=pcall(team.GetName,p:Team()) if s and tn then j=tn end end "
		"local gang=\"\" "
		"if p.getDarkRPVar then gang=tostring(p:getDarkRPVar(\"gang\") or \"\") end "
		"if gang==\"\" and p.getFamily then local s,f=pcall(p.getFamily,p) if s and f then gang=tostring(f) end end "
		"local faction=\"\" "
		"if p.getDarkRPVar then "
		"local sf,fv=pcall(p.getDarkRPVar,p,\"faction\") "
		"if sf and fv and tostring(fv)~=\"\" then faction=tostring(fv) end "
		"end "
		"if faction==\"\" and p.getFaction then "
		"local sf2,fv2=pcall(p.getFaction,p) "
		"if sf2 and fv2 and tostring(fv2)~=\"\" then faction=tostring(fv2) end "
		"end "
		"local org=\"\" "
		"if p.getOrganization then "
		"local so,ov=pcall(p.getOrganization,p) "
		"if so and ov and tostring(ov)~=\"\" then org=tostring(ov) end "
		"end "
		"if org==\"\" and p.getDarkRPVar then "
		"local so2,ov2=pcall(p.getDarkRPVar,p,\"org\") "
		"if so2 and ov2 and tostring(ov2)~=\"\" then org=tostring(ov2) end "
		"end "
		"local extra=\"\" "
		"if faction~=\"\" then extra=faction end "
		"if org~=\"\" then extra=(extra~=\"\" and extra..\"/\"..org) or org end "
		"if extra~=\"\" then gang=(gang~=\"\" and gang..\" / \"..extra) or extra end "
		"rn=string.gsub(rn,\"\\t\",\" \") "
		"j=string.gsub(j,\"\\t\",\" \") "
		"gang=string.gsub(gang,\"\\t\",\" \") "
		"local aw=\"\" "
		"if IsValid(p:GetActiveWeapon()) then aw=cn(p:GetActiveWeapon()) end "
		"local wl={} "
		"for _,w in ipairs(p:GetWeapons()) do "
		"if IsValid(w) and w~=p:GetActiveWeapon() then "
		"local wn=cn(w) "
		"if wn~=\"\" then wl[#wl+1]=wn end "
		"end end "
		"local wls=table.concat(wl,\",\") "
		"local adm=(p:IsAdmin() and 1) or 0 "
		"local sadm=(p:IsSuperAdmin() and 1) or 0 "
		"local om=0 "
		"if p.GetObserverMode then om=tonumber(p:GetObserverMode()) or 0 end "
		"local ot=-1 "
		"if p.GetObserverTarget then "
		"local otE=p:GetObserverTarget() "
		"if IsValid(otE) then ot=otE:EntIndex() end "
		"end "
		"local mn=0 "
		"if p.getDarkRPVar then mn=tonumber(p:getDarkRPVar(\"money\")) or 0 end "
		"local wt=0 "
		"if p.getDarkRPVar and p:getDarkRPVar(\"wanted\") then wt=1 end "
		"return i..\"\\t\"..rn..\"\\t\"..j..\"\\t\"..aw..\"\\t\"..wls"
		"..\"\\t\"..adm..\"\\t\"..sadm..\"\\t\"..om..\"\\t\"..ot"
		"..\"\\t\"..mn..\"\\t\"..wt..\"\\t\"..gang "
		"end) "
		"if ok and line then r=r..line..\"\\n\" end "
		"end "

		"r=r..\"===ENTITIES===\\n\" "

		"local ec=0 "
		"for _,e in ipairs(ents.GetAll()) do "
		"if ec>=512 then break end "
		"local ok2,eline=pcall(function() "
		"if e:IsNPC() or e:IsPlayer() then return nil end "
		"local cls=string.lower(e:GetClass() or \"\") "
		"local tp=-1 "
		"if string.find(cls,\"printer\") and not string.find(cls,\"npc\") then tp=0 "
		"elseif string.find(cls,\"shipment\") and not string.find(cls,\"npc\") then tp=1 "
		"elseif string.find(cls,\"drug\") or string.find(cls,\"meth\") "
		"or string.find(cls,\"weed\") or string.find(cls,\"cocaine\") then tp=2 "
		"elseif string.find(cls,\"bitcoin\") or string.find(cls,\"miner\") then tp=2 "
		"elseif string.find(cls,\"door\") or string.find(cls,\"prop_door\") then "
		"local dOwn=nil "
		"if e.getDoorOwner then local sd,dv=pcall(e.getDoorOwner,e) if sd and IsValid(dv) then dOwn=dv end end "
		"if not dOwn and e.getKeysOwner then local sk,kv=pcall(e.getKeysOwner,e) if sk and IsValid(kv) then dOwn=kv end end "
		"if dOwn then tp=3 end "
		"elseif cls==\"spawned_weapon\" or (string.sub(cls,1,7)==\"weapon_\" and (not e.GetOwner or not IsValid(e:GetOwner()))) then tp=4 "
		"elseif cls==\"spawned_money\" or cls==\"darkrp_money\" or cls==\"money_bag\" "
		"or cls==\"dropped_money\" or cls==\"printer_money\" then tp=5 "
		"elseif string.find(cls,\"prop_vehicle\") or string.find(cls,\"simfphys\") "
		"or string.find(cls,\"lvs_\") or string.find(cls,\"gmod_sent_vehicle\") then tp=6 "
		"end "
		"if tp<0 then return nil end "
		"local pos=e:GetPos() "
		"local own=\"\" "
		"if e.Getowning_ent and IsValid(e:Getowning_ent()) then "
		"own=e:Getowning_ent():Nick() or \"\" "
		"elseif e.getDoorOwner then "
		"local d=e:getDoorOwner() "
		"if IsValid(d) then own=d:Nick() or \"\" end "
		"elseif e.getKeysOwner then "
		"local d2=e:getKeysOwner() "
		"if IsValid(d2) then own=d2:Nick() or \"\" end end "
		"local sm=0 "
		"if tp==0 then "
		"if e.GetStoredMoney then local s2,mv=pcall(e.GetStoredMoney,e) if s2 and mv then sm=tonumber(mv) or 0 end end "
		"if sm==0 and e.GetMoney then local s2,mv=pcall(e.GetMoney,e) if s2 and mv then sm=tonumber(mv) or 0 end end "
		"if sm==0 and e.dt and e.dt.StoredMoney then sm=tonumber(e.dt.StoredMoney) or 0 end "
		"if sm==0 then sm=tonumber(e:GetNWInt(\"StoredMoney\",0)) or 0 end "
		"if sm==0 then sm=tonumber(e:GetNWInt(\"money\",0)) or 0 end "
		"else "
		"if e.GetMoney then local s2,mv=pcall(e.GetMoney,e) if s2 and mv then sm=tonumber(mv) or 0 end end "
		"if sm==0 then sm=tonumber(e:GetNWInt(\"money\",0)) or 0 end "
		"end "
		"local lbl=e:GetClass() "
		"lbl=string.gsub(lbl,\"^nut_\",\"\") "
		"lbl=string.gsub(lbl,\"^darkrp_\",\"\") "
		"lbl=string.gsub(lbl,\"^spawned_\",\"\") "
		"if e.GetPrintName then local sp0,pv0=pcall(e.GetPrintName,e) if sp0 and pv0 and pv0~=\"\" and string.sub(pv0,1,1)~=\"#\" then lbl=pv0 end end "
		"if tp==1 then "
		"local cont=\"\" "
		"local cnt=0 "
		"if e.dt and e.dt.contents then cont=tostring(e.dt.contents) end "
		"if cont==\"\" and e.Getcontents then local sc,cv=pcall(e.Getcontents,e) if sc and cv then cont=tostring(cv) end end "
		"if cont==\"\" then local nwc=e:GetNWString(\"contents\",\"\") if nwc~=\"\" then cont=nwc end end "
		"if cont==\"\" and e.GetItemName then local si,iv=pcall(e.GetItemName,e) if si and iv and iv~=\"\" then cont=tostring(iv) end end "
		"if cont==\"\" and e.GetName and e.GetName~=e.GetClass then local sn,nv=pcall(e.GetName,e) if sn and nv and nv~=\"\" then cont=tostring(nv) end end "
		"if e.dt and e.dt.count then cnt=tonumber(e.dt.count) or 0 end "
		"if cnt==0 and e.Getcount then local sc2,cv2=pcall(e.Getcount,e) if sc2 and cv2 then cnt=tonumber(cv2) or 0 end end "
		"if cnt==0 and e.Getamount then local sc3,cv3=pcall(e.Getamount,e) if sc3 and cv3 then cnt=tonumber(cv3) or 0 end end "
		"if cnt==0 then cnt=tonumber(e:GetNWInt(\"count\",0)) or 0 end "
		"if cnt==0 then cnt=tonumber(e:GetNWInt(\"amount\",0)) or 0 end "
		"if cont~=\"\" then "
		"cont=string.gsub(cont,\"^weapon_\",\"\") "
		"if cnt>0 then lbl=cont..\" x\"..cnt "
		"else lbl=cont end "
		"elseif cnt>0 then lbl=lbl..\" (x\"..cnt..\")\" end "
		"end "
		"local hp=0 local mhp=0 "
		"local shp,hpv=pcall(e.Health,e) if shp and hpv then hp=tonumber(hpv) or 0 end "
		"local smhp,mhpv=pcall(e.GetMaxHealth,e) if smhp and mhpv then mhp=tonumber(mhpv) or 0 end "
		"if tp==4 then "
		"local pn=\"\" "
		"local spn,pnv=pcall(e.GetPrintName,e) if spn and pnv and pnv~=\"\" then pn=tostring(pnv) end "
		"if pn~=\"\" and string.sub(pn,1,1)~=\"#\" then lbl=pn else lbl=cls end "
		"lbl=string.gsub(lbl,\"^weapon_\",\"\") "
		"end "
		"if tp==5 then "
		"local amt=0 "
		"if e.GetAmount then local sa,av=pcall(e.GetAmount,e) if sa and av then amt=tonumber(av) or 0 end end "
		"if amt==0 then amt=tonumber(e:GetNWInt(\"Amount\",0)) or 0 end "
		"if amt==0 then amt=tonumber(e:GetNWInt(\"amount\",0)) or 0 end "
		"if amt>0 then sm=amt end "
		"lbl=\"Money\" "
		"end "
		"if tp==6 then "
		"local vn=\"\" "
		"if e.GetVehicleName then local sv,vv=pcall(e.GetVehicleName,e) if sv and vv then vn=tostring(vv) end end "
		"if vn==\"\" then vn=cls end "
		"vn=string.gsub(vn,\"prop_vehicle_\",\"\") "
		"local dr=\"\" "
		"if e.GetDriver and IsValid(e:GetDriver()) then dr=e:GetDriver():Nick() end "
		"if dr~=\"\" then own=dr lbl=vn..\" [occupied]\" else lbl=vn end "
		"end "
		"return e:EntIndex()..\"\\t\"..math.floor(pos.x)..\"\\t\"..math.floor(pos.y)..\"\\t\"..math.floor(pos.z)"
		"..\"\\t\"..lbl..\"\\t\"..own..\"\\t\"..tp..\"\\t\"..sm..\"\\t\"..hp..\"\\t\"..mhp "
		"end) "
		"if ok2 and eline then r=r..eline..\"\\n\" ec=ec+1 end "
		"end "

		"return r";

	// ---- Helpers ----

	inline int SafeAtoi(const std::string& s) {
		if (s.empty()) return 0;
		return std::atoi(s.c_str());
	}

	inline float SafeAtof(const std::string& s) {
		if (s.empty()) return 0.f;
		return static_cast<float>(std::atof(s.c_str()));
	}

	// ---- Main query + parse ----

	inline void RunIntelQuery(std::unordered_map<int, PlayerLuaData>& playerData) {
		auto result = lualoader::ExecuteAndGetResult(kIntelQuery);
		if (result.empty()) return;

		// Split on ===ENTITIES===
		const std::string separator = "===ENTITIES===";
		size_t sepPos = result.find(separator);
		std::string playerSection, entitySection;
		if (sepPos != std::string::npos) {
			playerSection = result.substr(0, sepPos);
			entitySection = result.substr(sepPos + separator.size());
		} else {
			playerSection = result;
		}

		// ---- Parse player lines ----
		playerData.clear();
		{
			std::istringstream ss(playerSection);
			std::string line;
			while (std::getline(ss, line)) {
				if (line.empty()) continue;

				// entIndex \t rpName \t job \t weapon \t weaponList \t isAdmin \t isSuperAdmin \t observerMode \t observerTarget \t money \t isWanted \t gang
				size_t t[11];
				t[0] = line.find('\t');
				if (t[0] == std::string::npos) continue;
				for (int i = 1; i < 11; ++i) {
					t[i] = line.find('\t', t[i - 1] + 1);
					if (t[i] == std::string::npos) break;
				}
				if (t[10] == std::string::npos) continue;

				int idx = SafeAtoi(line.substr(0, t[0]));
				auto& d = playerData[idx];

				strncpy_s(d.rpName,     line.substr(t[0] + 1, t[1] - t[0] - 1).c_str(), 63);
				strncpy_s(d.job,        line.substr(t[1] + 1, t[2] - t[1] - 1).c_str(), 63);
				strncpy_s(d.weapon,     line.substr(t[2] + 1, t[3] - t[2] - 1).c_str(), 63);
				strncpy_s(d.weaponList, line.substr(t[3] + 1, t[4] - t[3] - 1).c_str(), 127);

				d.isAdmin       = SafeAtoi(line.substr(t[4] + 1, t[5] - t[4] - 1)) != 0;
				d.isSuperAdmin  = SafeAtoi(line.substr(t[5] + 1, t[6] - t[5] - 1)) != 0;
				d.observerMode  = SafeAtoi(line.substr(t[6] + 1, t[7] - t[6] - 1));
				d.observerTarget= SafeAtoi(line.substr(t[7] + 1, t[8] - t[7] - 1));
				d.money         = SafeAtoi(line.substr(t[8] + 1, t[9] - t[8] - 1));
				d.isWanted      = SafeAtoi(line.substr(t[9] + 1, t[10] - t[9] - 1)) != 0;
				strncpy_s(d.gang, line.substr(t[10] + 1).c_str(), 63);
			}
		}

		// ---- Parse entity lines into double buffer ----
		{
			int writeIdx = 1 - config::g_entReadIdx.load(std::memory_order_acquire);
			auto& buf = config::g_entBuf[writeIdx];
			int count = 0;

			std::istringstream ss(entitySection);
			std::string line;
			while (std::getline(ss, line) && count < 512) {
				if (line.empty()) continue;

				// entIndex \t posX \t posY \t posZ \t label \t owner \t type \t money \t health \t maxHealth
				size_t t[9];
				t[0] = line.find('\t');
				if (t[0] == std::string::npos) continue;
				for (int i = 1; i < 9; ++i) {
					t[i] = line.find('\t', t[i - 1] + 1);
					if (t[i] == std::string::npos) t[i] = std::string::npos;
				}
				if (t[5] == std::string::npos) continue;

				{
					auto& e = buf[count];
					e.entIndex = SafeAtoi(line.substr(0, t[0]));
					e.pos.x = SafeAtof(line.substr(t[0] + 1, t[1] - t[0] - 1));
					e.pos.y = SafeAtof(line.substr(t[1] + 1, t[2] - t[1] - 1));
					e.pos.z = SafeAtof(line.substr(t[2] + 1, t[3] - t[2] - 1));
					strncpy_s(e.label, line.substr(t[3] + 1, t[4] - t[3] - 1).c_str(), 63);
					strncpy_s(e.owner, line.substr(t[4] + 1, t[5] - t[4] - 1).c_str(), 31);
					e.type  = SafeAtoi(line.substr(t[5] + 1, t[6] != std::string::npos ? t[6] - t[5] - 1 : std::string::npos));
					if (t[6] != std::string::npos && t[7] != std::string::npos) {
						e.money = SafeAtoi(line.substr(t[6] + 1, t[7] - t[6] - 1));
						e.health = (t[8] != std::string::npos) ? SafeAtoi(line.substr(t[7] + 1, t[8] - t[7] - 1)) : 0;
						e.maxHealth = (t[8] != std::string::npos) ? SafeAtoi(line.substr(t[8] + 1)) : 0;
					} else if (t[6] != std::string::npos) {
						e.money = SafeAtoi(line.substr(t[6] + 1));
						e.health = 0;
						e.maxHealth = 0;
					} else {
						e.money = 0;
						e.health = 0;
						e.maxHealth = 0;
					}
					e.valid = true;
					e.distance = 0.f;
				}
				++count;
			}

			// Invalidate remaining slots
			for (int i = count; i < 512; ++i)
				buf[i].valid = false;

			config::g_entCount[writeIdx] = count;
			config::g_entReadIdx.store(writeIdx, std::memory_order_release);
		}
	}

} // namespace intel_lua
